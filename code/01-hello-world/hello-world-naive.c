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

     for (long k = 0; k < n; ++k)
          ptr[k] = (long)str[k]; // Remote writes

     for (long k = 0; k < n; ++k)
          str_out[k] = (char)ptr[k]; // Migration and remote write...

     printf("%s\n", str);  // Migration back
}
