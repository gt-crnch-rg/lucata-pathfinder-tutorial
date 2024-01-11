#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

void saxpy(long n, long a, long *x, long *y)
{
  for (long i = 0; i < n; i++)
    y[i] += a * x[i];
}

int main(int argc, char **argv)
{
  long nth = 4; long size = 32; long aval = 50;
  if (argc == 4) {
    nth = atol(argv[1]); // number threads
    size = atol(argv[2]); // array size
    aval = atol(argv[3]); // constant
  }
  long *x = mw_malloc1dlong(size);
  long *y = mw_malloc1dlong(size);
  for (long i = 0; i < size; i++) { x[i] = i; y[i] = 0; }

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  long grain = size / nth; // elts per thread
  for (long i = 0, j = 0; i < nth; i++, j += grain)
    cilk_spawn saxpy(grain, aval, &x[j], &y[j]);
  cilk_sync;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  for (long i = 0; i < size; i++) printf("%ld ", y[i]);
  printf("\n");
}

