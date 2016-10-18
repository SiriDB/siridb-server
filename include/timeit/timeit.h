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
#pragma once

#include <sys/time.h>

typedef struct timeval timeit_t;

void timeit_start(timeit_t * start);
float timeit_stop(timeit_t * start);



/*
 * Usage:
 *
 *  timeit_t start;
 *  timeit_start(&start);
 *
 *  ... some code ....
 *
 *  log_debug("Time in milliseconds: %f",timeit_stop(&start));
 */
