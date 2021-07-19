//saxpy-1d-workflow.c
#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>

//If x86 is specified use the x86 compatibility headers with Cilk; otherwise use the Lucata toolchain
#ifdef X86
  #include "memoryweb_x86.h"
  #include <emu_c_utils.h>
#else
  #include "memoryweb.h"
  #include <emu_c_utils/emu_c_utils.h>
#endif

void saxpy(long n, long a, long *x, long *y)
{
  for (long i = 0; i < n; i++)
    y[i] += a * x[i];
}

int main(int argc, char **argv)
{
  if(argc < 4){      
      printf("Pass at least 3 arguments!\n");
      printf("saxpy-1d-workflow <num_threads> <array_size> <constant>\n");
      exit(1);
  }  
    
  long nth = atol(argv[1]); // number threads
  long size = atol(argv[2]); // array size
  long aval = atol(argv[3]); // constant
  
  long *x = mw_malloc1dlong(size);
  long *y = mw_malloc1dlong(size);

  for (long i = 0; i < size; i++) {
    x[i] = i; y[i] = 0;
  }

  long grain = size / nth; // elts per thread

  //Timing flag to simulation only; ignored on x86 and HW
  starttiming();
  for (long i = 0, j = 0; i < nth; i++, j += grain)
    cilk_spawn saxpy(grain, aval, &x[j], &y[j]);
  cilk_sync;
    
  printf("SAXPY complete!\n");
}