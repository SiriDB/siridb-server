/*
 * tasks.c - SiriDB Error.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 31-10-2016
 *
 */
#include <siri/db/tasks.h>
#include <timeit/timeit.h>

void siridb_tasks_init(siridb_tasks_t * tasks)
{
    tasks->active = 0;
    tasks->idle_time = 0.0f;
    timeit_start(&tasks->_timeit);
}
