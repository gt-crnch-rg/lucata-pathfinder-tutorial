# Lucata Tutorial Code Readme

This directory contains sample code for you to walk through and run in one of two ways. You can either 1) walk through the Python notebook for each example from its directory or 2) compile the code from the command line using the included Makefile in each directory. 

For command-line execution, note that you will need to set your environment to get access to the tool binaries and libraries. On the CRNCH testbed, we run the following script to "source" our environment:
```
[user_directory]$ . code/.env
Lucata tools are added to current path from /tools/emu/pathfinder-sw/22.09-beta

#Then you can use the Emu compiler and simulator
[user_directory]$ which emu-cc
/tools/emu/pathfinder-sw/22.09-beta/bin/emu-cc
```

## Included Examples
This directory currently contains the following example codes:
* 01    Hello-World - Demonstrates the basic syntax of a program for the Lucata architecture, how to spawn threads, and how to "spawn at" a remote node
* 01.1  Plotting    - Demonstrates the usage of emusim_profile and associated plotting tools
* 02    Saxpy       - Shows a more complex example and demonstrates some basic layout strategy 
* 03    Workflow    - Shows how to compile code for x86 and Slurm
* 04    Profiling   - Shows how to profile code with timing hooks and HW performance counters
