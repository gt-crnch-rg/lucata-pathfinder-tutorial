#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <assert.h>

#include <GraphBLAS.h>

extern int bfs(GrB_Vector *level,
               GrB_Matrix A, GrB_Index source,
               const GrB_Index max_level);
extern int pagerank(GrB_Vector *pr,
                    GrB_Matrix A, GrB_Vector v,
                    const double alpha, const double ctol, const int itmax);

extern void read_dumped(GrB_Matrix *A, const char *fname);
extern void dump_vtcs(const char *fname, GrB_Vector v);
static void filter_pr(GrB_Vector *filtered_pr, GrB_Vector pr, double thresh);

int
main (int argc, char** argv)
{
  srand48(11 * 0xDEADBEEF);

  GrB_Info info;
  GrB_init(GrB_BLOCKING);

  GrB_Matrix A;
  read_dumped(&A, argv[1]);
  
  GrB_Index N;
  info = GrB_Matrix_nrows(&N, A);
  assert(info == GrB_SUCCESS);

  GrB_Index seed = N-1; // Kinda works for most pre-cooked graphs.

  GrB_Vector region;

  bfs(&region, A, seed, 3);

  GrB_Index region_size;
  GrB_Vector_nvals(&region_size, region);

  GrB_Index *region_vtx = malloc(region_size * sizeof(GrB_Index));
  int64_t *levels = malloc(region_size * sizeof(int64_t));
  GrB_Vector_extractTuples(region_vtx, levels, &region_size, region);

  free(levels);
  GrB_free(&region);

  GrB_Vector pr_seeds;
  GrB_Vector_new(&pr_seeds, GrB_FP64, N);

  GrB_Index n_seeds = (region_size >= 3? 3 : region_size);

  for (GrB_Index k = 0; k < n_seeds; ++k) {
    const GrB_Index i = lrand48() % region_size;
    GrB_Index vtx_of_interest = region_vtx[i];
    GrB_Vector_setElement(pr_seeds, 1.0 / n_seeds, vtx_of_interest);
    printf("Seed %ld: %ld\n", (long)k, (long)vtx_of_interest);
  }

  free(region_vtx);

  GrB_Vector pr;
  pagerank(&pr, A, pr_seeds, 0.85, 1.0e-4, 100);

  // Really, you'd filter for a statistical difference,
  // possibly against global PageRank.
  GrB_Vector filtered_pr;
  filter_pr(&filtered_pr, pr, 1.0e-3);

  // SuiteSparse GraphBLAS extension, but LGB also supports:
  GxB_print(pr, GxB_SUMMARY);
  GxB_print(filtered_pr, GxB_COMPLETE);

  dump_vtcs("out-list", filtered_pr);

  GrB_free(&filtered_pr);
  GrB_free(&pr);
  GrB_free(&pr_seeds);
  GrB_free(&A);
}

void
filter_pr(GrB_Vector *filtered_pr, GrB_Vector pr, double thresh)
{
  // Included in the v2.0 spec:
  GxB_Scalar tmp;
  GxB_Scalar_new(&tmp, GrB_FP64);
  GxB_Scalar_setElement(tmp, thresh);

  GrB_Vector_dup(filtered_pr, pr);
  GrB_Vector_clear(*filtered_pr);
  
  GxB_select(*filtered_pr, GrB_NULL, GrB_NULL, GxB_GE_THUNK, pr, tmp, GrB_NULL);
  GrB_free(&tmp);
}
