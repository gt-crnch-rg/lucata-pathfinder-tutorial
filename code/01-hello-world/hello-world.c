#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cilk/cilk.h>

// Emu-specific headers
#include <memoryweb/memoryweb.h>
#include <memoryweb/timing.h>

static const char str[] = "Hello, world!";

replicated long * ptr;
replicated char * str_out;

int main (void)
{
     // long is the reliable word length, 64-bits.
     const long n = strlen (str) + 1;

     /* Allocating a copy on each nodelet reduces migrations in theory.
        In *this* case, the pointers stay in registers and do not trigger migration.
        But that's up to compiler register allocation... */
     mw_replicated_init ((long*)&ptr, (long)mw_malloc1dlong (n));
     mw_replicated_init ((long*)&str_out, (long)malloc (n * sizeof (char)));

     starttiming(); // For the simulator.  Start gathering stats here.
     lu_profile_perfcntr(PFC_CLEAR, "HELLO WORLD CLEARING COUNTERS");
     lu_profile_perfcntr(PFC_READ, "HELLO WORLD READING COUNTERS AFTER CLEARING");
     lu_profile_perfcntr(PFC_START, "HELLO WORLD STARTING COUNTERS");

     for (long k = 0; k < n; ++k)
          ptr[k] = (long)str[k]; // Remote writes

     for (long k = 0; k < n; ++k)
          str_out[k] = (char)ptr[k]; // Migration and remote write

     printf("%s\n", str_out);  // Migration back

     lu_profile_perfcntr(PFC_STOP, "HELLO WORLD STOPPING COUNTERS AT END");
    
     return 0;
}
