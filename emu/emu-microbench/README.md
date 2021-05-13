# Overview

Microbenchmarks to test the performance characteristics of the Emu Chick.

# Building

Build for emu hardware/simulator:
```
mkdir build-hw && cd build-hw
cmake .. \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=../cmake/emu-cxx-toolchain.cmake
make -j4
```

Build for testing on x86:
```
mkdir build-x86 && cd build-x86
cmake .. \
-DCMAKE_BUILD_TYPE=Debug
make -j4
```

# Benchmarks

## `local_stream`

### Description
Allocates three arrays (A, B, C) with 2^`log2_num_elements` on a single nodelet. Computes the sum of two vectors (C = A + B) with `num_threads` threads, and reports the average memory bandwidth.

### Usage

`./local_stream mode log2_num_elements num_threads num_trials`

### Modes

- serial - Uses a serial for loop
- cilk_for - Uses a cilk_for loop
- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- recursive_spawn - Recursively spawns threads to divide up the loop range
- library - Uses `emu_local_for` from `emu_c_utils`

## `global_stream`
Allocates three arrays (A, B, C) with 2^`log2_num_elements` using a chunked (malloc2D) array distributed across all the nodelets. Computes the sum of two vectors (C = A + B) with `num_threads` threads, and reports the average memory bandwidth.

### Usage

`./global_stream mode log2_num_elements num_threads num_trials`

### Modes

- serial - Uses a serial for loop
- cilk_for - Uses a cilk_for loop
- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- recursive_spawn - Recursively spawns threads to divide up the loop range
- recursive_remote_spawn - Recursively spawns threads to divide up the loop range, using remote spawns where possible.
- serial_remote_spawn - Remote spawns a thread on each nodelet, then divides up work as in serial_spawn
- serial_remote_spawn_shallow - Like serial_remote_spawn, but all threads are remote spawned from nodelet 0.
- library - Uses `emu_chunked_array_apply` from `emu_c_utils`.

## `global_stream_1d`
Allocates three arrays (A, B, C) with 2^`log2_num_elements` using a striped array (malloc1dlong) distributed across all the nodelets. Computes the sum of two vectors (C = A + B) with `num_threads` threads, and reports the average memory bandwidth.

### Usage

`./global_stream_1d mode log2_num_elements num_threads num_trials`

### Modes

- serial - Uses a serial for loop
- cilk_for - Uses a cilk_for loop
- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- library - Uses `emu_1d_array_apply` from `emu_c_utils`.


## `pointer_chase`

The pointer chasing benchmark is defined as follows:
1. Allocate a contiguous array of N elements. Each element consists of an 8-byte payload and an 8-byte pointer to the next element.
2. Form a linked list by connecting the elements in random order.
3. Each of P threads traverses N/P nodes in the list in parallel, summing up the payloads as it goes.

The randomization of elements in step 2 can be further controlled using the `block_size` and `sort_mode` parameters.
See the documentation for `sort_mode` below.

### Usage

```
./pointer_chase [OPTIONS]

    --log2_num_elements  Number of elements in the list
    --num_threads        Number of threads traversing the list
    --block_size         Number of elements to swap at a time
    --spawn_mode         How to spawn the threads
    --sort_mode          How to shuffle the array
    --num_trials         Number of times to run the benchmark
```

### Spawn Modes

- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- serial_remote_spawn - Remote spawns a thread on each nodelet, then divides up work as in serial_spawn

### Sort Modes

This parameter controls how the elements in the list are linked together.
In each example, the number refers to the index of the list element in the contiguous array.
Also `--log2_num_elements=4` and `--block_size=4`.

- ordered - Each element points to the next element in the array
```
0->1->2->3->4->5->6->7->8->9->10->11->12->13->14->15
```
- block_shuffle - The elements are grouped into blocks of size `block_size`,
then the order of the blocks is randomized
```
4->5->6->7--->0->1->2->3--->12->13->14->15--->8->9->10->11
```
- intra_block_shuffle - The elements are grouped into blocks of size `block_size`,
then the order of elements within each block is randomized
```
2->1->3->0--->4->5->7->6--->8->10->11->9--->14->15->13->12
```
- full_block_shuffle - The elements are grouped into blocks of size `block_size`.
The order of elements within each block is randomized and the order of each block is randomized.
```
4->5->7->6--->2->1->3->0--->14->15->13->12--->8->10->11->9
```
