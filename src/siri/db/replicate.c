/*
 * replicate.c - Replicate SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 11-07-2016
 *
 */
#define _GNU_SOURCE
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/fifo.h>
#include <siri/db/insert.h>
#include <siri/db/replicate.h>
#include <siri/db/series.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/siri.h>
#include <stddef.h>

#define REPLICATE_SLEEP 10          // 10 milliseconds * active tasks
#define REPLICATE_TIMEOUT 300000    // 5 minutes

static void REPLICATE_work(uv_timer_t * handle);
static void REPLICATE_on_repl_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static void REPLICATE_on_repl_finished_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

/*
 * Return 0 is successful or -1 and a SIGNAL is raised if failed.
 *
 * replicate->status   : REPLICATE_IDLE
 */
int siridb_replicate_init(siridb_t * siridb, siridb_initsync_t * initsync)
{
#ifdef DEBUG
    assert (siri.loop != NULL);
#endif

    siridb->replicate =
            (siridb_replicate_t *) malloc(sizeof(siridb_replicate_t));
    if (siridb->replicate == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    siridb->replicate->initsync = initsync;

    siridb->replicate->timer = (uv_timer_t *) malloc(sizeof(uv_timer_t));
    if (siridb->replicate->timer == NULL)
    {
        ERR_ALLOC
        siridb->replicate->status = REPLICATE_CLOSED;
        siridb_replicate_free(&siridb->replicate);
        return -1;
    }
    siridb->replicate->timer->data = siridb;

    siridb->replicate->status = REPLICATE_IDLE;

    uv_timer_init(siri.loop, siridb->replicate->timer);

    return 0;
}

/*
 * This will close the replicate task.
 *
 * Warning: make sure open promises are closed since the 'REPLICATE_work'
 * depends on having siridb->replicate.
 */
void siridb_replicate_close(siridb_replicate_t * replicate)
{
#ifdef DEBUG
    assert (replicate != NULL &&
            replicate->timer != NULL &&
            replicate->status != REPLICATE_CLOSED);
#endif
    /* we can use uv_timer_stop() even if the timer is not scheduled */
    uv_timer_stop(replicate->timer);
    uv_close((uv_handle_t *) replicate->timer, (uv_close_cb) free);
    replicate->status = REPLICATE_CLOSED;
}

/*
 * This will destroy the replicate task.
 * Make sure to call 'siridb_replicate_close' first.
 *
 * Warning: make sure open promises are closed since the 'REPLICATE_work'
 * depends on having siridb->replicate.
 */
void siridb_replicate_free(siridb_replicate_t ** replicate)
{
#ifdef DEBUG
    log_debug("Free replicate");
    assert ((*replicate)->status == REPLICATE_CLOSED);
#endif
    if ((*replicate)->initsync != NULL)
    {
        siridb_initsync_free(&(*replicate)->initsync);
    }
    free(*replicate);

    *replicate = NULL;
}

/*
 * Returns 0 if successful or anything else if not.
 * (signal is set in case of an error)
 */
int siridb_replicate_pkg(siridb_t * siridb, sirinet_pkg_t * pkg)
{
    int rc = siridb_fifo_append(siridb->fifo, pkg);
    if (!rc && siridb_replicate_is_idle(siridb->replicate))
    {
        siridb_replicate_start(siridb->replicate);
    }
    return rc;
}

/*
 * Start replicate task. Only call this function when status is 'idle'.
 * Idle status can be checked using 'siridb_replicate_is_idle(replicate)'
 */
void siridb_replicate_start(siridb_replicate_t * replicate)
{
#ifdef DEBUG
    assert (siridb_replicate_is_idle(replicate));
#endif
    replicate->status = REPLICATE_RUNNING;
    if (replicate->initsync == NULL)
    {
        uv_timer_start(
                replicate->timer,
                REPLICATE_work,
                REPLICATE_SLEEP,
                0);
    }
    else
    {
        siridb_initsync_run(replicate->timer);
    }
}

/*
 * Pause the replicate process. This can take some time and one should check
 * for replicate->status to be REPLICATE_PAUSED. Do not stop the fifo buffer
 * before the replication process is truly paused.
 */
void siridb_replicate_pause(siridb_replicate_t * replicate)
{
#ifdef DEBUG
    assert (replicate->status != REPLICATE_CLOSED);
#endif
    replicate->status = (replicate->status == REPLICATE_IDLE) ?
        REPLICATE_PAUSED : REPLICATE_STOPPING;
}

/*
 * Continue the replication process.
 *
 * (this will start the replicate task)
 */
void siridb_replicate_continue(siridb_replicate_t * replicate)
{
#ifdef DEBUG
    /* make sure the fifo buffer is open */
    assert (siridb_fifo_is_open(((siridb_t *) replicate->timer->data)->fifo));
    assert (replicate->status != REPLICATE_CLOSED);
#endif

    replicate->status = (replicate->status == REPLICATE_STOPPING) ?
            REPLICATE_RUNNING : REPLICATE_IDLE;

    if (replicate->initsync != NULL)
    {
        siridb_initsync_fopen(replicate->initsync, "r+");

        if (replicate->initsync == NULL)
        {
            log_critical("Cannot open initial synchronization file: '%s'",
                    replicate->initsync->fn);
            siridb_initsync_free(&replicate->initsync);
        }
    }

    if (siridb_replicate_is_idle(replicate))
    {
        siridb_replicate_start(replicate);
    }
}



/*
 * This function can raise a SIGNAL.
 */
static void REPLICATE_work(uv_timer_t * handle)
{
    siridb_t * siridb = (siridb_t *) handle->data;
    sirinet_pkg_t * pkg;

#ifdef DEBUG
    assert (siridb->fifo != NULL);
    assert (siridb->replicate != NULL);
    assert (siridb->replica != NULL);
    assert (siridb->replicate->status != REPLICATE_IDLE);
    assert (siridb->replicate->status != REPLICATE_PAUSED);
    assert (siridb->replicate->status != REPLICATE_CLOSED);
    assert (siridb->replicate->initsync == NULL);
    assert (siridb_fifo_is_open(siridb->fifo));
#endif

    if (    siridb->replicate->status == REPLICATE_RUNNING &&
            siridb_fifo_has_data(siridb->fifo) &&
            (   siridb_server_is_accessible(siridb->replica) ||
                siridb_server_is_synchronizing(siridb->replica)) &&
            (pkg = siridb_fifo_pop(siridb->fifo)) != NULL)
    {
        if (siridb_server_send_pkg(
                siridb->replica,
                pkg,
                REPLICATE_TIMEOUT,
                REPLICATE_on_repl_response,
                siridb,
                0))
        {
            free(pkg);
        }
    }
    else
    {
        if (   siridb_server_is_synchronizing(siridb->replica) &&
                        !siridb_fifo_has_data(siridb->fifo))
        {
            pkg = sirinet_pkg_new(0, 0, BPROTO_REPL_FINISHED, NULL);
            if (pkg != NULL && siridb_server_send_pkg(
                    siridb->replica,
                    pkg,
                    0,
                    REPLICATE_on_repl_finished_response,
                    NULL,
                    0))
            {
                free(pkg);
            }
        }
        siridb->replicate->status =
                (siridb->replicate->status == REPLICATE_STOPPING) ?
                REPLICATE_PAUSED : REPLICATE_IDLE;
    }
}

/*
 * Return a pkg without series which are scheduled for initial synchronization.
 *
 * In case of an error, NULL is returned and a SIGNAL is raised.
 */
sirinet_pkg_t * siridb_replicate_pkg_filter(
        siridb_t * siridb,
        char * data,
        size_t len,
        int flags)
{
    siridb_series_t * series;
    qp_packer_t * netpacker = sirinet_packer_new(len + PKG_HEADER_SIZE);

    if (netpacker == NULL)
    {
        return NULL;  /*signal is raised */
    }

    qp_unpacker_t unpacker;
    qp_unpacker_init(&unpacker, data, len);

    qp_obj_t qp_series_name;

    qp_next(&unpacker, NULL); // map
    qp_add_type(netpacker, QP_MAP_OPEN);

    qp_next(&unpacker, &qp_series_name); // first series or end
    while (qp_is_raw_term(&qp_series_name))
    {
        series = (siridb_series_t *) ct_get(
                siridb->series,
                qp_series_name.via.raw);
        if (series == NULL || (~series->flags & SIRIDB_SERIES_INIT_REPL))
        {
            /* raw is terminated so len is included a terminator char */
            qp_add_raw(
                    netpacker,
                    qp_series_name.via.raw,
                    qp_series_name.len);
            qp_packer_extend_fu(netpacker, &unpacker);
        }
        else
        {
            qp_skip_next(&unpacker);
        }

        qp_next(&unpacker, &qp_series_name);
    }

    sirinet_pkg_t * npkg = sirinet_packer2pkg(
            netpacker,
            0,
            (flags & INSERT_FLAG_TEST) ?
                    BPROTO_INSERT_TEST_SERVER : (flags & INSERT_FLAG_TESTED) ?
                    BPROTO_INSERT_TESTED_SERVER : BPROTO_INSERT_SERVER);

    return npkg;
}

/*
 * Call-back function: sirinet_promise_cb
 */
static void REPLICATE_on_repl_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    siridb_t * siridb = (siridb_t *) promise->data;

#ifdef DEBUG
    /* open promises must be closed before siridb->replicate is destroyed */
    assert (siridb->replicate != NULL);
    assert (siridb->fifo != NULL);
#endif

    switch ((sirinet_promise_status_t) status)
    {
    case PROMISE_WRITE_ERROR:
        /*
         * Write to socket error, data is not send so we should not commit.
         */
        break;
    case PROMISE_TIMEOUT_ERROR:
        /*
         * Timeout occurred, use commit error since the data can be
         * processed by the replica, we're just not sure.
         */
    case PROMISE_CANCELLED_ERROR:
        /*
         * Promise is cancelled but most likely the data is successful
         * processed. Use siridb_fifo_commit_err() since we're not sure.
         */
    case PROMISE_PKG_TYPE_ERROR:
        /*
         * Commit with error since this package has result in an unknown
         * package type.
         */
        siridb_fifo_commit_err(siridb->fifo);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
            siridb_fifo_commit_err(siridb->fifo);
        }
        else
        {
            siridb_fifo_commit(siridb->fifo);
        }
        break;
    }

    if (siridb->replicate->status != REPLICATE_CLOSED)
    {
        uv_timer_start(
                siridb->replicate->timer,
                REPLICATE_work,
                REPLICATE_SLEEP * siridb->active_tasks,
                0);
    }
    sirinet_promise_decref(promise);
}

static void REPLICATE_on_repl_finished_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    if (status)
    {
        /* we already have a log entry so this can be a debug log */
        log_debug(
                "Error while sending replication finished to '%s' (%s)",
                promise->server->name,
                sirinet_promise_strstatus(status));
    }
    else if (pkg->tp == BPROTO_ACK_REPL_FINISHED)
    {
        log_debug("Replication finished ACK received from '%s'",
                promise->server->name);
    }
    else
    {
        log_critical("Unexpected package type received from '%s' (type: %u)",
                promise->server->name,
                pkg->tp);
    }

    /* we must free the promise */
    sirinet_promise_decref(promise);
}




