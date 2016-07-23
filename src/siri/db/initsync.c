/*
 * initsync.c - Initial replica synchronization
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 22-07-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/initsync.h>
#include <siri/err.h>
#include <stddef.h>
#include <siri/db/series.h>
#include <assert.h>
#include <unistd.h>
#include <siri/net/protocol.h>
#include <stdio.h>
#include <siri/siri.h>
#include <siri/optimize.h>

#define INITSYNC_SLEEP 100          // 100 milliseconds
#define INITSYNC_TIMEOUT 60000      // 1 minute
#define INITSYC_FN ".initsync"

void siridb_initsync_fopen(siridb_initsync_t * initsync, const char * opentype);
static int INITSYNC_create_cb(siridb_series_t * series, FILE * fp);
static void INITSYNC_work(uv_timer_t * timer);
static void INITSYNC_next_series_id(siridb_t * siridb);
static int INITSYNC_unlink(siridb_initsync_t * initsync);
inline static int INITSYNC_fn(siridb_t * siridb, siridb_initsync_t * initsync);
static void INITSYNC_pause(siridb_replicate_t * replicate);
static void INITSYNC_on_insert_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

static const long int SIZE2 = 2 * sizeof(uint32_t);

/*
 * Returns a pointer to initsync. If 'create_new' is zero and an initial
 * synchronization file cannot be found, NULL is returned.
 *
 * In case of an error we also return NULL but in this case a SIGNAL is raised
 */
siridb_initsync_t * siridb_initsync_open(siridb_t * siridb, int create_new)
{
    siridb_initsync_t * initsync =
            (siridb_initsync_t *) malloc(sizeof(siridb_initsync_t));
    if (initsync == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        initsync->fn = NULL;
        initsync->fp = NULL;
        initsync->next_series_id = NULL;
        initsync->series = NULL;

        if (INITSYNC_fn(siridb, initsync) < 0)
        {
            ERR_ALLOC
            siridb_initsync_free(&initsync);
        }
        else
        {
            siridb_initsync_fopen(initsync, create_new ? "w+" : "r+");
            if (initsync->fp == NULL)
            {
                siridb_initsync_free(&initsync);
            }
            else
            {
                initsync->next_series_id =
                        (uint32_t *) malloc(sizeof(uint32_t));
                if (initsync->next_series_id == NULL)
                {
                    ERR_ALLOC
                    siridb_initsync_free(&initsync);
                }
                else if (create_new)
                {
                    if (imap32_walk(
                                siridb->series_map,
                                (imap32_cb) INITSYNC_create_cb,
                                initsync->fp) || fflush(initsync->fp))
                    {
                        ERR_FILE
                        siridb_initsync_free(&initsync);
                    }
                }
                else
                {
                    siridb_series_t * series;
                    uint32_t series_id;
                    while (fread(
                            &series_id,
                            sizeof(uint32_t),
                            1,
                            initsync->fp) == 1)
                    {
                        series = imap32_get(
                                siridb->series_map,
                                series_id);

                        if (series != NULL)
                        {
                            series->flags |= SIRIDB_SERIES_INIT_REPL;
                        }
                    }
                }
                if (initsync != NULL)
                {
                    initsync->size = ftell(initsync->fp);
                    if (initsync->size == -1)
                    {
                        ERR_FILE
                        siridb_initsync_free(&initsync);
                    }
                    else if (initsync->size < sizeof(uint32_t))
                    {
                        log_warning("Empty synchronization file found...");
                        INITSYNC_unlink(initsync);
                        siridb_initsync_free(&initsync);
                    }
                    else
                    {
                        if (fseek(initsync->fp, -sizeof(uint32_t), SEEK_END) ||
                            fread(  initsync->next_series_id,
                                    sizeof(uint32_t),
                                    1,
                                    initsync->fp) != 1)
                        {
                            ERR_FILE
                            siridb_initsync_free(&initsync);
                        }
                        else
                        {
                            siri_optimize_pause(siri.optimize);
                        }
                    }
                }
            }
        }
    }
    return initsync;
}

/*
 * Destroy *initsync and set initsync to NULL. Parsing *initsync == NULL is
 * not allowed.
 *
 * (a SIGNAL is raised in case the file cannot be closed)
 */
void siridb_initsync_free(siridb_initsync_t ** initsync)
{
    if ((*initsync)->fp != NULL && fclose((*initsync)->fp))
    {
        ERR_FILE
    }
    free((*initsync)->fn);
    free((*initsync)->next_series_id);
    free(*initsync);
    *initsync = NULL;
}

void siridb_initsync_run(uv_timer_t * timer)
{
    uv_timer_start(timer, INITSYNC_work, INITSYNC_SLEEP, 0);
}

/*
 * Read the next series id and truncate the synchronization file to remove
 * the last synchronization id.
 *
 * This function might destroy 'replicate->initsync' when initial
 * synchronization is finished.
 *
 * In case the 'current' series is set, remove the flag and decrement the
 * series reference counter.
 */
static void INITSYNC_next_series_id(siridb_t * siridb)
{
#ifdef DEBUG
    assert (siridb->replicate != NULL);
    assert (siridb->replicate->status == REPLICATE_RUNNING ||
            siridb->replicate->status == REPLICATE_STOPPING);
#endif

    siridb_initsync_t * initsync = siridb->replicate->initsync;

#ifdef DEUBUG
    assert (initsync->size == fseek(initsync->fp, 0, SEEK_END);
#endif
    if (initsync->series != NULL)
    {
        initsync->series->flags &= ~SIRIDB_SERIES_INIT_REPL;
        siridb_series_decref(siridb->replicate->initsync->series);
    }

    if (initsync->size >= SIZE2)
    {
        initsync->size -= sizeof(uint32_t);
        if (fseek(initsync->fp, -SIZE2, SEEK_END) ||
            fread(  initsync->next_series_id,
                    sizeof(uint32_t),
                    1,
                    initsync->fp) != 1 ||
            ftruncate(initsync->fd, initsync->size))
        {
            ERR_FILE
            log_critical(
                    "Reading next series id has failed "
                    "(replicate status: %d)",
                    siridb->replicate->status);
        }
        else if (siridb->replicate->status == REPLICATE_STOPPING)
        {
            INITSYNC_pause(siridb->replicate);
        }
        else
        {
            siridb_initsync_run(siridb->replicate->timer);
        }
    }
    else
    {
        log_info("Finished initial replica synchronization");
        INITSYNC_unlink(initsync);
        siridb_initsync_free(&siridb->replicate->initsync);

        siridb->replicate->status =
                (siridb->replicate->status == REPLICATE_STOPPING) ?
                REPLICATE_PAUSED : REPLICATE_IDLE;
        siridb_replicate_start(siridb->replicate);
        siri_optimize_continue(siri.optimize);
    }
}

/*
 * Close the initial synchronization file and set status to REPLICATE_PAUSED.
 * (should only be called when status is REPLICATE_STOPPING)
 */
static void INITSYNC_pause(siridb_replicate_t * replicate)
{
#ifdef DEBUG
    assert (replicate->status == REPLICATE_STOPPING);
#endif
    if (fclose(replicate->initsync->fp))
    {
        log_critical("Error occurred while closing file: '%s'",
                replicate->initsync->fn);
    }
    replicate->initsync->fp = NULL;
    replicate->status = REPLICATE_PAUSED;
}

static void INITSYNC_work(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
#ifdef DEBUG
    assert (siridb->replicate->status == REPLICATE_RUNNING ||
            siridb->replicate->status == REPLICATE_STOPPING);
    assert (siridb->replicate->initsync != NULL);
    assert (siridb->replicate->initsync->fp != NULL);
#endif
    siridb_initsync_t * initsync = siridb->replicate->initsync;
    sirinet_pkg_t * pkg;

    initsync->series = imap32_get(siridb->series_map, *initsync->next_series_id);

    if (initsync->series != NULL)
    {
        siridb_series_incref(initsync->series);

        uv_mutex_lock(&siridb->series_mutex);

        siridb_points_t * points = siridb_series_get_points_num32(
                initsync->series,
                NULL,
                NULL);

        uv_mutex_unlock(&siridb->series_mutex);

        if (points != NULL)
        {
            qp_packer_t * packer = qp_packer_new(QP_SUGGESTED_SIZE);
            if (packer != NULL)
            {
                qp_add_type(packer, QP_MAP1);
                qp_add_string_term(packer, initsync->series->name);

                if (siridb_points_pack(points, packer) == 0)
                {
                    pkg = sirinet_pkg_new(
                            0,
                            packer->len,
                            BPROTO_INSERT_SERVER,
                            packer->buffer);
                    if (pkg != NULL)
                    {
                        siridb_server_send_pkg(
                                siridb->replica,
                                pkg,
                                INITSYNC_TIMEOUT,
                                INITSYNC_on_insert_response,
                                siridb);

                        free(pkg);
                    }
                }
                qp_packer_free(packer);
            }
            siridb_points_free(points);
        }
    }
    else
    {
        INITSYNC_next_series_id(siridb);
    }
}

/*
 * Call-back function: sirinet_promise_cb
 */
static void INITSYNC_on_insert_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    siridb_t * siridb = (siridb_t *) promise->data;

    switch ((sirinet_promise_status_t) status)
    {
    case PROMISE_WRITE_ERROR:
        /*
         * Write to socket error, data is not send so we should not commit.
         */
        if (siridb->replicate->status == REPLICATE_STOPPING)
        {
            siridb_series_decref(siridb->replicate->initsync->series);
            INITSYNC_pause(siridb->replicate);
        }
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
        log_error("Error occurred while sending series to the replica (%d)",
                status);

        INITSYNC_next_series_id(siridb);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
        }
#ifdef DEBUG
        assert(siridb->replicate->initsync->series != NULL);
#endif
        INITSYNC_next_series_id(siridb);
        break;
    default:
        assert (0);
        break;
    }

    free(promise);
}


/*
 * Typedef: imap32_cb
 *
 * Returns 0 if successful
 */
static int INITSYNC_create_cb(siridb_series_t * series, FILE * fp)
{
    series->flags |= SIRIDB_SERIES_INIT_REPL;
    return fwrite(&series->id, sizeof(uint32_t), 1, fp) - 1;
}

/*
 * Set initsync->fn to the correct file name.
 *
 * Returns the length of 'fn' or a negative value in case of an error.
 */
inline static int INITSYNC_fn(siridb_t * siridb, siridb_initsync_t * initsync)
{
     return asprintf(
             &initsync->fn,
             "%s%s",
             siridb->dbpath,
             INITSYC_FN);
}

/*
 * Open the initsync file. (set both the file pointer and file descriptor
 * In case of and error, initsync->fp is set to NULL
 */
void siridb_initsync_fopen(siridb_initsync_t * initsync, const char * opentype)
{
    initsync->fp = fopen(initsync->fn, opentype);
    if (initsync->fp != NULL)
    {
        initsync->fd = fileno(initsync->fp);
        if (initsync->fd == -1)
        {
            LOGC("Error reading file descriptor: '%s'", initsync->fn);
            fclose(initsync->fp);
            initsync->fp = NULL;
        }
    }
}

/*
 * Close the file pointer and remove the initial synchronization file.
 * Return 0 if successful or -1 in case of an error.
 */
static int INITSYNC_unlink(siridb_initsync_t * initsync)
{
#ifdef DEBUG
    assert (initsync->fp != NULL);
#endif
    fclose(initsync->fp);
    initsync->fp = NULL;

    if (unlink(initsync->fn))
    {
        log_critical(
                "Initial synchronization file could not be removed: '%s'",
                initsync->fn);
        return -1;
    }
    else
    {
        log_info("Initial synchronization file removed: '%s'", initsync->fn);
    }
    return 0;
}
