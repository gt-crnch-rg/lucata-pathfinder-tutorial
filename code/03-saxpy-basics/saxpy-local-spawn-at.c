#include <stdlib.h>
#include <cilk/cilk.h>
#include <memoryweb/memoryweb.h>
#include <memoryweb/timing.h>

void saxpy4(long n, float a, float *x, float *y)
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
  float *y = mw_localmalloc(num * size * sizeof(*y), x[0]);

  for (long j = 0; j < num; j++) {
    for (long i = 0; i < size; i++) {
      x[j][i] = j * size + i; y[j * size + i] = 0;
    }
  }


  lu_profile_perfcntr(PFC_CLEAR, "CLEAR COUNTERS");
  lu_profile_perfcntr(PFC_START, "START COUNTERS");

  for (long i = 0; i < num; i++) {
    cilk_spawn_at (x[i]) saxpy4(size, aval, x[i], &y[i * size]);
  }
  cilk_sync;

  lu_profile_perfcntr(PFC_STOP, "STOP COUNTERS");
}
