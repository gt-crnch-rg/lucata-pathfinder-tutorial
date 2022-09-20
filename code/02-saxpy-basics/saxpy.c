#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>
#include <memoryweb/timing.h>

void saxpy(long n, float a, float *x, float *y)
{
  for (long i = 0; i < n; i++)
    y[i] += a * x[i];
}

int main(int argc, char **argv)
{
  long nth = atol(argv[1]); // number threads
  long size = atol(argv[2]); // array size
  float aval = atof(argv[3]); // constant
  float *x = malloc(size * sizeof(*x));
  float *y = malloc(size * sizeof(*y));
  for (long i = 0; i < size; i++) {
    x[i] = i; y[i] = 0;
  }
  long grain = size / nth; // elts per thread

  lu_profile_perfcntr(PFC_CLEAR, "CLEAR COUNTERS");
  lu_profile_perfcntr(PFC_START, "START COUNTERS");

  for (long i = 0, j = 0; i < nth; i++, j += grain)
    cilk_spawn saxpy(grain, aval, &x[j], &y[j]);
  cilk_sync;

  lu_profile_perfcntr(PFC_STOP, "STOP COUNTERS");
}
