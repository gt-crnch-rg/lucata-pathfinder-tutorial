#include <stdio.h>
#include <stdlib.h>
#ifdef X86
#include <memoryweb_x86.h>
#else
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

void vectoradd(long n, long *x, long *y, long *z) {
  for (long i = 0; i < n; i++) z[i] = x[i] + y[i];
}

int main(int argc, char **argv)
{
  long size = 32; long nth = 8; // size, number threads
  if (argc > 2) { size = atol(argv[1]); nth = atol(argv[2]); }
  long *x = mw_malloc1dlong(size);
  long *y = mw_malloc1dlong(size);
  long *z = mw_malloc1dlong(size);
  for (long i = 0; i < size; i++) { x[i] = i; y[i] = 1 - i; }

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  long grain = size / nth;
  for (long i = 0, j = 0; i < nth; i++, j += grain) {
    cilk_spawn vectoradd(grain, &x[j], &y[j], &z[j]); }
  cilk_sync;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  long total = 0; for (long i = 0; i < size; i++) total += z[i];
  printf("%ld\n", total);
}
