#include <GraphBLAS.h>

extern void degree_pseudoinv(GrB_Vector* v, GrB_Matrix A);
extern void scale_vector(GrB_Vector v, double scl);

int pagerank(GrB_Vector *pr,
             GrB_Matrix A, GrB_Vector v,
             const double alpha, const double ctol, const int itmax)
{
  GrB_Vector v_scaled, x, xprev, accum, D_pseudoinv;
  int k = -1;

  GrB_Index N;
  GrB_Matrix_nrows(&N, A);
  if (N == 0) return 0;

  {
    GrB_Index NE;
    GrB_Matrix_nvals(&NE, A);
    if (NE == 0) return 0;
    GrB_Vector_nvals(&NE, v);
    if (NE == 0) return 0;
  }

  GrB_Vector_new(&v_scaled, GrB_FP64, N);
  GrB_Vector_new(&x, GrB_FP64, N);
  GrB_Vector_new(&xprev, GrB_FP64, N);
  GrB_Vector_new(&accum, GrB_FP64, N);
  GrB_Vector_new(&D_pseudoinv, GrB_FP64, N);

  degree_pseudoinv(&D_pseudoinv, A);
  GrB_Vector_dup(&v_scaled, v);
  scale_vector(v_scaled, 1.0 - alpha);

  for (k = 0; k < itmax; ++k) {
    GrB_assign(xprev, GrB_NULL, GrB_NULL, x, GrB_ALL, N, GrB_DESC_R);

    GrB_vxm(accum, GrB_NULL, GrB_NULL, GrB_PLUS_TIMES_SEMIRING_FP64, x, A, GrB_DESC_R);
    GrB_eWiseAdd(accum, GrB_NULL, GrB_NULL, GrB_TIMES_FP64, D_pseudoinv, accum, GrB_DESC_R);
    scale_vector(accum, alpha);

    GrB_eWiseAdd(accum, GrB_NULL, GrB_NULL, GrB_PLUS_FP64, v_scaled, accum, GrB_DESC_R);

    GrB_assign(x, GrB_NULL, GrB_NULL, accum, GrB_ALL, N, GrB_DESC_R);

    // Re-use xprev for "convergence" check
    GrB_eWiseAdd(xprev, GrB_NULL, GrB_NULL, GrB_MINUS_FP64, accum, xprev, GrB_DESC_R);
    GrB_apply(xprev, GrB_NULL, GrB_NULL, GrB_ABS_FP64, xprev, GrB_DESC_R);
    double diff = 0.0;
    GrB_reduce(&diff, GrB_NULL, GrB_MAX_MONOID_FP64, xprev, GrB_NULL);
    //fprintf(stderr, "norm1 diff %lg\n", diff);
    if (diff <= ctol) break;
  }

  double sum;
  GrB_reduce(&sum, GrB_NULL, GrB_PLUS_MONOID_FP64, x, GrB_NULL);
  if (sum != 0)
    scale_vector(x, 1.0/sum);

  GrB_Vector_dup(pr, x);

  GrB_free(&D_pseudoinv);
  GrB_free(&accum);
  GrB_free(&xprev);
  GrB_free(&x);
  GrB_free(&v_scaled);

  return k;
}
