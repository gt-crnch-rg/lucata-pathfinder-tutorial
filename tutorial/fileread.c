#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <cilk.h>
#include <memoryweb/memoryweb.h>
#include <memoryweb/io.h>

replicated char ReplReadMode[2] = { 'r', '\0' };
replicated char ReplWriteMode[2] = { 'w', '\0' };

// convert int to string without call to strcpy
int longToString(long number, char *ptr) {
  char rev[1024]; // reversed character array
  int s = 0;
  if (number == 0) ptr[s++] = '0';
  else {
    while (number > 0) { rev[s++] = (number % 10) + '0'; number /= 10; };
    for (int i = s - 1; i >= 0; i--) { *ptr = rev[i]; ptr++; }
  } return s;
}

// make file names; basename is view 0
size_t makeFileName(int n, long nnodes, char *basename, char *filename) {
  size_t s = 0;
  while (basename[s] != '\0') s++;
  memcpy(filename, basename, s); filename[s++] = '.';
  s += longToString(n, &filename[s]); filename[s++] = 'o'; filename[s++] = 'f';
  s += longToString(nnodes, &filename[s]); filename[s] = '\0'; return s;
}

#define NRECS 10

// read a type for a node; basename is view 0, ptr, glocal are view 1
void readFile(char *basename, int n, long nnodes, void *ptr) {
  char filename[1024];
  makeFileName(n, nnodes, basename, filename);
  FILE *fp = mw_fopen(filename, ReplReadMode, ptr); assert (fp);
  assert (mw_fread(ptr, sizeof(long), NRECS, fp) == NRECS);
  assert (mw_fclose(fp) != EOF);
}

int main(int argc, char *argv[]) {
  if (argc != 3) { printf("need filename and num_nodes\n"); exit(-1); }
  long num_nodes = atol(argv[2]); assert (num_nodes <= NUM_NODES());

  // replicate the base name to avoid migration
  size_t s = strlen(argv[1]) + 1; char *bn = malloc(s); // aligned
  strcpy(bn, argv[1]); char *basename = mw_mallocrepl(s); assert(basename);
  for (int n = 0; n < num_nodes; n++) memcpy(mw_get_nth(basename, n), bn, s);

  // write files to read in parallel
  long *data = malloc(NRECS * sizeof(long));
  for (int i = 0; i < NRECS; i++) data[i] = 10;
  for (int n = 0; n < num_nodes; n++) {
    char filename[1024];
    makeFileName(n, num_nodes, argv[1], filename);
    FILE *fp = fopen(filename, "w"); assert(fp);
    assert (fwrite(data, sizeof(long), NRECS, fp) == NRECS);
    assert (fclose(fp) != EOF);
  }

  // read buffer
  long **buf = mw_malloc2d(NUM_NODES(), NRECS * sizeof(long));

#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_CLEAR, "Clear");
  lu_profile_perfcntr(PFC_START, "Start");
#endif
  volatile uint64_t start = CLOCK();
  for (int n = 0; n < num_nodes; n++) {
    cilk_spawn_at (buf[n]) readFile(basename, n, num_nodes, buf[n]);
  } cilk_sync;
  volatile uint64_t end = CLOCK();
#ifdef LUPROFILE
  lu_profile_perfcntr(PFC_STOP, "Stop");
#endif

  printf("File read: %s Nnodes %s Ticks %ld\n", argv[1], argv[2], end - start);
  long total = 0;
  for (int n = 0; n < num_nodes; n++)
    for (int i = 0; i < NRECS; i++) total += buf[n][i];
  printf("total %ld\n", total);
}
