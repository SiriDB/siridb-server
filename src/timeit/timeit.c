/*
 * timeit.c - Timeit.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2016
 *
 */
#include <timeit/timeit.h>

void timeit_start(timeit_t * start)
{
    gettimeofday(start, 0);
}

float timeit_stop(timeit_t * start)
{
   timeit_t end;

   gettimeofday(&end, 0);

   return (end.tv_sec - start->tv_sec) * 1000.0f +
           (end.tv_usec - start->tv_usec) / 1000.0f;
}

