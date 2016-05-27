/*
 * optimize.c - Optimize task SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * There is one and only one optimize task thread running for SiriDB. For this
 * reason we do not need to parse data but we should only take care for locks
 * while writing data.
 *
 * changes
 *  - initial version, 09-05-2016
 *
 */
#include <siri/optimize.h>
#include <siri/siri.h>
#include <logger/logger.h>
#include <siri/db/shard.h>
#include <unistd.h>

static siri_optimize_t optimize;

static void work(uv_work_t * work);
static void work_finish(uv_work_t * work, int status);
static void optimize_cb(uv_timer_t * handle);

void siri_optimize_init(siri_t * siri)
{
    /*
     * Main Thread
     */

    uint64_t timeout = siri->cfg->optimize_interval * 1000;
    siri->optimize = &optimize;
    optimize.status = SIRI_OPTIMIZE_PENDING;
    uv_timer_init(siri->loop, &optimize.timer);
    uv_timer_start(&optimize.timer, optimize_cb, timeout, timeout);
}

void siri_optimize_cancel(void)
{
    /*
     * Main Thread
     */

    /* uv_cancel will only be successful when the task is not started yet */
    optimize.status = SIRI_OPTIMIZE_CANCELLED;
    uv_cancel((uv_req_t *) &optimize.work);
}

static void work(uv_work_t * work)
{
    /*
     * Optimize Thread
     */

    siridb_list_t * current = siri.siridb_list;

    log_info("Start optimize task");

    for (; current != NULL; current = current->next)
    {
        log_debug("Start optimizing database '%s'", current->siridb->dbname);
        imap64_walk(
                current->siridb->shards,
                (imap64_cb_t) &siridb_shard_optimize,
                (void *) current->siridb);
        if (optimize.status == SIRI_OPTIMIZE_CANCELLED)
        {
            log_info("Optimize task is cancelled.");
            return;
        }
        log_debug("Finished optimizing database '%s'", current->siridb->dbname);
    }
}

static void work_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */

    if (Logger.level <= LOGGER_INFO)
    {
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);

        log_info("Finished optimize task in %d seconds with status: %d",
                end.tv_sec - optimize.start.tv_sec,
                status);
    }

    /* reset optimize status to pending if and only if the status is RUNNING */
    if (optimize.status == SIRI_OPTIMIZE_RUNNING)
        optimize.status = SIRI_OPTIMIZE_PENDING;
}

static void optimize_cb(uv_timer_t * handle)
{
    /*
     * Main Thread
     */

    if (optimize.status != SIRI_OPTIMIZE_PENDING)
    {
        log_debug("Skip optimize task because of having status: %d",
                optimize.status);
        return;
    }
    /* set status to RUNNING */
    optimize.status = SIRI_OPTIMIZE_RUNNING;

    /* set start time */
    clock_gettime(CLOCK_REALTIME, &optimize.start);

    uv_queue_work(siri.loop, &optimize.work, work, work_finish);
}


