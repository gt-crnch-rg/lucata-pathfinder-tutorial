#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>
#include <memoryweb/timing.h>

int main(int argc, char **argv)
{
  long size = atol(argv[1]); // array size
  float aval = atof(argv[2]); // constant
  
  float *x = malloc(size * sizeof(*x));
  float *y = malloc(size * sizeof(*y));

  for (long i = 0; i < size; i++) {
    x[i] = i; y[i] = 0;
  }

  lu_profile_perfcntr(PFC_CLEAR, "CLEAR COUNTERS");
  lu_profile_perfcntr(PFC_START, "START COUNTERS");

  #pragma cilk grainsize = 8
  cilk_for (long i = 0; i < size; i++) {
    y[i] += aval * x[i];
  }

  lu_profile_perfcntr(PFC_STOP, "STOP COUNTERS");
}
