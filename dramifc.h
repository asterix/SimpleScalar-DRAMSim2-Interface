/*---------------------------------------------------------------------------

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

#ifndef DRAM_IFC_H
#define DRAM_IFC_H

#include <stdlib.h>
#include <stdio.h>
#include "DRAMSim.h"

typedef struct WriteBufferEntries_tag
{
   unsigned int size;
   int *addr;
} WriteBufferEntries;

typedef struct WriteBuffer_tag
{
   unsigned int mysize;
   unsigned int total_occupancy;
   unsigned int num_entries;
   // Note: It would be better to use an array instead of a linked list.
   WriteBufferEntries *entries;
} WriteBuffer;



class SimpleScalarDram
{     
  private:

  // Simulate a write buffer
  WriteBuffer WriteBuf;
  bool read_done;
  bool write_done;

  public:

  // This gives the ratio of CPU to DRAM cycle time
  unsigned int cycle_multiplier;
  bool write_buffer_on;


  SimpleScalarDram()
  {
    //Default constructor
  }

  SimpleScalarDram(int wbufsize, int wbufentries, float cpucycletime, char* drammodule);
  ~SimpleScalarDram();

  // These will be callbacks
  void on_read_done(unsigned id, uint64_t address, uint64_t clock_cycle);
  void on_write_done(unsigned id, uint64_t address, uint64_t clock_cycle);
  static void power_info(double a, double b, double c, double d);

  int* writebuf_get_next_addr();
  void print_write_buffer();

  // Will return latency on DRAM read operation
  unsigned int clock_dram_and_get_read_latency(int blk_size, int *addr);

  // Write should be done only to transfer enough to free up the write buffer
  unsigned int clock_dram_and_get_write_latency(int blk_size, int *addr);

  // Returns a free write buffer entry ptr when available
  WriteBufferEntries* write_buffer_get_free_entry(int blk_size);

  // Clock DRAM
  void tick_update();

};

extern SimpleScalarDram *DramSim;

// Interface to create an object
unsigned int dramifc_create_dramsim_object(int wbufsize, int wbufentries, float cpucycletime, char* drammodule);
void dramifc_destroy_dramsim_object();

unsigned int dram_read(int blk_size, int *addr);
unsigned int dram_write(int blk_size, int *addr);
void dram_clock();
bool dram_is_writeback_buffer_free(int blk_size);

#endif

