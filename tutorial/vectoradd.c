#include <stdio.h>
#include <stdlib.h>

void vectoradd(long n, long *x, long *y, long *z) {
  for (long i = 0; i < n; i++) z[i] = x[i] + y[i];
}

int main(int argc, char **argv)
{
  long size = 32; if (argc > 1) size = atol(argv[1]);
  long *x = malloc(size * sizeof(long));
  long *y = malloc(size * sizeof(long));
  long *z = malloc(size * sizeof(long));
  for (long i = 0; i < size; i++) { x[i] = i; y[i] = 1 - i; }

  vectoradd(size, x, y, z);

  long total = 0; for (long i = 0; i < size; i++) total += z[i];
  printf("%ld\n", total);
}
