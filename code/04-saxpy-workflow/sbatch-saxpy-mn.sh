#!/bin/bash
#==== Specify the time for the reservation (HH:MM:SS)
#SBATCH --time=0:15:00
#==== Create a useful name for output files
#SBATCH --job-name=saxpy-1d-workflow
#==== Specify the cluster to use
#SBATCH -M pathfinder
#==== Specify the QoS parameter. Valid options are single-node, single-chassis, multi-chassis
#SBATCH -q single-chassis

# Run the command on a single chassis and report the time it took
time emu_multinode_exec 0 0 -- saxpy-1d-workflow.mwx 8 2097152 5.0
