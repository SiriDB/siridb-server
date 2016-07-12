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
#include <siri/db/replicate.h>
#include <stddef.h>
#include <siri/err.h>
#include <siri/db/fifo.h>
#include <siri/net/protocol.h>
#include <siri/siri.h>
#include <assert.h>
#include <logger/logger.h>

#define REPLICATE_SLEEP 100

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
 */
int siridb_replicate_init(siridb_t * siridb)
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

    siridb->replicate->status = REPLICATE_IDLE;
    siridb->replicate->timer = (uv_timer_t *) malloc(sizeof(uv_timer_t));
    if (siridb->replicate->timer == NULL)
    {
        ERR_ALLOC
        free(siridb->replicate);
        siridb->replicate = NULL;
        return -1;
    }

    siridb->replicate->timer->data = siridb;

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
void siridb_replicate_destroy(siridb_t * siridb)
{
#ifdef DEBUG
    assert (siridb->replicate->status == REPLICATE_CLOSED);
#endif
    free(siridb->replicate);
    siridb->replicate = NULL;
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
    uv_timer_start(
            replicate->timer,
            REPLICATE_work,
            REPLICATE_SLEEP,
            0);
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

    if (siridb_replicate_is_idle(replicate))
    {
        siridb_replicate_start(replicate);
    }
}

static void REPLICATE_work(uv_timer_t * handle)
{
    LOGC("Work...1");
    siridb_t * siridb = (siridb_t *) handle->data;
    sirinet_pkg_t * pkg;
    LOGC("Work...2");

#ifdef DEBUG
    assert (siridb->fifo != NULL);
    assert (siridb->replicate != NULL);
    assert (siridb->replica != NULL);
    assert (siridb->replicate->status == REPLICATE_RUNNING ||
            siridb->replicate->status == REPLICATE_STOPPING);
    assert (siridb_fifo_is_open(siridb->fifo));
#endif

    if (    siridb->replicate->status == REPLICATE_RUNNING &&
            siridb_fifo_has_data(siridb->fifo) &&
            (   siridb_server_is_available(siridb->replica) ||
                siridb_server_is_synchronizing(siridb->replica)))
    {
        pkg = siridb_fifo_pop(siridb->fifo);
        if (pkg != NULL)
        {
            siridb_server_send_pkg(
                    siridb->replica,
                    pkg,
                    0,
                    (sirinet_promise_cb) REPLICATE_on_repl_response,
                    siridb);
            free(pkg);
        }
    }
    else
    {
        if (   siridb_server_is_synchronizing(siridb->replica) &&
                        !siridb_fifo_has_data(siridb->fifo))
        {
            pkg = sirinet_pkg_new(0, 0, BPROTO_REPL_FINISHED, NULL);
            if (pkg != NULL)
            {
                siridb_server_send_pkg(
                        siridb->replica,
                        pkg,
                        0,
                        (sirinet_promise_cb) REPLICATE_on_repl_finished_response,
                        NULL);
                free(pkg);
            }
        }
        siridb->replicate->status =
                (siridb->replicate->status == REPLICATE_STOPPING) ?
                REPLICATE_PAUSED : REPLICATE_IDLE;
    }
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
         * processed. Use siridb_fifo_error() since we're not sure.
         */
    case PROMISE_PKG_TYPE_ERROR:
        /*
         * Commit with error since this package has result in an unknown
         * package type.
         */
        siridb_fifo_error(siridb->fifo);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
            siridb_fifo_error(siridb->fifo);
        }
        else
        {
            siridb_fifo_commit(siridb->fifo);
        }
        break;
    }
    uv_timer_start(
            siridb->replicate->timer,
            REPLICATE_work,
            REPLICATE_SLEEP,
            0);

    free(promise);
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
    free(promise);
}
