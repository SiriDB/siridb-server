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
 *
 * Thread debugging:
 *  log_debug("getpid: %d - pthread_self: %lu",getpid(), pthread_self());
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

void siri_optimize_stop(siri_t * siri)
{
    /*
     * Main Thread
     */

    /* uv_cancel will only be successful when the task is not started yet */
    optimize.status = SIRI_OPTIMIZE_CANCELLED;
    uv_cancel((uv_req_t *) &optimize.work);

    /* stop the timer so it will not run again */
    uv_timer_stop(&optimize.timer);
    uv_close((uv_handle_t *) &optimize.timer, NULL);

    /* keep optimize bound to siri because we still want to check the status */
}

static void OPTIMIZE_work(uv_work_t * work)
{
    /*
     * Optimize Thread
     */

    slist_t * slsiridb;
    slist_t * slshards;
    siridb_t * siridb;
    siridb_shard_t * shard;

    log_info("Start optimize task");

    uv_mutex_lock(&siri.siridb_mutex);

    slsiridb = llist2slist(siri.siridb_list);
    if (slsiridb != NULL)
    {
        for (size_t i = 0; i < slsiridb->len; i++)
        {
            siridb = (siridb_t *) slsiridb->data[i];
            siridb_incref(siridb);
        }
    }

    uv_mutex_unlock(&siri.siridb_mutex);

    if (siri_err)
    {
        /* signal is set when slsiridb is NULL */
        return;
    }

    for (size_t i = 0; i < slsiridb->len; i++)
    {
        siridb = (siridb_t *) slsiridb->data[i];

#ifdef DEBUG
        log_debug("Start optimizing database '%s'", siridb->dbname);
#endif

        uv_mutex_lock(&siridb->shards_mutex);

        slshards = slist_new(siridb->shards->len);
        if (slshards != NULL)
        {
            imap64_walk(
                    siridb->shards,
                    (imap64_cb_t) &OPTIMIZE_create_slist,
                    (void *) slshards);
        }

        uv_mutex_unlock(&siridb->shards_mutex);

        if (siri_err)
        {
            /* signal is set when slshards is NULL */
            return;
        }

        sleep(1);

        for (size_t i = 0; i < slshards->len; i++)
        {
            shard = (siridb_shard_t *) slshards->data[i];

            if (    !siri_err &&
                    optimize.status != SIRI_OPTIMIZE_CANCELLED &&
                    shard->flags != SIRIDB_SHARD_OK &&
                    !(shard->flags & SIRIDB_SHARD_WILL_BE_REMOVED))
            {
                log_info("Start optimizing shard id %lu (%u)",
                        shard->id, shard->flags);
                if (siridb_shard_optimize(shard, siridb) == 0)
                {
                    log_info("Finished optimizing shard id %ld", shard->id);
                }
                else
                {
                    /* signal is raised */
                    log_critical(
                            "Optimizing shard id %ld has failed with a "
                            "critical error", shard->id);
                }
            }

            /* decrement ref for the shard which was incremented earlier */
            siridb_shard_decref(shard);
        }

        slist_free(slshards);

        if (optimize.status == SIRI_OPTIMIZE_CANCELLED)
        {
            log_info("Optimize task is cancelled.");
            break;
        }
#ifdef DEBUG
        log_debug("Finished optimizing database '%s'", siridb->dbname);
#endif
    }

    for (size_t i = 0; i < slsiridb->len; i++)
    {
        siridb = (siridb_t *) slsiridb->data[i];
        siridb_decref(siridb);
    }

    slist_free(slsiridb);
}

static void OPTIMIZE_work_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */

    if (Logger.level <= LOGGER_INFO)
    {
        log_info("Finished optimize task in %d seconds with status: %d",
                time(NULL) - optimize.start,
                status);
    }

    /* reset optimize status to pending if and only if the status is RUNNING */
    if (optimize.status == SIRI_OPTIMIZE_RUNNING)
    {
        optimize.status = SIRI_OPTIMIZE_PENDING;
    }
}

/*
 * Start the optimize task. (will start a new thread performing the work)
 */
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
    optimize.start = time(NULL);

    uv_queue_work(
            siri.loop,
            &optimize.work,
            OPTIMIZE_work,
            OPTIMIZE_work_finish);
}

/*
 * Append shard to simple list. (list must have enough space to hold the shard)
 */
static void OPTIMIZE_create_slist(siridb_shard_t * shard, slist_t * slist)
{
    siridb_shard_incref(shard);
    slist_append(slist, shard);
}

