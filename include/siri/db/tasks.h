/*
 * tasks.h - SiriDB Error.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 31-10-2016
 *
 */
#pragma once

#include <time.h>
#include <inttypes.h>
#include <timeit/timeit.h>

typedef struct siridb_tasks_s
{
    struct timespec _timeit;
    uint64_t active;
    double idle_time;
} siridb_tasks_t;


void siridb_tasks_init(siridb_tasks_t *tasks);

#define siridb_tasks_inc(tasks) \
if (!tasks.active++) tasks.idle_time += timeit_stop(&tasks._timeit)

#define siridb_tasks_dec(tasks) \
if (!--tasks.active) timeit_start(&tasks._timeit)
