/*--------------------------------------------------------------------------

Copyright (c) 2014, Vaibhav Desai, Andrew Makouski, Haakon Aamdal,
                    Ben Stein, Sagar Yadav

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.
THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

-----------------------------------------------------------------------------
Function:  DRAMSim2 - SimpleScalar interface
Created:   20-Nov-2014
---------------------------------------------------------------------------*/

#include <vector>
#include <string>
#include <iomanip>
#include <iostream>
#include <math.h>
#include "dramifc.h"
#include "PrintMacros.h"

int SHOW_SIM_OUTPUT = false;

extern float tCK;

using namespace DRAMSim;

MultiChannelMemorySystem *dram_mem;

SimpleScalarDram *DramSim;
TransactionCompleteCB *CallbackRead;
TransactionCompleteCB *CallbackWrite;


SimpleScalarDram::SimpleScalarDram(int wbufsize, int wbufentries, float cpucycletime, char* drammodule)
{
  read_done = write_done = false;
  write_buffer_on = true;

  // Parameters are received
  dram_mem = getMemorySystemInstance(drammodule, "ini/system.ini", "", "SimpleScalarDramSim", 65536);
  dram_mem->setCPUClockSpeed(0); // Because I will clock DRAM at tCK myself

  // tCK is parsed from the DRAM module file
  // If tCK isn't an integral multiple of CPU cycle time, there will be errors in reported cycles
  if(fmod(tCK, cpucycletime) != 0.0) ERROR("== SimpleScalarDram: CPU cycle time not an integral multiple of DRAM cycle ==")

  // Calculate the multiplier
  cycle_multiplier = tCK/cpucycletime;

  std::cerr <<  "==***** DRAMSim Configuration Parameters \t\t\t*****== \n"
                "==***** Write Buffer Size = "<< wbufsize << " Bytes \t\t\t\t*****== \n"
                "==***** Write Buffer Entries = "<< wbufentries << " \t\t\t\t*****== \n"
                "==***** CPU Cycle Time = " << cpucycletime <<" ns \t\t\t\t*****== \n"
                "==***** DRAM Module @" << tCK << "ns = " << drammodule <<" *****== \n"
                "==***** CPU to DRAM cycles' ratio = " << cycle_multiplier << " \t\t\t\t*****== \n" << std::endl;

  // Write Buffer size is configured
  WriteBuf.num_entries = wbufentries;
  WriteBuf.mysize = wbufsize;
  WriteBuf.total_occupancy = 0;
  
  // A zero buffer size turns off Write Buffer. Unlimited buffer space is assumed.
  if(WriteBuf.num_entries == 0) write_buffer_on = false;

  // Allocate memory to WriteBuffer
  if(write_buffer_on) WriteBuf.entries = (WriteBufferEntries*)calloc(WriteBuf.num_entries, sizeof(WriteBufferEntries));

  CallbackRead = new Callback<SimpleScalarDram, void, unsigned, uint64_t, uint64_t>(this, &SimpleScalarDram::on_read_done);
  CallbackWrite = new Callback<SimpleScalarDram, void, unsigned, uint64_t, uint64_t>(this, &SimpleScalarDram::on_write_done);

  dram_mem->RegisterCallbacks(CallbackRead, CallbackWrite, SimpleScalarDram::power_info);
}

SimpleScalarDram::~SimpleScalarDram()
{
  if(write_buffer_on) free(WriteBuf.entries);
  delete CallbackRead;
  delete CallbackWrite;
}

void
SimpleScalarDram::on_read_done(unsigned id, uint64_t address, uint64_t clock_cycle)
{
  DEBUG("== SimpleScalarDram::on_read_done: Address: "<< (int*)address <<" ==");
  // Reading from DRAM was done
  read_done = true;
}

void
SimpleScalarDram::tick_update()
{
  // Clock once
  dram_mem->update();
}

void
SimpleScalarDram::on_write_done(unsigned id, uint64_t address, uint64_t clock_cycle)
{
  // Writing something to DRAM was done
  write_done = true;
  DEBUG("== SimpleScalarDram::on_write_done: Address: "<<  (int*)address <<" ==");

  WriteBufferEntries *free_entry = WriteBuf.entries;
  unsigned int i = 0;

  // Find the serviced entry and remove it
  while (i < WriteBuf.num_entries &&
         (uint64_t)free_entry->addr != address)
  { free_entry++; i++; }
  if (i >= WriteBuf.num_entries)
      DEBUG("== SimpleScalarDram::on_write_done: Can't find the entry that was written to DRAM ? ==");

  //Remove an entry
  if((uint64_t)free_entry->addr == address)
  {
     WriteBuf.total_occupancy -= free_entry->size;
     free_entry->size = 0;
     free_entry->addr = NULL;
     DEBUG("== SimpleScalarDram::on_write_done: Removed address: "<< (int*)address <<" Occupancy: "<< WriteBuf.total_occupancy << " ==");
  }

  //this->print_write_buffer();

  // If something left in write buffer, then
  if (WriteBuf.total_occupancy != 0)
  {
    // Schedule another write
    if(!dram_mem->addTransaction(true, (uint64_t)writebuf_get_next_addr()))
    {
      ERROR("== SimpleScalarDram::on_write_done: Can't write to DRAM :-( ==");
    }
  }

  // Well, that's all here
}

void
SimpleScalarDram::print_write_buffer()
{
   WriteBufferEntries *free_entry = WriteBuf.entries;
   unsigned int i = 0;

   while(i < WriteBuf.num_entries)
   {
     std::cerr << "== SimpleScalarDram::print_write_buffer: Entry: "<< i << " Address: "<< free_entry->addr << " Size: " << free_entry->size <<" ==" << std::endl;
     free_entry++; i++;
   }
}


// Call only when sure there is something in the WriteBuffer
int *
SimpleScalarDram::writebuf_get_next_addr()
{
    WriteBufferEntries *free_entry = WriteBuf.entries;
    unsigned int i = 0;

    while (i < WriteBuf.num_entries && free_entry->size == 0)
    {
       free_entry++; i++;
    }
    if (i >= WriteBuf.num_entries)
        ERROR("== SimpleScalarDram::writebuf_get_next_addr: Can't find an entry! ==");

    DEBUG("== SimpleScalarDram::writebuf_get_next_addr: Address: "<< free_entry->addr << " ==");
    return free_entry->addr;
}

void
SimpleScalarDram::power_info(double a, double b, double c, double d)
{
    // Use me somehow?
}

unsigned int
SimpleScalarDram::clock_dram_and_get_read_latency(int blk_size, int *addr)
{
  // Add a read transaction to DRAMSim and clock until it is done
  dram_mem->addTransaction(false, (uint64_t)addr);
  DEBUG("== SimpleScalarDram::clock_dram_and_get_read_latency: Address: "<< addr <<" ==");

  // This will be updated on callback
  read_done = false;
  unsigned int read_cycles = 0;
  do { dram_mem->update(); read_cycles++;} while (!read_done);
  DEBUG("== SimpleScalarDram::read_to_finish: Read cycles: " << read_cycles <<" ==");
  return read_cycles;
}

// Returns NULL if no free entry or if buffer is full
WriteBufferEntries*
SimpleScalarDram::write_buffer_get_free_entry(int blk_size)
{
   DEBUG("== SimpleScalarDram::write_buffer_get_free_entry: Occupancy: " << WriteBuf.total_occupancy << " ==");
   WriteBufferEntries *free_ptr = WriteBuf.entries;
   WriteBufferEntries *ret_ptr = NULL;

   // There is enough space
   if(((WriteBuf.mysize - WriteBuf.total_occupancy) >= blk_size))
   {
      unsigned int i = 0;
      while (i < WriteBuf.num_entries && free_ptr != NULL)
      {  
         // And there is a free entry
         if(free_ptr->size == 0)
         {
            ret_ptr = free_ptr;
            break;
         }
         free_ptr++; i++;
      }
   }

   return ret_ptr;
}

// Write should be done only to transfer enough to free up the write buffer
unsigned int
SimpleScalarDram::clock_dram_and_get_write_latency(int blk_size, int *addr)
{
  int cycles = 0;
  DEBUG("== SimpleScalarDram::clock_dram_and_get_write_latency: Block size: " << blk_size << " Address: " << addr << " ==");

  do
  {
    // Check write buffer
    WriteBufferEntries *free_entry = this->write_buffer_get_free_entry(blk_size);
    if (free_entry != NULL)
    {
      DEBUG("== SimpleScalarDram::clock_dram_and_get_write_latency: Found free entry ==");
      // There is still space left in write buffer, so enqueue
      // No latency here as writing will happen in the background
      free_entry->size = blk_size;
      free_entry->addr = addr;
      WriteBuf.total_occupancy += blk_size;

      // Schedule another write only if nothing else is being written
      if(dram_mem->willAcceptTransaction())
      {
        if(!dram_mem->addTransaction(true, (uint64_t)free_entry->addr))
        {
          ERROR("== SimpleScalarDram::clock_dram_and_get_write_latency: Can't write to DRAM :-( ==");
        }
        DEBUG("== SimpleScalarDram::clock_dram_and_get_write_latency: Queued transaction: Address: "<< free_entry->addr << " ==");
      }
      DEBUG("== SimpleScalarDram::clock_dram_and_get_write_latency: Write cycles: "<< cycles << " ==");
      // Returns added latencies from the "else" block too!
      return cycles;
    }
    else
    {
      // Write buffer is full so have to wait to enqueue
      // Keep clocking DRAM until it finishes and returns latency
      write_done = false;
      DEBUG("== SimpleScalarDram::clock_dram_and_get_write_latency: No free entry. Occupancy: "<< WriteBuf.total_occupancy << " ==");
      do { dram_mem->update(); cycles++;} while (!write_done);
    }
  } while (true); // This loop is exited on return
}

// These two will be the interface to SimpleScalar
unsigned int
dram_read(int blk_size, int *addr)
{
  unsigned int cycles = DramSim->clock_dram_and_get_read_latency(blk_size, addr);
  // CPU operates at cycle_multiplier times DRAM frequency
  return ((int)cycles*DramSim->cycle_multiplier);
}

unsigned int
dram_write(int blk_size, int *addr)
{
  unsigned int cycles = 0;
  DEBUG("== SimpleScalarDram::dram_write: Entered ==");

  if(DramSim->write_buffer_on)
  {
    cycles = DramSim->clock_dram_and_get_write_latency(blk_size, addr);
  }
  // CPU operates at cycle_multiplier times DRAM frequency
  return ((int)cycles*DramSim->cycle_multiplier);
}

// Returns true if an entry of the required size is available in the writeback buffer
bool
dram_is_writeback_buffer_free(int blk_size)
{
   return ((DramSim->write_buffer_get_free_entry(blk_size) != NULL)? true : false);
}

void
dram_clock()
{
  DramSim->tick_update();
}

// Create and destroy DRAMSim interface object
unsigned int
dramifc_create_dramsim_object(int size, int entries, float cycle, char* module)
{
  std::cerr << "==****************************************************== " << std::endl;
  std::cerr << "==*** DRAMSim2 is now interfaced with SimpleScalar ***== " << std::endl;
  std::cerr << "==****************************************************== " << std::endl;

  DramSim = new SimpleScalarDram(size, entries, cycle, module);
  DEBUG("== dramifc_create_dramsim_object : SimpleScalarDram created ==");

  return DramSim->cycle_multiplier;
}

void
dramifc_destroy_dramsim_object()
{
  delete DramSim;
  DEBUG("== dramifc_destroy_dramsim_object : SimpleScalarDram deleted ==");
}

