/*
 * reindex.c - SiriDB Re-index.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 27-07-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/reindex.h>
#include <stddef.h>
#include <siri/err.h>
#include <logger/logger.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <siri/optimize.h>
#include <siri/db/servers.h>

#define REINDEX_FN ".reindex"
#define REINDEX_SLEEP 100           // 100 milliseconds
#define REINDEX_RETRY 30            // 30 seconds
#define REINDEX_TIMEOUT 60000       // 1 minute

#define NEXT_SERIES_ERR -1
#define NEXT_SERIES_SET 0
#define NEXT_SERIES_END 1

static const long int SIZE2 = 2 * sizeof(uint32_t);

inline static int REINDEX_fn(siridb_t * siridb, siridb_reindex_t * reindex);
static int REINDEX_create_cb(siridb_series_t * series, FILE * fp);
static int REINDEX_unlink(siridb_reindex_t * reindex);
static int REINDEX_next_series_id(siridb_reindex_t * reindex);
static void REINDEX_on_insert_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

static char reindex_progress[30];

/*
 * Returns a pointer to reindex. If 'create_new' is zero and an
 * re-index file cannot be found, NULL is returned.
 *
 * In case of an error we also return NULL but in this case a SIGNAL is raised
 */
siridb_reindex_t * siridb_reindex_open(siridb_t * siridb, int create_new)
{
    siridb_reindex_t * reindex =
            (siridb_reindex_t *) malloc(sizeof(siridb_reindex_t));
    if (reindex == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        reindex->fn = NULL;
        reindex->fp = NULL;
        reindex->next_series_id = NULL;
        reindex->pkg = NULL;
        reindex->timer = NULL;
        reindex->server = NULL;

        if (REINDEX_fn(siridb, reindex) < 0)
        {
            ERR_ALLOC
            siridb_reindex_free(&reindex);
        }
        else
        {
            siridb_reindex_fopen(reindex, create_new ? "w+" : "r+");
            if (reindex->fp == NULL)
            {
                siridb_reindex_free(&reindex);
            }
            else
            {
                siridb->flags |= SIRIDB_FLAG_REINDEXING;

                if (create_new)
                {
                    if (imap32_walk(
                                siridb->series_map,
                                (imap32_cb) REINDEX_create_cb,
                                reindex->fp) || fflush(reindex->fp))
                    {
                        ERR_FILE
                        siridb_reindex_free(&reindex);
                    }
                }

                if (reindex != NULL)
                {
                    reindex->size = ftell(reindex->fp);
                    if (reindex->size == -1)
                    {
                        ERR_FILE
                        siridb_reindex_free(&reindex);
                    }
                    else if (reindex->size == 0)
                    {
                        /* this is the new server, or a finished one */
                        siridb_reindex_free(&reindex);
                    }
                    else
                    {
                        reindex->next_series_id =
                                (uint32_t *) malloc(sizeof(uint32_t));

                        if (reindex->next_series_id == NULL)
                        {
                            ERR_ALLOC
                            siridb_reindex_free(&reindex);
                        }
                        else if (
                            fseek(reindex->fp, -sizeof(uint32_t), SEEK_END) ||
                            fread(  reindex->next_series_id,
                                    sizeof(uint32_t),
                                    1,
                                    reindex->fp) != 1)
                        {
                            ERR_FILE
                            siridb_reindex_free(&reindex);
                        }
                        else
                        {
                            reindex->timer =
                                    (uv_timer_t *) malloc(sizeof(uv_timer_t));
                            if (reindex->timer == NULL)
                            {
                                ERR_ALLOC
                                siridb_reindex_free(&reindex);
                            }
                            else
                            {
                                reindex->server = siridb->pools->pool[
                                          siridb->pools->len -1].server[0];
                                siridb->server->flags |= SERVER_FLAG_REINDEXING;
                                siridb->flags |= SIRIDB_FLAG_REINDEXING;
                                reindex->timer->data = siridb;
                                siri_optimize_pause();
                                if (create_new)
                                {
                                    /*
                                     * Sending the flags is only needed when
                                     * the re-index was just created. Otherwise
                                     * the flags are send when we authenticate.
                                     */
                                    siridb_servers_send_flags(siridb->servers);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return reindex;
}

/*
 * Returns a human readable re-index progress status
 */
const char * siridb_reindex_progress(siridb_t * siridb)
{
    if (siridb->reindex == NULL)
    {
        sprintf(reindex_progress, "not available");
    }
    else
    {
        size_t num = siridb->reindex->size / sizeof(uint32_t);
        size_t total = siridb->series_map->len;
        double percent = 100 * (double) (total - num) / total;

        sprintf(reindex_progress,
                "approximately at %0.2f%%",
                (0 > percent) ? 0 : percent);
    }
    return reindex_progress;
}

/*
 * This will close the re-index task and sets 'reindex->timer' to NULL.
 *
 * Warning: make sure open promises are closed since the 'REINDEX_work'
 * depends on having siridb->reindex.
 */
void siridb_reindex_close(siridb_reindex_t * reindex)
{
#ifdef DEBUG
    assert (reindex != NULL && reindex->timer != NULL);
#endif
    /* we can use uv_timer_stop() even if the timer is not scheduled */
    uv_timer_stop(reindex->timer);
    uv_close((uv_handle_t *) reindex->timer, (uv_close_cb) free);
    reindex->timer = NULL;
}

/*
 * Destroy *reindex and set reindex to NULL. Parsing *reindex == NULL is
 * not allowed and reindex->timer must be NULL. (use 'siridb_reindex_close' to
 * close and set the timer to NULL)
 *
 * (a SIGNAL is raised in case the file cannot be closed)
 */
void siridb_reindex_free(siridb_reindex_t ** reindex)
{
#ifdef DEBUG
    assert ((*reindex)->timer == NULL);
#endif
    if ((*reindex)->fp != NULL && fclose((*reindex)->fp))
    {
        ERR_FILE
    }
    free((*reindex)->fn);
    free((*reindex)->next_series_id);
    free(*reindex);
    *reindex = NULL;
}

/*
 * Open the reindex file. (set both the file pointer and file descriptor
 * In case of and error, reindex->fp is set to NULL
 */
void siridb_reindex_fopen(siridb_reindex_t * reindex, const char * opentype)
{
    reindex->fp = fopen(reindex->fn, opentype);
    if (reindex->fp != NULL)
    {
        reindex->fd = fileno(reindex->fp);
        if (reindex->fd == -1)
        {
            LOGC("Error reading file descriptor: '%s'", reindex->fn);
            fclose(reindex->fp);
            reindex->fp = NULL;
        }
    }
}

/*
 * Only call this function when the re-index flag is set and
 */
void siridb_reindex_update(siridb_t * siridb)
{
#ifdef DEBUG
    assert (siridb->server->flags & ~SERVER_FLAG_REINDEXING);
    assert (siridb->flags & SIRIDB_FLAG_REINDEXING);
#endif
    if (siridb_servers_is_available(siridb))
    {
        siridb->flags &= ~SIRIDB_FLAG_REINDEXING;
        log_info("Finished re-indexing database '%s'", siridb->dbname);
        REINDEX_unlink(siridb->reindex);
    }
}

static void REINDEX_send(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;

    if (siridb_server_is_available(siridb->reindex->server))
    {
        siridb_server_send_pkg(
                siridb->reindex->server,
                siridb->reindex->pkg,
                REINDEX_TIMEOUT,
                REINDEX_on_insert_response,
                siridb);
    }
    else
    {
        log_info("Cannot send initial replica package to '%s' "
                "(try again in %d seconds)",
                siridb->reindex->server->name,
                REINDEX_RETRY / 1000);
        uv_timer_start(
                siridb->reindex->timer,
                REINDEX_send,
                REINDEX_RETRY,
                0);
    }
}

/*
 * Return values:
 *  NEXT_SERIES_SET: Successful set the next series id and removes the previous
 *                   series id
 *  NEXT_SERIES_END: End of the file is reached, re-indexing has finished.
 *  NEXT_SERIES_ERR: An error occurred and a SIGNAL is raised
 */
static int REINDEX_next_series_id(siridb_reindex_t * reindex)
{
#ifdef DEUBUG
    assert (reindex->size == fseek(reindex->fp, 0, SEEK_END);
#endif

    /* free re-index package */
    free(reindex->pkg);
    reindex->pkg = NULL;

    int rc;
    reindex->size -= sizeof(uint32_t);

    if (!reindex->size)
    {
        rc = NEXT_SERIES_END;
    }
    else
    {
        if (fseek(reindex->fp, -SIZE2, SEEK_END) ||
            fread(  reindex->next_series_id,
                    sizeof(uint32_t),
                    1,
                    reindex->fp) != 1)
        {
            ERR_FILE
            log_critical("Reading next series id has failed");
            return NEXT_SERIES_ERR;
        }
        rc = NEXT_SERIES_SET;
    }

    if (ftruncate(reindex->fd, reindex->size))
    {
        ERR_FILE
        log_critical("Reading next series id has failed");
        return NEXT_SERIES_ERR;
    }
    return rc;
}

static void REINDEX_work(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
    siridb_reindex_t * reindex = siridb->reindex;

#ifdef DEBUG
    assert (SIRI_OPTIMZE_IS_PAUSED);
    assert (reindex != NULL);
#endif

    uint16_t pool;
    siridb_series_t * series =
            imap32_get(siridb->series_map, *reindex->next_series_id);

    if (    series == NULL ||
            (pool = siridb_pool_sn(
                    series->name)) == siridb->server->pool ||
            (   siridb->replica != NULL &&
                (series->mask & 1) == siridb->server->id))
    {
        switch (REINDEX_next_series_id(reindex))
        {
        case NEXT_SERIES_SET:
            uv_timer_start(reindex->timer, REINDEX_work, REINDEX_SLEEP, 0);
            break;
        case NEXT_SERIES_END:

            siridb_reindex_free(&reindex);

            /* update and send the flags */
            siridb->server->flags &= ~SERVER_FLAG_REINDEXING;
            siridb_servers_send_flags(siridb->servers);

            log_info("Re-indexing has successfully finished on '%s'",
                    siridb->server->name);

            siridb_reindex_update(siridb);

            siri_optimize_continue();
            break;
        case NEXT_SERIES_ERR:
            break; /* signal is raised */
        }

    }
    else
    {
        /*
         * lock is not needed since we are sure the optimize task is
         * not running
         */
        siridb_points_t * points = siridb_series_get_points_num32(
                series,
                NULL,
                NULL);

        if (points != NULL)
        {
            qp_packer_t * packer = sirinet_packer_new(QP_SUGGESTED_SIZE);
            if (packer != NULL)
            {
                qp_add_type(packer, QP_MAP1);
                qp_add_string_term(packer, series->name);

                if (siridb_points_pack(points, packer) == 0)
                {
                    reindex->pkg = sirinet_packer2pkg(
                            packer,
                            0,
                            BPROTO_INSERT_SERVER);

                    uv_timer_start(
                            reindex->timer,
                            REINDEX_send,
                            0,
                            0);
                }
                else
                {
                    qp_packer_free(packer);  /* signal raised */
                }
            }
            siridb_points_free(points);  /* signal raised */
        }
    }
}

/*
 * Call-back function: sirinet_promise_cb
 */
static void REINDEX_on_insert_response(
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
        uv_timer_start(
                siridb->reindex->timer,
                REINDEX_send,
                REINDEX_RETRY,
                0);
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
        REINDEX_next_series_id(siridb);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
        }

        REINDEX_next_series_id(siridb);
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
static int REINDEX_create_cb(siridb_series_t * series, FILE * fp)
{
    return fwrite(&series->id, sizeof(uint32_t), 1, fp) - 1;
}

/*
 * Set reindex->fn to the correct file name.
 *
 * Returns the length of 'fn' or a negative value in case of an error.
 */
inline static int REINDEX_fn(siridb_t * siridb, siridb_reindex_t * reindex)
{
     return asprintf(
             &reindex->fn,
             "%s%s",
             siridb->dbpath,
             REINDEX_FN);
}

/*
 * Close the file pointer and remove the re-index file.
 * Return 0 if successful or -1 in case of an error.
 */
static int REINDEX_unlink(siridb_reindex_t * reindex)
{
#ifdef DEBUG
    assert (reindex->fp != NULL);
#endif
    fclose(reindex->fp);
    reindex->fp = NULL;

    if (unlink(reindex->fn))
    {
        log_critical(
                "Re-index file could not be removed: '%s'",
                reindex->fn);
        return -1;
    }
    else
    {
        log_info("Re-index file removed: '%s'", reindex->fn);
    }
    return 0;
}
