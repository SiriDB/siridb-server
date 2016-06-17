/*
 * heartbeat.c - Heart-beat task SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * There is one and only one heart-beat task thread running for SiriDB. For
 * this reason we do not need to parse data but we should only take care for
 * locks while writing data.
 *
 * changes
 *  - initial version, 17-06-2016
 *
 */
#include <siri/heartbeat.h>
#include <logger/logger.h>
#include <unistd.h>
#include <slist/slist.h>
#include <siri/db/server.h>

static siri_heartbeat_t heartbeat;

static void HEARTBEAT_work(uv_work_t * work);
static void HEARTBEAT_work_finish(uv_work_t * work, int status);
static void HEARTBEAT_cb(uv_timer_t * handle);


void siri_heartbeat_init(siri_t * siri)
{
    /*
     * Main Thread
     */

    uint64_t timeout = siri->cfg->heartbeat_interval * 1000;
    siri->heartbeat = &heartbeat;
    heartbeat.status = SIRI_HEARTBEAT_PENDING;
    uv_timer_init(siri->loop, &heartbeat.timer);
    uv_timer_start(&heartbeat.timer, HEARTBEAT_cb, timeout, timeout);
}

void siri_heartbeat_cancel(void)
{
    /*
     * Main Thread
     */

    /* uv_cancel will only be successful when the task is not started yet */
    heartbeat.status = SIRI_HEARTBEAT_CANCELLED;
    uv_cancel((uv_req_t *) &heartbeat.work);
}

static void HEARTBEAT_work(uv_work_t * work)
{
    /*
     * Heart-beat Thread
     */

    siridb_list_t * current = siri.siridb_list;
    slist_t * slservers;
    siridb_server_t * server;

    log_info("Start heart-beat task");

    /* TODO: copy siridb_list with a mutex */
    for (; current != NULL; current = current->next)
    {
        log_debug("Start heart-beat for database '%s'", current->siridb->dbname);

        uv_mutex_lock(&current->siridb->servers_mutex);

        slservers = llist2slist(current->siridb->servers);

        for (size_t i = 0; i < slservers->len; i++)
        {
            server = (siridb_server_t *) slservers->data[i];
            siridb_server_incref(server);
        }

        uv_mutex_unlock(&current->siridb->servers_mutex);

        usleep(10000);

        for (size_t i = 0; i < slservers->len; i++)
        {
            server = (siridb_server_t *) slservers->data[i];

            /* decrement ref for the server which was incremented earlier */
            siridb_server_decref(server);
        }

        slist_free(slservers);

        if (heartbeat.status == SIRI_HEARTBEAT_CANCELLED)
        {
            log_info("Heart-beat task is cancelled.");
            return;
        }
        log_debug("Finished heart-beat task for database '%s'", current->siridb->dbname);
    }
}

static void HEARTBEAT_work_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */

    if (Logger.level <= LOGGER_INFO)
    {
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);

        log_info("Finished heart-beat task in %d seconds with status: %d",
                end.tv_sec - heartbeat.start.tv_sec,
                status);
    }

    /* reset heart-beat status to pending if and only if the status is RUNNING */
    if (heartbeat.status == SIRI_HEARTBEAT_RUNNING)
        heartbeat.status = SIRI_HEARTBEAT_PENDING;
}

static void HEARTBEAT_cb(uv_timer_t * handle)
{
    /*
     * Main Thread
     */

    if (heartbeat.status != SIRI_HEARTBEAT_PENDING)
    {
        log_debug("Skip heart-beat task because of having status: %d",
                heartbeat.status);
        return;
    }
    /* set status to RUNNING */
    heartbeat.status = SIRI_HEARTBEAT_RUNNING;

    /* set start time */
    clock_gettime(CLOCK_REALTIME, &heartbeat.start);

    uv_queue_work(
            siri.loop,
            &heartbeat.work,
            HEARTBEAT_work,
            HEARTBEAT_work_finish);
}
