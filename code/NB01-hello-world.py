# %% [markdown]
# # Hello World

# %% [markdown]
# We first need to initialize our environment to use the Lucata toolchain. This toolchain allows you to compile Cilk code with x86, the Lucata simulator, and for hardware execution. Note that this notebook should load the toolchain using the included .env file, so this is just if you wanted to compile code on the command line.
# 

# %%
!. /tools/emu/pathfinder-sw/set-lucata-env.sh
!which emu-cc

# %% [markdown]
# For this and other notebooks, we will import the following environment variables - a pointer to the user's notebook code director and a pointer to the Lucata tools

# %%
import os

#Get the path to where all code samples are
os.environ["USER_NOTEBOOK_CODE"]=os.path.dirname(os.getcwd())
os.environ["PATH"]=os.pathsep.join(["/tools/emu/pathfinder-sw/22.02/bin",os.environ["PATH"]])

!echo $USER_NOTEBOOK_CODE

# %% [markdown]
# ## Naive Hello World

# %% [markdown]
# Here is a "Hello, world" example to start showing aspects of writing for the Emu. However, your first question might be related to the use of the mw_malloc1dlong array with a distributed system.
# 
# Where does `ptr` itself live? Does computing `ptr[k]` cause a migration?
# 
# ```c
# #include <stdlib.h>
# #include <stdio.h>
# #include <string.h>
# #include <cilk.h>
# 
# // These are Emu-specific.
# #include <memoryweb.h>
# #include <timing.h>
# 
# static const char str[] = "Hello, world!";
# 
# long * ptr;
# char * str_out;
# 
# int main (void)
# {
#      // long is the reliable word length, 64-bits.
#      const long n = strlen (str) + 1;
# 
#      ptr = mw_malloc1dlong (n); // striped across the nodelets
#      str_out = malloc (n * sizeof (char))); // entirely on the first nodelet
# 
#      starttiming(); // For the simulator.  Start gathering stats here.
# 
#      for (long k = 0; k < n; ++k)
#           ptr[k] = (long)str[k]; // Remote writes
# 
#      for (long k = 0; k < n; ++k)
#           str_out[k] = (char)ptr[k]; // Migration and remote write...
# 
#      printf("%s\n", str_out);  // Migration back
# }
# ```
# 
# 

# %% [markdown]
# We'll test compiling this example to show the syntax and then move on to a more optimized example. Note that the .mwx output can be used for simulation and execution on the Pathfinder system.

# %%
%%bash
cd ${USER_NOTEBOOK_CODE}/01-hello-world
FLAGS="-I/tools/lucata/pathfinder-sw/22.02/include/memoryweb/ -L/tools/lucata/pathfinder-sw/22.02/lib -lmemoryweb"
emu-cc -o hello-world-naive.mwx $FLAGS hello-world-naive.c
ls *.mwx

# %% [markdown]
# ## Hello World
# 
# With the Lucata architecture, we often want to avoid spurious migrations by replicating data across nodes so that each node has a copy of the relevant data it needs. This improved sample in `hello-world/hello-world.c`, demonstrates the usage of the `replicated` type:
# 
# ```c
# #include <stdlib.h>
# #include <stdio.h>
# #include <string.h>
# #include <cilk.h>
# 
# // These are Emu-specific.
# #include <memoryweb.h>
# #include <timing.h>
# 
# static const char str[] = "Hello, world!";
# 
# replicated long * ptr;
# replicated char * str_out;
# 
# int main (void)
# {
#      // long is the reliable word length, 64-bits.
#      const long n = strlen (str) + 1;
# 
#      /* Allocating a copy on each nodelet reduces migrations in theory.
#         In *this* case, the pointers stay in registers and do not trigger migration.
#         But that's up to compiler register allocation... */
#      mw_replicated_init ((long*)&ptr, (long)mw_malloc1dlong (n));
#      mw_replicated_init ((long*)&str_out, (long)malloc (n * sizeof (char)));
# 
#      starttiming(); // For the simulator.  Start gathering stats here.
# 
#      for (long k = 0; k < n; ++k)
#           ptr[k] = (long)str[k]; // Remote writes
# 
#      for (long k = 0; k < n; ++k)
#           str_out[k] = (char)ptr[k]; // Migration and remote write
# 
#      printf("%s\n", str_out);  // Migration back
# }
# ```
# Let's compile and simulate this one. 

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
FLAGS="-I/tools/lucata/pathfinder-sw/22.02/include/memoryweb/ -L/tools/lucata/pathfinder-sw/22.02/lib -lmemoryweb"
emu-cc -o hello-world.mwx $FLAGS hello-world.c
ls *.mwx

# %% [markdown]
# Note that we are using the "capture_timing_queues" and "output_instruction_count" flags to generate some added data from the simulator. These generate tqd and uis files respectively.

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
emusim.x --capture_timing_queues -m 20 --total_nodes 1 --output_instruction_count -- hello-world.mwx

# %%
!ls hello-world.*

# %% [markdown]
# We now have several different output files. These are detailed in Ch. 7.6 of the Programming Guide and are as follows:
# * hello-world.mwx - Lucata executable
# * hello-world.cdc - Configuration data output file; includes system information and wall-clock time
# * hello-world.mps - Memory map output; shows memory operation types and thread enqueuing
# * hello-world.tqd - Timed activity tracing; includes live threads, thread activity counts, and requests
# * hello-world.uis - Instruction count statistics; shows the number of instructions per function in the application and number of migrations
# * hello-world.vsf - Verbose statistics information; advanced counter statistics for debugging bottlenecks
# 
# These files can be used with plotting tools to provide detailed output on the simulation of the application.

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
make_tqd_plots.py hello-world.tqd

# %%
from IPython.display import Image, display
display(Image(filename="hello-world.Live_Threads.png"))
display(Image(filename="hello-world.Thread_Activity.png"))
display(Image(filename="hello-world.MSP_Activity.png"))

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
make_map_plots.py hello-world.mps

# %%
display(Image(filename="hello-world.Thread_Enqueue_Map.png"))
display(Image(filename="hello-world.Memory_Read_Map.png"))
display(Image(filename="hello-world.Memory_Write_Map.png"))
display(Image(filename="hello-world.Atomic_Transaction_Map.png"))
display(Image(filename="hello-world.Remote_Transaction_Map.png"))

# %%
%%bash
. /tools/emu/tutorial-env.sh
make_uis_plots.py hello-world.uis

# %%
display(Image(filename="hello-world_total_instructions.png"))

# %% [markdown]
# ## Hello World Spawn Example
# 
# That example kept one thread alive and migrating between nodelets.  This one, hello-world-spawn.c, uses Cilk's thread spawning intrinsic:
# 
# ```c
# #include <stdlib.h>
# #include <stdio.h>
# #include <string.h>
# #include <cilk.h>
# 
# #include <memoryweb.h>
# #include <timing.h>
# 
# const char str[] = "Hello, world!";
# 
# static inline void copy_ptr (char *pc, const long *pl) { *pc = (char)*pl; }
# 
# replicated long * ptr;
# replicated char * str_out;
# 
# int main (void)
# {
#      long n = strlen (str) + 1;
# 
#      mw_replicated_init ((long*)&ptr, (long)mw_malloc1dlong (n));
#      mw_replicated_init ((long*)&str_out, (long)malloc (n * sizeof (char)));
# 
#      starttiming();
# 
#      for (long k = 0; k < n; ++k)
#           ptr[k] = (long)str[k]; // Remote writes
# 
#      for (long k = 0; k < n; ++k)
#           cilk_spawn copy_ptr (&str_out[k], &ptr[k]);
# 
#      printf("%s\n", str_out);  // Migration back
# }
# ```

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
emu-cc -o hello-world-spawn.mwx hello-world-spawn.c
emusim.x --capture_timing_queues --output_instruction_count -- hello-world-spawn.mwx
ls hello-world-spawn*
make_tqd_plots.py hello-world-spawn.tqd
make_map_plots.py hello-world-spawn.mps
make_uis_plots.py hello-world-spawn.uis

# %% [markdown]
# Then we can compare the output of the normal Hello World and the Spawn Hello World for the statistics that are different.

# %%
display(Image(filename="hello-world.Live_Threads.png"))
display(Image(filename="hello-world-spawn.Live_Threads.png"))
display(Image(filename="hello-world.Thread_Activity.png"))
display(Image(filename="hello-world-spawn.Thread_Activity.png"))
display(Image(filename="hello-world.MSP_Activity.png"))
display(Image(filename="hello-world-spawn.MSP_Activity.png"))
display(Image(filename="hello-world_total_instructions.png"))
display(Image(filename="hello-world-spawn_total_instructions.png"))

# %% [markdown]
# ## Advanced Implementation - Spawn At
# 
# This example just shows one additional variation of using a `cilk_spawn_at` call to spawn threads at a remote node
# 
# ```c
# #include <stdlib.h>
# #include <stdio.h>
# #include <string.h>
# #include <cilk.h>
# 
# #include <memoryweb.h>
# #include <timing.h>
# 
# static const char str[] = "Hello, world!";
# 
# static inline void copy_ptr (char *pc, const long *pl) { *pc = (char)*pl; }
# 
# replicated long * ptr;
# replicated char * str_out;
# 
# int main (void)
# {
#      long n = strlen (str) + 1;
# 
#      mw_replicated_init ((long*)&ptr, (long)mw_malloc1dlong (n));
#      mw_replicated_init ((long*)&str_out, (long)malloc (n * sizeof (char)));
# 
#      starttiming();
# 
#      for (long k = 0; k < n; ++k)
#           ptr[k] = (long)str[k]; // Remote writes
# 
#      for (long k = 0; k < n; ++k) {
#           cilk_spawn_at(&ptr[k]) copy_ptr (&str_out[k], &ptr[k]);
#      }
# 
#      printf("%s\n", str_out);  // Migration back
# }
# ```

# %%
%%bash
. /tools/emu/pathfinder-sw/set-lucata-env.sh
emu-cc -o hello-world-spawn-at.mwx hello-world-spawn-at.c
emusim.x --capture_timing_queues --output_instruction_count -- hello-world-spawn-at.mwx
ls hello-world-spawn-at*
make_map_plots.py hello-world-spawn-at.mps
make_tqd_plots.py hello-world-spawn-at.tqd
make_uis_plots.py hello-world-spawn-at.uis

# %%
display(Image(filename="hello-world.Live_Threads.png"))
display(Image(filename="hello-world-spawn-at.Live_Threads.png"))
display(Image(filename="hello-world.Thread_Activity.png"))
display(Image(filename="hello-world-spawn-at.Thread_Activity.png"))
display(Image(filename="hello-world.MSP_Activity.png"))
display(Image(filename="hello-world-spawn-at.MSP_Activity.png"))
display(Image(filename="hello-world_total_instructions.png"))
display(Image(filename="hello-world-spawn-at_total_instructions.png"))

# %% [markdown]
# Once we've finished our testing, we can then clean up some of the logfiles that we used for this example. Here we'll run `make clean` to save our figures in a separate directory and finish this demo.

# %%
!make clean


