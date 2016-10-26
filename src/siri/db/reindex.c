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
 * Differences while re-indexing:
 *
 *  - Group information like number of series will be updated at a lower
 *    interval which leads to probably incorrect number of series per group.
 *    Selections for series in a group or a list of series per group are still
 *    correct and can only lack of brand new series. (newer than 30 seconds)
 *
 *  - Selecting an unknown series usually raises a QueryError but we do not
 *    raise this error during re-indexing since the series might be in either
 *    the old- or new pool. (selecting series during re-indexing has therefore
 *    the same behavior as a regular expression selection)
 *
 *  - Drop server is not allowed while re-indexing.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/db/db.h>
#include <siri/db/reindex.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/err.h>
#include <siri/net/protocol.h>
#include <siri/optimize.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#define REINDEX_FN ".reindex"
#define REINDEX_SLEEP 100           // 100 milliseconds * active tasks
#define REINDEX_RETRY 5000          // 5 seconds
#define REINDEX_INITWAIT 20000      // 20 seconds
#define REINDEX_TIMEOUT 300000      // 5 minutes

#define NEXT_SERIES_ERR -1
#define NEXT_SERIES_SET 0
#define NEXT_SERIES_END 1

static const long int SIZE2 = 2 * sizeof(uint32_t);
static const size_t PCKSZ = PKG_HEADER_SIZE + 5;

inline static int REINDEX_fn(siridb_t * siridb, siridb_reindex_t * reindex);
static int REINDEX_create_cb(siridb_series_t * series, FILE * fp);
static int REINDEX_unlink(siridb_reindex_t * reindex);
static int REINDEX_next_series_id(siridb_reindex_t * reindex);
static void REINDEX_next(siridb_t * siridb);
static void REINDEX_work(uv_timer_t * timer);
static void REINDEX_commit_series(siridb_t * siridb);
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
                    if (imap_walk(
                                siridb->series_map,
                                (imap_cb) REINDEX_create_cb,
                                reindex->fp) || fflush(reindex->fp))
                    {
                        ERR_FILE
                        siridb_reindex_free(&reindex);
                    }
                }
                else
                {
                    siridb->pools->prev_lookup =
                            siridb_lookup_new(siridb->pools->len - 1);
                    if (siridb->pools->prev_lookup == NULL)
                    {
                        siridb_reindex_free(&reindex);  /* signal is raised */
                    }
                }

                if (reindex != NULL)
                {
                    reindex->size = ftello(reindex->fp);
                    if (reindex->size == -1)
                    {
                        ERR_FILE
                        siridb_reindex_free(&reindex);
                    }
                    else if (reindex->size)
                    {
                        reindex->next_series_id =
                                (uint32_t *) malloc(sizeof(uint32_t));

                        if (reindex->next_series_id == NULL)
                        {
                            ERR_ALLOC
                            siridb_reindex_free(&reindex);
                        }
                        else if (
                            fseeko(reindex->fp, -sizeof(uint32_t), SEEK_END) ||
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
                                reindex->timer->data = siridb;
                                siri_optimize_pause();

                                uv_timer_init(siri.loop, reindex->timer);
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
    if (    siridb->reindex == NULL ||
            siridb->reindex->timer == NULL ||
            !siridb->reindex->size)
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
    log_debug("Free re-index");
#endif
    if ((*reindex)->fp != NULL && fclose((*reindex)->fp))
    {
        ERR_FILE
    }
    free((*reindex)->fn);
    free((*reindex)->next_series_id);
    free((*reindex)->pkg);
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
        	log_critical("Error reading file descriptor: '%s'", reindex->fn);
            fclose(reindex->fp);
            reindex->fp = NULL;
        }
    }
}

/*
 * Only call this function when the re-index flag is set and the server
 * re-index flag is not set.
 *
 * This task can destroy siridb->reindex when all servers are finished with
 * re-indexing.
 */
void siridb_reindex_status_update(siridb_t * siridb)
{
#ifdef DEBUG
    assert (~siridb->server->flags & SERVER_FLAG_REINDEXING);
    assert (siridb->flags & SIRIDB_FLAG_REINDEXING);
#endif
    if (siridb_servers_available(siridb))
    {
        siridb->flags &= ~SIRIDB_FLAG_REINDEXING;

        /* free previous lookup */
        siridb_lookup_free(siridb->pools->prev_lookup);
        siridb->pools->prev_lookup = NULL;

        REINDEX_unlink(siridb->reindex);
        siridb_reindex_free(&siridb->reindex);
        log_info("Finished re-indexing database '%s'", siridb->dbname);
    }
}

/*
 * Make sure the optimize task is set to paused before calling this function.
 * Its not required that the optimize task is already in PAUSED status.
 * In that case we just wait for the PAUSED status.
 */
void siridb_reindex_start(uv_timer_t * timer)
{
    if (timer == NULL)
    {
        log_debug("No series are scheduled for re-indexing");
    }
    else
    {
#ifdef DEBUG
        assert (siri.optimize->pause);
#endif
        if (!SIRI_OPTIMZE_IS_PAUSED)
        {
            log_debug("Wait for the optimize task to pause");
            uv_timer_start(timer, siridb_reindex_start, 1000, 0);
        }
        else
        {
            log_debug("Start re-indexing task in %u seconds",
                    REINDEX_INITWAIT / 1000);
            uv_timer_start(timer, REINDEX_work, REINDEX_INITWAIT, 0);
        }
    }
}

/*
 * Type: uv_timer_cb
 *
 * This function sends a packed series to the new (pool) server.
 */
static void REINDEX_send(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
#ifdef DEBUG
    assert (siridb->reindex->pkg != NULL);
#endif
    /* actually 'available' is sufficient since the destination server has
     * never status 're-indexing' unless one day we support down-scaling.
     */
    if (siridb_server_is_accessible(siridb->reindex->server))
    {
        siridb_server_send_pkg(
                siridb->reindex->server,
                siridb->reindex->pkg,
                REINDEX_TIMEOUT,
                REINDEX_on_insert_response,
                siridb,
                FLAG_KEEP_PKG);
    }
    else
    {
        log_info("Cannot send re-index package to '%s' "
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
    assert (reindex->size == fseeko(reindex->fp, 0, SEEK_END);
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
        if (fseeko(reindex->fp, -SIZE2, SEEK_END) ||
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

/*
 * This function can raise a SIGNAL
 */
static void REINDEX_next(siridb_t * siridb)
{
    switch (REINDEX_next_series_id(siridb->reindex))
    {
    case NEXT_SERIES_SET:
        uv_timer_start(
        		siridb->reindex->timer,
				REINDEX_work,
				REINDEX_SLEEP * siridb->active_tasks,
				0);
        break;

    case NEXT_SERIES_END:
        /* update and send the flags */
        siridb->server->flags &= ~SERVER_FLAG_REINDEXING;
        siridb_servers_send_flags(siridb->servers);

        log_info("Re-indexing has successfully finished on '%s'",
                siridb->server->name);

        /* we can close the timer */
        siridb_reindex_close(siridb->reindex);

        /* check if everyone is finished and if so destroy re-index */
        siridb_reindex_status_update(siridb);

        siri_optimize_continue();
        break;

    case NEXT_SERIES_ERR:
        break; /* signal is raised */
    }
}

static void REINDEX_work(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
    siridb_reindex_t * reindex = siridb->reindex;

#ifdef DEBUG
    assert (SIRI_OPTIMZE_IS_PAUSED);
    assert (reindex != NULL);
    assert (siridb->reindex->pkg == NULL);
#endif

    reindex->series = imap_get(siridb->series_map, *reindex->next_series_id);

    if (    reindex->series == NULL ||
            siridb_lookup_sn(
                    siridb->pools->lookup,
                    reindex->series->name) == siridb->server->pool ||
            (siridb->replica != NULL &&
			 siridb_series_server_id(reindex->series) != siridb->server->id))
    {
        REINDEX_next(siridb);
    }
    else
    {
        /*
         * lock is not needed since we are sure the optimize task is
         * not running
         */
#ifdef DEBUG
        assert (siridb_lookup_sn(
                    siridb->pools->prev_lookup,
                    reindex->series->name) == siridb->server->pool);
#endif
        siridb_points_t * points = siridb_series_get_points(
                reindex->series,
                NULL,
                NULL);

        if (points != NULL)  /* signal is raised in case NULL */
        {
            /*
             * Prepare drop, increasing the reference counter is not needed
             * since the series can only be decremented when dropped. since
             * the series is not member of the siridb->series_map it will not
             * be decremented there either.
             */

            siridb_series_drop_prepare(siridb, reindex->series);

            qp_packer_t * packer = sirinet_packer_new(QP_SUGGESTED_SIZE);
            if (packer != NULL)
            {
                qp_add_type(packer, QP_MAP1);

                /* add series name including terminator char */
                qp_add_raw(
                        packer,
                        reindex->series->name,
                        reindex->series->name_len + 1);

                if (siridb_points_pack(points, packer) == 0)
                {
                    reindex->pkg = sirinet_packer2pkg(
                            packer,
                            0,
                            BPROTO_INSERT_TESTED_SERVER);
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
            siridb_points_free(points);
        }
    }
}

/*
 * Sends 'dropped series' to the replica and commit the drop.
 *
 * This function can raise an ALLOC error but file errors are only logged.
 */
static void REINDEX_commit_series(siridb_t * siridb)
{
    /*
     * Send the dropped series to the replica. The replica server might have
     * received points between the time we prepared the drop and commit the
     * drop but this is not an issue because the replica then forwards the
     * points to this server and this server to the new server since the series
     * is not in 'this' series store anymore
     */
    if (siridb->replica != NULL)
    {
        size_t len = siridb->reindex->series->name_len + 1;
        qp_packer_t * packer = sirinet_packer_new(PCKSZ + len);
        if (packer != NULL)
        {
            /* no need for testing, fits for sure */
            qp_add_raw(packer, siridb->reindex->series->name, len);
            sirinet_pkg_t * pkg = sirinet_packer2pkg(
                    packer,
                    0,
                    BPROTO_DROP_SERIES);
            siridb_replicate_pkg(siridb, pkg);
            free(pkg);
        }
    }

    /* commit the drop */
    if (siridb_series_drop_commit(siridb, siridb->reindex->series) == 0)
    {
        siridb_series_flush_dropped(siridb);
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
        REINDEX_commit_series(siridb);
        REINDEX_next(siridb);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
        }
        REINDEX_commit_series(siridb);
        REINDEX_next(siridb);
        break;
    default:
        assert (0);
        break;
    }

    sirinet_promise_decref(promise);
}

/*
 * Typedef: imap_cb
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
