#include <sys/time.h>
#include <stdio.h>

static struct timeval t0;
static struct timeval t1;

void timeit_start(void)
{
    gettimeofday(&t0, 0);
}

float timeit_stop(void)
{
   gettimeofday(&t1, 0);
   return (t1.tv_sec - t0.tv_sec) * 1000.0f +
           (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

