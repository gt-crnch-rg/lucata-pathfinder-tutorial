#include <limits.h>
#ifdef X86
#include <stdio.h>
#include <emu_c_utils.h>
#else
#include <emu_c_utils/emu_c_utils.h>
#endif

#define N 256
#define T (N * NUM_NODES())

struct stats {
  long count;
  long max;
  long min;
};
replicated struct stats s;

emu_chunked_array *distr_array()
{
  emu_chunked_array *A = emu_chunked_array_replicated_new(T, sizeof(long));
  for (long j = 0; j < NUM_NODES(); j++) {
    long *P = emu_chunked_array_index(A, j * N);
    for (long i = 0; i < N; i++) P[i] = i;
  }
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

void update(emu_chunked_array *A, long begin, long end, va_list args)
{
  long *P = emu_chunked_array_index(A, begin);
  for (long i = 0; i < end - begin; ++i) {
    long score = scoreval(P[i]);
    if (score != 0) {
      ATOMIC_ADDM(&(s.count), 1);
      ATOMIC_MAXM(&(s.max), score);
      ATOMIC_MINM(&(s.min), score);
    }
  }
}

int main(int argc, char **argv)
{
  emu_chunked_array *A = distr_array();
  initialize();

  hooks_region_begin("example");
  emu_chunked_array_apply(A, GLOBAL_GRAIN_MIN(T, 64), update);

  long count = 0;
  long max = 0;
  long min = LONG_MAX;
  for (long i = 0; i < NUM_NODES(); i++) {
    struct stats *si = mw_get_nth(&s, i);
    count += si->count;
    if (si->max > max) max = si->max;
    if (si->min < min) min = si->min;
  }
  double time_ms = hooks_region_end("example");
  printf("time (ms) = %lf\n", time_ms);
  //  printf("time (ms) %lf count %ld max %ld min %ld\n", time_ms, count, max, min);
}
