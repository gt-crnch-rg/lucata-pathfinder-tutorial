/* 
*   Matrix Market I/O example program
*
*   Read a real (non-complex) sparse matrix from a Matrix Market (v. 2.0) file.
*   and copies it to stdout.  This porgram does nothing useful, but
*   illustrates common usage of the Matrix Matrix I/O routines.
*   (See http://math.nist.gov/MatrixMarket for details.)
*
*   Usage:  a.out [filename] > output
*
*       
*   NOTES:
*
*   1) Matrix Market files are always 1-based, i.e. the index of the first
*      element of a matrix is (1,1), not (0,0) as in C.  ADJUST THESE
*      OFFSETS ACCORDINGLY offsets accordingly when reading and writing 
*      to files.
*
*   2) ANSI C requires one to use the "l" format modifier when reading
*      double precision floating point numbers in scanf() and
*      its variants.  For example, use "%lf", "%lg", or "%le"
*      when reading doubles, otherwise errors will occur.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <GraphBLAS.h>
#include "mmio.h"

int main(int argc, char *argv[])
{
    int ret_code;
    MM_typecode matcode;
    FILE *f;
    int M, N, nz;   
    GrB_Index i, *I, *J;
    double *val;

    if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [martix-market-filename]\n", argv[0]);
		exit(1);
	}
    else    
    { 
        if ((f = fopen(argv[1], "r")) == NULL) 
            exit(1);
    }

    if (mm_read_banner(f, &matcode) != 0)
    {
        printf("Could not process Matrix Market banner.\n");
        exit(1);
    }


    /*  This is how one can screen matrix types if their application */
    /*  only supports a subset of the Matrix Market data types.      */

    if (!mm_is_matrix(matcode) || !mm_is_sparse(matcode) )
    {
        printf("Sorry, this application does not support ");
        printf("Market Market type: [%s]\n", mm_typecode_to_str(matcode));
        exit(1);
    }

    /* find out size of sparse matrix .... */

    if ((ret_code = mm_read_mtx_crd_size(f, &M, &N, &nz)) !=0)
        exit(1);

    uint64_t sizes[3] = {M, N, nz};

    I = malloc(nz * sizeof(*I));
    J = malloc(nz * sizeof(*J));
    val = malloc(nz * sizeof(*val));

    /* NOTE: when reading in doubles, ANSI C requires the use of the "l"  */
    /*   specifier as in "%lg", "%lf", "%le", otherwise errors will occur */
    /*  (ANSI C X3.159-1989, Sec. 4.9.6.2, p. 136 lines 13-15)            */

    fprintf(stderr, "NZ %ld  %ld %ld\n", (long)nz, (long)M, (long)N);
    for (i=0; i<nz; i++)
    {
        long edge[2];
        int tmp = fscanf(f, "%ld %ld %lg\n", &edge[0], &edge[1], &val[i]);
        //assert(tmp == 3);
        if (tmp < 3) break;
        I[i] = edge[0];
        J[i] = edge[1];
        if (i < 20) fprintf(stderr, "%ld %ld %g\n", (long)I[i], (long)J[i], val[i]);
        if (I[i] <= 0) fprintf(stderr, "I[%ld] %ld 0\n", (long)i, (long)I[i]);
        if (J[i] <= 0) fprintf(stderr, "J[%ld] %ld 0\n", (long)i, (long)J[i]);
        assert(I[i] > 0);
        assert(J[i] > 0);
        I[i]--;  /* adjust from 1-based to 0-based */
        J[i]--;
        if (I[i] >= M) fprintf(stderr, "I[%ld] %ld %ld\n", (long)i, (long)I[i], (long)M);
        if (J[i] >= N) fprintf(stderr, "J[%ld] %ld %ld\n", (long)i, (long)J[i], (long)N);
        assert(I[i] < M);
        assert(J[i] < N);
    }
    sizes[2] = i;

    FILE *out = fopen("out.bin", "wb");

    fwrite(sizes, sizeof(uint64_t), 3, out);

    fwrite(I, sizeof(*I), nz, out);
    fwrite(J, sizeof(*J), nz, out);
    fwrite(val, sizeof(*val), nz, out);

    fclose(out);

    if (f !=stdin) fclose(f);
	return 0;
}

