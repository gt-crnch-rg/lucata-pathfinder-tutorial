#include <stdio.h>
#include <GraphBLAS.h>

int bfs(GrB_Vector *level,
        GrB_Matrix A, GrB_Index source,
        const GrB_Index max_level)
{
  GrB_Index N;
  GrB_Matrix_nrows(&N, A);

  if (N == 0) return 0;

  GrB_Vector_new(level, GrB_INT64, N);

  GrB_Vector q;
  GrB_Vector_new(&q, GrB_BOOL, N);
  GrB_Vector_setElement(q, 1, source);

  int64_t depth;
  for (depth = 1; depth <= max_level; ++depth) {
    GrB_assign(*level, q, GrB_NULL, depth, GrB_ALL, N, GrB_NULL);
    GrB_vxm(q, *level, GrB_NULL, GrB_LOR_LAND_SEMIRING_BOOL, q, A, GrB_DESC_RC);
    bool found_more;
    GrB_reduce(&found_more, GrB_NULL, GrB_LOR_MONOID_BOOL, q, GrB_NULL);
    if (!found_more) break;
  }

  GrB_free(&q);
  
  return depth;
}
