#include <stdio.h>
#include <GraphBLAS.h>

#include <assert.h>

void
read_dumped(GrB_Matrix *A, const char *fname)
{
  GrB_Info info;
  FILE *mat_in = NULL;
  if (!fname) mat_in = stdin;
  else mat_in = fopen(fname, "r");
  if (!mat_in) { perror("File opening error"); abort(); }

  GrB_Index sizes[3];
  fread(sizes, sizeof(*sizes), 3, mat_in);

  assert(sizes[0] == sizes[1]);

  GrB_Index nrows = sizes[0];
  GrB_Index nvals = sizes[2];
  
  GrB_Index *I, *J;
  double *val;
  I = malloc(nvals*sizeof(*I));
  J = malloc(nvals*sizeof(*J));
  val = malloc(nvals*sizeof(*val));
  fread(I, sizeof(GrB_Index), nvals, mat_in);
  fread(J, sizeof(GrB_Index), nvals, mat_in);
  //fread(val, sizeof(double), nvals, mat_in);
  for (size_t k = 0; k < nvals; ++k)
    val[k] = 1.0;

  fclose(mat_in);
  
  info = GrB_Matrix_new(A, GrB_FP64, nrows, nrows);
  assert(info == GrB_SUCCESS);
  info = GrB_Matrix_build(*A, I, J, val, nvals, GrB_FIRST_FP64);
  assert(info == GrB_SUCCESS);
  free(I); free(J); free(val);
}

void dump_vtcs(const char *fname, GrB_Vector v)
{
  GrB_Index nvals;
  GrB_Vector_nvals(&nvals, v);

  GrB_Index *idx = malloc(nvals * sizeof(*idx));
  double *val = malloc(nvals * sizeof(*val));

  FILE *f = fopen(fname, "w");

  GrB_Vector_extractTuples(idx, val, &nvals, v);
  for (GrB_Index k = 0; k < nvals; ++k)
    fprintf(f, "%ld\n", (long)idx[k]);
  
  fclose(f);
  free(val);
  free(idx);
}
