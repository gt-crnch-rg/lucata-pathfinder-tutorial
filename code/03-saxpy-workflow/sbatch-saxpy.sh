#!/bin/bash
#SBATCH --time=0:15:00
#SBATCH --job-name=saxpy-1d-workflow
#SBATCH -M pathfinder
#SBATCH -q single-node


# Run the command on the node
emu_handler_and_loader 0 0 -- saxpy-1d-workflow.mwx 8 128 5.0