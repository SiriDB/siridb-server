/*
 * tasks.f - Counters for info on SiriDB tasks.
 */
#ifndef SIRIDB_TASKS_H_
#define SIRIDB_TASKS_H_

typedef struct siridb_tasks_s siridb_tasks_t;

#include <time.h>
#include <inttypes.h>
#include <timeit/timeit.h>

void siridb_tasks_init(siridb_tasks_t * tasks);

#define siridb_tasks_inc(tasks) \
if (!tasks.active++) tasks.idle_time += timeit_get(&tasks._timeit)

#define siridb_tasks_dec(tasks) \
if (!--tasks.active) timeit_start(&tasks._timeit)

struct siridb_tasks_s
{
    struct timespec _timeit;
    uint64_t active;
    double idle_time;
};

#endif  /* SIRIDB_TASKS_H_ */
