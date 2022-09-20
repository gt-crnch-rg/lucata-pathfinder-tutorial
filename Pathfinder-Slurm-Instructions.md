# Running Jobs on the Pathfinder with Slurm
Slurm is set up on the Pathfinder nodes to allow for soft reservation of the nodes. This means that you can run a Slurm command to reserve specific nodes and then launch your job automatically on either a single node or multi-chassis setup. 

Key notes:
* We don’t use salloc for any of the job launch commands here. 
* We don’t do any reconfiguration of the system, so you will still need an admin to reconfigure the system (for the near future) if it differs from what you need.  Our default setup will is typically c0 (single-chassis), c1(single-chassis), and c2-c3 (single nodes).
    * You can launch a `single-node`, `single-chassis`, or `multi-chassis` job if the configuration is set correctly.
* Slurm on the Pathfinder is federated with the rest of the RG testbed. This means that the main slurm controller runs on pathfinder0 and it communicates to other Slurm instances.


## Running commands with a one-line Slurm command
To run a single command on a single node you run a command that does the following:

1) Launch a Slurm command with sbatch
2) Specify the “cluster” with -M pathfinder
3) Specify the quality of service (QoS) with -q single-node
4) Runs a specific command with -wrap "<Lucata-specific command>"

```shell
sbatch -M pathfinder -q single-node --wrap "emu_handler_and_loader 0 0 -- hello-world.mwx"
```


## To run a command on multiple nodes:

```
  sbatch -M pathfinder -q single-chassis --wrap "emu_multinode_exec 0 0 -- hello-world.mwx"
```
  
You can even request a specific chassis to run on:
```
sbatch -M pathfinder -q single-chassis -w c1n[0-7] --wrap "emu_multinode_exec 0 0 -- hello-world.mwx"
```

Or a single node:
```
sbatch -M pathfinder -q single-node -w c2n3 --wrap "emu_handler_and_loader 0 0 -- hello-world.mwx"
```
  
## Batch scripts
To run a batch script you would replace the wrap command with a pointer to a sbatch script we have called sbatch-hello-world.sh.

```
#!/bin/bash
# Specify the time (15 minutes), the job name (reported by squeue), the "federated cluster", and the QoS setting (pick a single chassis with 8 nodes)
#SBATCH --time=0:15:00
#SBATCH --job-name=emu-hello-world
#SBATCH -o emu-hw-test.out
#SBATCH -M pathfinder
#SBATCH -q single-chassis
# This line is not required but can be useful for targeting different chassis
#SBATCH -w c1n[0-7]

# Run the command on the allocated node
emu_multinode_exec 0 0 -- hello-world.mwx
```

Then run the job with:
```
rg-login$ sbatch sbatch-hello-world.sh
```
  
## Checking the status of jobs on the Pathfinder
Note that you need to add -federation if you are running a Slurm command from anywhere other than pathfinder0-3 (slurm commands are not available on pathfinder1-3 ccb).
 
Sinfo will report the state of all the Pathfinder nodes. Here we’ve started a `single-chassis` job on pathfinder1 (c1) so these 8 nodes are allocated.
```
rg-login$ sinfo --federation
PARTITION         CLUSTER  AVAIL  TIMELIMIT  NODES  STATE NODELIST
rg-pathfinder*    pathfind    up 180-00:00:      8  alloc c1n[0-7]
rg-pathfinder*    pathfind    up 180-00:00:     24   idle c0n[0-7],c2n[0-7],c3n[0-7]
```

You can also use squeue to show which jobs are running 
```
$ squeue -p rg-pathfinder --federation
             JOBID PARTITION     NAME     USER ST       TIME  NODES NODELIST(REASON)
         134217786 rg-pathfi emu-hell  gburdell  R       6:36      8 c1n[0-7]
#Then to cancel this job
$ scancel 134217786
```
