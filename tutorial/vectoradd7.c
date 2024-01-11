#include <stdio.h>
#include <stdlib.h>
#ifdef X86
#include <memoryweb_x86.h>
#else
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

replicated long n;

void vectoradd(long *x, long *y, long *z) {
  for (long i = 0; i < n; i++) z[i] = x[i] + y[i];
}

int main(int argc, char **argv)
{
  long size = 8; long blk = 4; // number elements, blocksize
  if (argc > 2) { size = atol(argv[1]); blk = atol(argv[2]); }
  mw_replicated_init(&n, blk);
  long **x = (long **)mw_malloc1dlong(size);
  long **y = (long **)mw_malloc1dlong(size);
  long **z = (long **)mw_malloc1dlong(size);
  for (long i = 0; i < size; i++) {
    x[i] = mw_localmalloc(blk * sizeof(long), x[i]);
    y[i] = mw_localmalloc(blk * sizeof(long), y[i]);
    z[i] = mw_localmalloc(blk * sizeof(long), z[i]);
  }
  for (long i = 0; i < size; i++)
    for (long j = 0; j < blk; j++) { x[i][j] = i; y[i][j] = 1 - i; }

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  for (long i = 0; i < size; i++) {
    cilk_spawn_at (z[i]) vectoradd(x[i], y[i], z[i]);
  } cilk_sync;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  long total = 0; for (long i = 0; i < size; i++)
		    for (long j = 0; j < blk; j++) total += z[i][j];
  printf("%ld\n", total);
}
