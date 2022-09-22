//array-sum-hw-profiling.c
#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

#include "memoryweb.h"
#include <emu_c_utils/emu_c_utils.h>

long sum(long **array, long epn) {

  long sum = 0;
  
  for (long i=0; i<NUM_NODES(); i++)
    for (long j=0; j<epn; j++)
      sum += array[i][j];

  return sum;
}


int main(int argc, char **argv)
{
  //allocate_arrays(); // Allocate arrays using malloc2d
  //initialize_arrays(); // Initialize arrays

  long size = atol(argv[1]); // block size
  
  //allocate arrays
  long epn = size/NUM_NODES(); // elements per node
  long** A = (long **) mw_malloc2d(NUM_NODES(), epn * sizeof(long));
  long** B = (long **) mw_malloc2d(NUM_NODES(), epn * sizeof(long));
  long sumA;
  long sumB;

  // Initialize array values
  for (long i=0; i<NUM_NODES(); i++){
    for (long j=0; j<epn; j++) {
	A[i][j] = 1;
	B[i][j] = 2;
    }
  }

  starttiming();
  lu_profile_perfcntr(PFC_CLEAR, "CLEAR COUNTERS");
  lu_profile_perfcntr(PFC_READ, "READ COUNTERS AFTER CLEAR");
  lu_profile_perfcntr(PFC_START, "START COUNTERS");

  sumA = cilk_spawn sum(A, epn); // Spawn child thread for A
  sumB = sum(B, epn); // Sum values of B in parent thread
  cilk_sync; // Wait for spawned thread to complete
  printf("sumA = %ld, sumB = %ld\n", sumA, sumB);
  
  //Stop the performance counters
  lu_profile_perfcntr(PFC_STOP, "STOP COUNTERS AT END");

  return 0;
}
