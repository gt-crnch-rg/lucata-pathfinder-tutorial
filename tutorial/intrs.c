#include <stdio.h>
#include <limits.h>
#ifdef X86
#include <memoryweb_x86.h>
#else
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

#define N 256

struct stats {
  long count;
  long max;
  long min;
};
replicated struct stats s;

long ** distr_array()
{
  long **A = mw_malloc2d(NUM_NODES(), N * sizeof(long));
  for (long j = 0; j < NUM_NODES(); j++)
    for (long i = 0; i < N; i++) A[j][i] = i;
  return A;
}

void initialize()
{
  for (long i = 0; i < NUM_NODES(); i++) {
    struct stats *si = mw_get_nth(&s, i);
    si->count = 0; si->max = 0; si->min = LONG_MAX;
  }
}

long scoreval(long a)
{
  if (a < 10) return 0; else return a;
}

void update(long *A)
{
  cilk_for (long i = 0; i < N; i++) {
    long score = scoreval(A[i]);
    if (score != 0) {
      ATOMIC_ADDM(&(s.count), 1);
      ATOMIC_MAXM(&(s.max), score);
      ATOMIC_MINM(&(s.min), score);
    }
  }
}

int main(int argc, char **argv)
{
  long **A = distr_array();
  initialize();

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  volatile unsigned long start = CLOCK(); // clock start
  for (long i = 0; i < NUM_NODES(); i++) {
    cilk_spawn_at (A[i]) update(A[i]);
  }
  cilk_sync;

  long count = 0;
  long max = 0;
  long min = LONG_MAX;
  for (long i = 0; i < NUM_NODES(); i++) {
    struct stats *si = mw_get_nth(&s, i);
    count += si->count;
    if (si->max > max) max = si->max;
    if (si->min < min) min = si->min;
  }

  volatile unsigned long end = CLOCK();
#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  long total = end - start; // total cycles
  printf("cycles = %lu\n", total);
  //  printf("cycles %lu count %ld max %ld min %ld\n", total, count, max, min);
}
