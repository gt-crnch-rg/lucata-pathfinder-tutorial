#include <stdio.h>
#include <stdlib.h>
#ifdef X86
#include <memoryweb_x86.h>
#else
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#endif

long result;
long fib(long n)
{
  if (n < 2) return n;
  long a = cilk_spawn fib(n-1);
  long b = cilk_spawn fib(n-2);
  cilk_sync;
  return a + b;
}

int main(int argc, char **argv)
{
  long input = 10;
  if (argc > 1) input = atol(argv[1]);

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif

  result = fib(input);

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  printf("%ld\n", result);
}
