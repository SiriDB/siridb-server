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
#include <slist/slist.h>

static siri_optimize_t optimize;

static void OPTIMIZE_work(uv_work_t * work);
static void OPTIMIZE_work_finish(uv_work_t * work, int status);
static void OPTIMIZE_cb(uv_timer_t * handle);
static void OPTIMIZE_create_slist(siridb_shard_t * shard, slist_t * slist);

void siri_optimize_init(siri_t * siri)
{
    /*
     * Main Thread
     */

    uint64_t timeout = siri->cfg->optimize_interval * 1000;
    siri->optimize = &optimize;
    optimize.status = SIRI_OPTIMIZE_PENDING;
    uv_timer_init(siri->loop, &optimize.timer);
    uv_timer_start(&optimize.timer, OPTIMIZE_cb, timeout, timeout);
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

static void OPTIMIZE_work(uv_work_t * work)
{
    /*
     * Optimize Thread
     */

    siridb_list_t * current = siri.siridb_list;
    slist_t * slshards;
    siridb_shard_t * shard;

    log_info("Start optimize task");

    /* TODO: copy siridb_list with a mutex */
    for (; current != NULL; current = current->next)
    {
        log_debug("Start optimizing database '%s'", current->siridb->dbname);

        uv_mutex_lock(&current->siridb->shards_mutex);

        slshards = slist_new(current->siridb->shards->len);

        imap64_walk(
                current->siridb->shards,
                (imap64_cb_t) &OPTIMIZE_create_slist,
                (void *) slshards);

        uv_mutex_unlock(&current->siridb->shards_mutex);

        for (ssize_t i = 0; i < slshards->len; i++)
        {
            shard = (siridb_shard_t *) slshards->data[i];
            siridb_shard_optimize(shard, current->siridb);

            /* decrement ref for the shard which was incremented earlier */
            siridb_shard_decref(shard);
        }

        slist_free(slshards);

        if (optimize.status == SIRI_OPTIMIZE_CANCELLED)
        {
            log_info("Optimize task is cancelled.");
            return;
        }
        log_debug("Finished optimizing database '%s'", current->siridb->dbname);
    }
}

static void OPTIMIZE_work_finish(uv_work_t * work, int status)
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

static void OPTIMIZE_cb(uv_timer_t * handle)
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

    uv_queue_work(
            siri.loop,
            &optimize.work,
            OPTIMIZE_work,
            OPTIMIZE_work_finish);
}

static void OPTIMIZE_create_slist(siridb_shard_t * shard, slist_t * slist)
{
    siridb_shard_incref(shard);
    slist_append(slist, shard);
}

