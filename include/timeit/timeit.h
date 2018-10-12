/*
 * timeit.h - Timeit.
 */
#ifndef TIMEIT_H_
#define TIMEIT_H_

#include <time.h>

double timeit_get(struct timespec * start);

#define timeit_start(start) clock_gettime(CLOCK_MONOTONIC, start)

/*
 * Usage:
 *
 *  struct timespec start;
 *  timeit_start(&start);
 *
 *  ... some code ....
 *
 *  log_debug("Time in milliseconds: %f",timeit_stop(&start));
 */

#endif  /* TIMEIT_H_ */
