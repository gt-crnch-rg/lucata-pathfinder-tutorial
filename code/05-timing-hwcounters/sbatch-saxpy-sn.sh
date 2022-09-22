#!/bin/bash
#==== Specify the time for the reservation (HH:MM:SS)
#SBATCH --time=0:15:00
#==== Create a useful name for output files
#SBATCH --job-name=saxpy-1d-workflow
#==== Specify the cluster to use
#SBATCH -M pathfinder
#==== Specify the QoS parameter. Valid options are single-node, single-chassis, multi-chassis
#SBATCH -q single-node

# Run the command on a single node and report the time it took
time emu_handler_and_loader 0 0 -- saxpy-1d-workflow.mwx 8 2097152 5.0
