#define RECURSIVE_CILK_SPAWN(BEGIN, END, GRAIN, FUNC, ...)          \
do {                                                                \
    long low = BEGIN;                                               \
    long high = END;                                                \
                                                                    \
    for (;;) {                                                      \
        /* How many elements in my range? */                        \
        long count = high - low;                                    \
                                                                    \
        /* Break out when my range is smaller than the grain size */\
        if (count <= GRAIN) break;                                  \
                                                                    \
        /* Divide the range in half */                              \
        /* Invariant: count >= 2 */                                 \
        long mid = low + count / 2;                                 \
                                                                    \
        /* Spawn a thread to deal with the lower half */            \
        cilk_spawn FUNC(low, mid, GRAIN, __VA_ARGS__);              \
                                                                    \
        low = mid;                                                  \
    }                                                               \
                                                                    \
    /* Recursive base case: call worker function */                 \
    FUNC ## _worker(low, high, __VA_ARGS__);                        \
} while (0)
