#ifdef X86
#include <stdio.h>
#include <memoryweb_x86.h>
#else
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

int main(int argc, char **argv)
{
  long size = 32; float aval = 50.0;
  if (argc == 3) {
    size = atol(argv[1]); // array size
    aval = atof(argv[2]); // constant
  }
  float *x = malloc(size * sizeof(*x));
  float *y = malloc(size * sizeof(*y));
  for (long i = 0; i < size; i++) { x[i] = i; y[i] = 0; }

#pragma cilk grainsize = 8
  cilk_for (long i = 0; i < size; i++) {
    y[i] += aval * x[i];
  }

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  for (long i = 0; i < size; i++) printf("%f ", y[i]);
  printf("\n");

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif
}

