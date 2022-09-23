#include <GraphBLAS.h>

#if defined(__cilk)
#include <cilk/cilk.h>
#else
#define cilk_for for
#endif

void degree_pseudoinv(GrB_Vector *v, GrB_Matrix A)
{
  GrB_Index N;
  GrB_Matrix_nrows(&N, A);

  GrB_Vector_new(v, GrB_FP64, N);

   // Row reduce to compute the degrees.
  GrB_reduce(*v, GrB_NULL, GrB_NULL, GrB_PLUS_MONOID_FP64, A, GrB_NULL);

  // Using a SuiteSparse extension, deprecated by GrB v2.0's GrB_select.
  GxB_select(*v, GrB_NULL, GrB_NULL, GxB_NONZERO, *v, GrB_NULL, GrB_DESC_R);
  GrB_apply(*v, GrB_NULL, GrB_NULL, GrB_MINV_FP64, *v, GrB_DESC_R);
  
#if 0
  // An older way for use with v1.2.
  GrB_Index nv;
  GrB_Vector_nvals(&nv, *v);
  GrB_Index *idx = malloc(nv * sizeof(*idx));
  double *val = malloc(nv * sizeof(*val));
  
  GrB_Vector_extractTuples(idx, val, &nv, *v);
  cilk_for (GrB_Index k = 0; k < nv; ++k) {
    if (val[k] == 0.0) val[k] = 1.0;
    else val[k] = 1.0 / val[k];
  }
  GrB_Vector_clear(*v);
  GrB_Vector_build(*v, idx, val, nv, GrB_FIRST_FP64);

  free(val); free(idx);
#endif
  
#if 0
  // How it could be done using v1.3's binary apply.
  GrB_Vector dangling_mask;
  GrB_Vector_new(&dangling_mask, GrB_BOOL, N);
  GrB_apply(dangling_mask, GrB_NULL, GrB_NULL, GrB_EQ_FP64, 0.0, *v, GrB_NULL);
  GrB_apply(*v, dangling_mask, GrB_NULL, GrB_MINV_FP64, *v, GrB_DESC_RSC);
  GrB_assign(*v, dangling_mask, GrB_NULL, 1.0, GrB_ALL, N, GrB_DESC_RS);
  GrB_free(&dangling_mask);
#endif
}
