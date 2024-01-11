#include <unistd.h>
#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

typedef enum { REPL, ARG, GLOB } vselect;

replicated long **R;
long **A;

void testfun(vselect userepl, long n, long **B)
{
  for (long i = 0; i < NUM_NODES(); i++) {
    long **L;
    switch (userepl) {
    case REPL: L = R; break;
    case ARG: L = B; break;
    case GLOB: L = A; break;
    default: L = R;
    }
    long *N = L[i];
    for (long j = 0; j < n; j++) ATOMIC_ADDM(&N[j], 100);
  }
}

int main(int argc, char **argv)
{
  // set defaults
  vselect userepl = 0;
  long usespawn = 0;
  long asize = 10;
  long dprint = 0;

  // get options
  int c;
  while ((c = getopt(argc, argv, "pur:n:")) != -1) {
    switch (c) {
    case 'p': dprint = 1; break;
    case 'u': usespawn = 1; break;
    case 'r': userepl = atol(optarg); break;
    case 'n': asize = atol(optarg); break;
    }
  }

  // allocate and initialize
  long **B = mw_malloc2d(NUM_NODES(), asize * sizeof(long));
  A = B;
  mw_replicated_init((long *)&R, (long)B);
  for (long i = 0; i < NUM_NODES(); i++)
    for (long j = 0; j < asize; j++) R[i][j] = i * asize + j;

  // start timing
#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif
  MIGRATE(R[0]);
  volatile unsigned long starttime = CLOCK();

  // test code
  if (usespawn) {
    cilk_spawn testfun(userepl, asize, B);
    cilk_sync;
  } else testfun(userepl, asize, B);

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
      for (long j = 0; j < asize; j++) printf("%ld ", A[i][j]);
    printf("\n");
  }
  return 0;
}
