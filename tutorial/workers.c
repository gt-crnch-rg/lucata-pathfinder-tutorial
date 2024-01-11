#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

replicated long n;
replicated long **R;
replicated long nodecounter;

void worker_thread(long node, long *localcounter, long tindex)
{
  while (tindex < n) {
    R[node][tindex] += 10;
    tindex = ATOMIC_ADDMS(localcounter, 1);
  }    
}

void recursive_spawn(long low, long high, long nthreads)
{
  // recursively spawn a threadlet at each node
  for (;;) {
    long count = high - low;
    if (count <= 1) break;
    long mid = low + count / 2;
    long *sptr = mw_get_nth(&n, mid);
    cilk_spawn_at (sptr) recursive_spawn(mid, high, nthreads);
    high = mid;
  }

  long *local = mw_get_nth(&nodecounter, low);
  for (int i = 0; i < nthreads; i++) cilk_spawn worker_thread(low, local, i);
}

int main(int argc, char **argv)
{
  // set defaults
  long size = 16; long nth = 4;
  if (argc == 3) { size = atol(argv[1]); nth = atol(argv[2]); }

  // allocate and initialize
  long **B = mw_malloc2d(NUM_NODES(), size * sizeof(long));
  mw_replicated_init((long *)&R, (long)B);
  mw_replicated_init(&n, size);
  mw_replicated_init(&nodecounter, nth);
  for (long i = 0; i < NUM_NODES(); i++)
    for (long j = 0; j < n; j++) R[i][j] = 10;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  // start timing
  MIGRATE(R[0]);
  volatile unsigned long starttime = CLOCK();

  recursive_spawn(0, NUM_NODES(), nth);

  // end timing
  MIGRATE(R[0]);
  volatile unsigned long endtime = CLOCK();
#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif
  unsigned long totaltime = endtime - starttime;
  double clockrate = 175.0;
  double ms = ((double) totaltime / clockrate) / 1000.0;
  printf("Clock %.1lf Total %lu Time(ms) %.1lf\n", clockrate, totaltime, ms);
  fflush(stdout);
  for (long i = 0; i < NUM_NODES(); i++) {
    for (long j = 0; j < size; j++) printf("%ld ", R[i][j]);
    printf("\n");
  }
  return 0;
}
