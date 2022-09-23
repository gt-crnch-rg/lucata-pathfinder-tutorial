#include <GraphBLAS.h>

void scale_vector(GrB_Vector v, double scl)
{
  GrB_Index N;
  GrB_Vector_size(&N, v);
  GrB_Vector tmp;
  GrB_Vector_dup(&tmp, v);

  GrB_assign(tmp, v, GrB_NULL, scl, GrB_ALL, N, GrB_DESC_RS);
  GrB_eWiseAdd(v, GrB_NULL, GrB_NULL, GrB_TIMES_FP64, v, tmp, GrB_DESC_R);
  GrB_free(&tmp);

  // Similar note as above...
}
