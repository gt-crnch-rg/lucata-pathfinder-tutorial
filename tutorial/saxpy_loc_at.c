#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

void saxpy(long n, float a, float *x, float *y)
{
  for (long i = 0; i < n; i++)
    y[i] += a * x[i];
}
int main(int argc, char **argv)
{
  long num = 4; long size = 32; float aval = 50.0;
  if (argc == 4) {
    num = atol(argv[1]); // number blocks
    size = atol(argv[2]); // block size
    aval = atof(argv[3]); // constant
  }
  float **x = mw_malloc2d(num, size * sizeof(*x));
  float *y = mw_localmalloc(num * size * sizeof(*y), x[0]);
  for (long j = 0; j < num; j++)
    for (long i = 0; i < size; i++) {
       x[j][i] = j * size + i; y[j * size + i] = 0;
    }

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  for (long i = 0; i < num; i++) {
    cilk_spawn_at (x[i]) saxpy(size, aval, x[i], &y[i*size]);
  }
  cilk_sync;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  for (long j = 0; j < num; j++)
    for (long i = 0; i < size; i++) printf("%f ", y[j*size+i]);
  printf("\n");
}
