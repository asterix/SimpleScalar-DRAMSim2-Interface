# DRAMSim2 - SimpleScalar interface

The interface files here allow you to connect SimpleScalar, a MIPS processor simulator to DRAMSim2, an accurate DRAM simulator.
The idea is to allow for full system modeling with DRAM read/write delays taken into account.

## Compiling the interface
This is a two step approach.

###Step 1:

To integrate the SimpleScalar interface with DRAMSim2, simply copy the two files ```dramifc.cpp and dramifc.h``` into DRAMSim2 main directory. This is the directory that contains the ```Makefile```.

Compile simply by typing ```make libdramsim.a/.so or make DEBUG=1 libdramsim.a/.so``` in this directory and the interface gets built into the linkable library (static or dynamic).

###Step 2:
Now that we have the linkable library with the interface in it, we simply need to link this to SimpleScalar during executable build. However, you'll first need to make the necessary changes to SimpleScalar to use the library effectively. This involves placing the startup, tick update calls and DRAM access (read and write) during regular access and cache misses in SimpleScalar. This should by itself be pretty straightforward.

For example:

You would add the below code at the end of ```sim-outorder.c```
```
     /* go to next cycle */
      sim_cycle++;
 
      /* Clock DRAM */
      if(DRAMSim_On)
      {
        /* CPU clock runs three times faster than DRAM clock */
        if(sim_cycle % 3 == 0) dram_clock();
      }

      /* finish early? */
      if (max_insts && sim_num_insn >= max_insts)
      {
        dramifc_destroy_dramsim_object();
	      return;
      }
    }

    dramifc_destroy_dramsim_object();
```
Once done, include the ```dramsim.a or dramsim.so``` to SimpleScalar and you should be all set to use DRAMSim2 with it.


# Copyrights

SimpleScalar is a property of SimpleScalar LLC and is used here for academic purposes only.
