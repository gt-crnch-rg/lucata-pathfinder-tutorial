#include <unistd.h>
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

void testfun(long nid)
{
  long lim = n;
  long *A = R[nid];
  for (long j = 0; j < lim; j++) ATOMIC_ADDM(&A[j], 100);
}

void linear_spawn()
{
  for (long i = 0; i < NUM_NODES(); i++) {
    //    long *sptr = R[i];
    long **sptr = &R[i];
    cilk_spawn_at(sptr) testfun(i);
  }
}

void recursive_spawn(long low, long high)
{
  // recursively spawn a threadlet at each nodelet
  for (;;) {
    long count = high - low;
    if (count <= 1) break;
    long mid = low + count / 2;
    //    long *sptr = R[i];
    long **sptr = &R[mid];
    cilk_spawn_at(sptr) recursive_spawn(mid, high);
    high = mid;
  }

  // in each nodelet, call function or spawn multiple threads to do work
  testfun(low);
} // end recursive_spawn_function

int main(int argc, char **argv)
{
  // set defaults
  long recurse = 0;
  long asize = 10;
  long dprint = 0;

  // get options
  int c;
  while ((c = getopt(argc, argv, "prn:")) != -1) {
    switch (c) {
    case 'p': dprint = 1; break;
    case 'r': recurse = 1; break;
    case 'n': asize = atol(optarg); break;
    }
  }

  // allocate and initialize
  long **B = mw_malloc2d(NUM_NODES(), asize * sizeof(long));
  mw_replicated_init((long *)&R, (long)B);
  mw_replicated_init(&n, asize);
  for (long i = 0; i < NUM_NODES(); i++)
    for (long j = 0; j < n; j++) R[i][j] = i * n + j;

  // start timing
#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  MIGRATE(R[0]);
  volatile unsigned long starttime = CLOCK();

  if (recurse) recursive_spawn(0, NUM_NODES());
  else linear_spawn();
  cilk_sync;

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
  if (dprint > 0) {
    for (long i = 0; i < NUM_NODES(); i++)
      for (long j = 0; j < asize; j++) printf("%ld ", R[i][j]);
    printf("\n");
  }
  return 0;
}
