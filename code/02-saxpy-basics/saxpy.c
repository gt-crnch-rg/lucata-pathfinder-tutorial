#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

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
  for (long i = 0, j = 0; i < nth; i++, j += grain)
    cilk_spawn saxpy(grain, aval, &x[j], &y[j]);
  cilk_sync;
}
