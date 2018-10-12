/*
 * timeit.c - Timeit.
 */
#include <timeit/timeit.h>
#include <time.h>

/*
 * Returns time past in seconds since given start time.
 */
double timeit_get(struct timespec * start)
{
    struct timespec end;

    clock_gettime(CLOCK_MONOTONIC, &end);

    return (end.tv_sec - start->tv_sec) +
            (end.tv_nsec - start->tv_nsec) / 1000000000.0f;
}

