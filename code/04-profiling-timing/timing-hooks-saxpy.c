//timing-hooks-saxpy.c
#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

#include "memoryweb.h"
#include <emu_c_utils/emu_c_utils.h>

void saxpy(long n, float a, float *x, float *y)
{
  for (long i = 0; i < n; i++)
    y[i] += a * x[i];
}

int main(int argc, char **argv)
{
  long num = atol(argv[1]); // number blocks
  long size = atol(argv[2]); // block size
  float aval = atof(argv[3]); // constant
  float **x = mw_malloc2d(num, size * sizeof(*x));
  float **y = mw_malloc2d(num, size * sizeof(*y));

  for (long j = 0; j < num; j++) {
    for (long i = 0; i < size; i++) {
      x[j][i] = j * size + i; y[j][i] = 0;
    }
  }
  
  hooks_region_begin("example");
  for (long i = 0; i < num; i++) {
    cilk_spawn_at (y[i]) saxpy(size, aval, x[i], y[i]);
  }
  cilk_sync;
    
  double time_ms = hooks_region_end();
  printf("time (ms) = %lf\n", time_ms);
}