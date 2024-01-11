#include <unistd.h>
#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

typedef enum { LINE, TREE } vselect;

replicated long n;
replicated long **R;

long linear_spawn(long node)
{
  // if not at second to last node, spawn in next node
  long nextvals = 0;
  if (node < NUM_NODES() - 1) {
    long next = node + 1; long *p = mw_get_nth(&n, next);
    cilk_migrate_hint (p);
    nextvals = linear_spawn(next);
  }

  // reduce local portion of array
  long tmp = 0;
  for (long i = 0; i < n; i++) tmp += R[node][i];
  cilk_sync; return tmp + nextvals;
}

long recursive_spawn(long low, long high)
{
  // recursively spawn a threadlet at each nodelet
  long nextvals = 0;
  for (;;) {
    long count = high - low;
    if (count <= 1) break;
    long mid = low + count / 2;
    long *sptr = mw_get_nth(&n, mid);
    cilk_migrate_hint (sptr);
    nextvals += recursive_spawn(mid, high);
    high = mid;
  }

  // reduce local portion of array
  long tmp = 0;
  for (long i = 0; i < n; i++) tmp += R[low][i];
  cilk_sync; return tmp + nextvals;
}

int main(int argc, char **argv)
{
  // set defaults
  vselect which = LINE; long dprint = 0; long asize = 10;

  // get options
  int c;
  while ((c = getopt(argc, argv, "pw:")) != -1) {
    switch (c) {
    case 'p': dprint = 1; break;
    case 'w': which = atol(optarg); break;
    }
  }

  // allocate and initialize
  long **B = mw_malloc2d(NUM_NODES(), asize * sizeof(long));
  mw_replicated_init((long *)&R, (long)B);
  mw_replicated_init(&n, asize);
  for (long i = 0; i < NUM_NODES(); i++)
    for (long j = 0; j < n; j++) R[i][j] = 1;

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  // start timing
  MIGRATE(R[0]);
  volatile unsigned long starttime = CLOCK();

  long total;
  switch (which) {
  case LINE: total = linear_spawn(0); break;
  case TREE: total = recursive_spawn(0, NUM_NODES()); break;
  }

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
  printf("Total %ld\n", total);
  return 0;
}
