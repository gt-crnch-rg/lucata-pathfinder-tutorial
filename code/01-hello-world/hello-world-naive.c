#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cilk/cilk.h>

// Emu-specific headers
#include <memoryweb/memoryweb.h>

static const char str[] = "Hello, world!";

long * ptr;
char * str_out;

int main (void)
{
     // long is the reliable word length, 64-bits.
     const long n = strlen (str) + 1;

     ptr = mw_malloc1dlong (n); // striped across the nodelets
     str_out = (char *)malloc (n * sizeof (char)); // entirely on the first nodelet

     // starttiming(); Deprecated
     lu_profile_perfcntr(PFC_CLEAR, "HELLO WORLD NAIVE CLEARING COUNTERS");
     lu_profile_perfcntr(PFC_READ, "HELLO WORLD NAIVE READING COUNTERS AFTER CLEARING");
     lu_profile_perfcntr(PFC_START, "HELLO WORLD NAIVE STARTING COUNTERS");
    
     for (long k = 0; k < n; ++k)
          ptr[k] = (long)str[k]; // Remote writes

     for (long k = 0; k < n; ++k)
          str_out[k] = (char)ptr[k]; // Migration and remote write...

     printf("%s\n", str);  // Migration back

     lu_profile_perfcntr(PFC_STOP, "HELLO WORLD NAIVE STOPPING COUNTERS AT END");
    
     return 0;
}
