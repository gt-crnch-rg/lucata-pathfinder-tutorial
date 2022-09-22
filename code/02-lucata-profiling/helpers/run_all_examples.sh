#!/bin/bash

#This script shows how to run all the examples from the command line. You can run all the commands here or copy them one at a time to run in a terminal.

#Memory size with 2^N Bytes
MEMSZ=21
#Number of nodes in a simulated system
NODES=1

#### Example 1
printf "\n======================================\n"
printf "Running the naive Hello World example with memory size $MEMSZ and $NODES nodes\n"
printf "======================================\n"

#We print out the command with xtrace then run it
set -x
emusim.x --untimed -m $MEMSZ --total_nodes $NODES -- hello-world-naive.mwx
set +x

#### Example 2
printf "\n======================================\n"
printf "Running the basic Hello World example with timing with memory size $MEMSZ and $NODES nodes\n"
printf "======================================\n"

set -x
emusim.x --capture_timing_queues -m $MEMSZ --total_nodes $NODES --output_instruction_count -- hello-world.mwx
set +x
printf "\n======================================\n"

#### Example 3
printf "\n======================================\n"
printf "Running the basic Hello World spawning example with timing with memory size $MEMSZ and $NODES nodes\n"
printf "======================================\n"

set -x
emusim.x --capture_timing_queues -m $MEMSZ --total_nodes $NODES --output_instruction_count -- hello-world-spawn.mwx
set +x
printf "\n======================================\n"

#### Example 4
printf "\n======================================\n"
printf "Running the basic Hello World spawning example with timing with memory size $MEMSZ and $NODES nodes\n"
printf "======================================\n"

set -x
emusim.x --capture_timing_queues -m $MEMSZ --total_nodes $NODES --output_instruction_count -- hello-world-spawn-at.mwx
set +x
printf "\n======================================\n"


