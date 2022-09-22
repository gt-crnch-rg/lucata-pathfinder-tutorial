#!/bin/bash
#SBATCH --time=0:15:00
#SBATCH --job-name=emu-hello-world
#SBATCH -M pathfinder
#SBATCH -q single-chassis
#SBATCH -w c1n[0-7]

# Run the command on the node
emu_multinode_exec 0 0 -- hello-world.mwx
