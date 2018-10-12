/*
 * tasks.c - Counters for info on SiriDB tasks.
 */
#include <siri/db/tasks.h>
#include <timeit/timeit.h>

void siridb_tasks_init(siridb_tasks_t * tasks)
{
    tasks->active = 0;
    tasks->idle_time = 0.0f;
    timeit_start(&tasks->_timeit);
}
