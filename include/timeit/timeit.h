/*
 * timeit.h - Timeit.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2016
 *
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
