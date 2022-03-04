#include <stdlib.h>
#include <cilk/cilk.h>
#include <memoryweb.h>

long aval;
replicated long a;

void saxpy(long n, long a, long *x, long *y) {
  for (long i = 0; i < n; i++) y[i] += a * x[i];
}

int main(int argc, char **argv)
{
  long nth = atol(argv[1]); // number threads
  long size = atol(argv[2]); // array size
  aval = atol(argv[3]); // constant
  mw_replicated_init(&a, aval);
  long *x = mw_malloc1dlong(size);
  long *y = mw_malloc1dlong(size);

  for (long i = 0; i < size; i++) {
    x[i] = i; y[i] = 0;
  }

  long grain = size / nth; // elts per thread

  for (long i = 0, j = 0; i < nth; i++, j += grain)
    cilk_spawn saxpy(grain, a, &x[j], &y[j]);
  cilk_sync;
}

