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
#include <qpack/qpack.h>

#define INITSYNC_SLEEP 100          // 100 milliseconds * active tasks
#define INITSYNC_TIMEOUT 120000     // 2 minutes
#define INITSYNC_RETRY 30000        // 30 seconds
#define INITSYC_FN ".initsync"

void siridb_initsync_fopen(siridb_initsync_t * initsync, const char * opentype);
static int INITSYNC_create_cb(siridb_series_t * series, FILE * fp);
static void INITSYNC_work(uv_timer_t * timer);
static void INITSYNC_next_series_id(siridb_t * siridb);
static int INITSYNC_unlink(siridb_initsync_t * initsync);
inline static int INITSYNC_fn(siridb_t * siridb, siridb_initsync_t * initsync);
static void INITSYNC_pause(siridb_replicate_t * replicate);
static void INITSYNC_send(uv_timer_t * timer);
static void INITSYNC_on_insert_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

static const long int SIZE2 = 2 * sizeof(uint32_t);
static char sync_progress[30];


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
        initsync->pkg = NULL;

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
                    if (imap_walk(
                                siridb->series_map,
                                (imap_cb) INITSYNC_create_cb,
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
                        series = imap_get(
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
                    initsync->size = ftello(initsync->fp);
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
                        if (fseeko(initsync->fp, -sizeof(uint32_t), SEEK_END) ||
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
                            siri_optimize_pause();
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
    free((*initsync)->pkg);
    free(*initsync);
    *initsync = NULL;
}

/*
 * Start initial replica work.
 */
void siridb_initsync_run(uv_timer_t * timer)
{
	siridb_t * siridb = (siridb_t *) timer->data;
    uv_timer_start(
            timer,
            (siridb->replicate->initsync->pkg == NULL) ?
                    INITSYNC_work : INITSYNC_send,
            INITSYNC_SLEEP * siridb->active_tasks,
            0);
}

/*
 * Returns a human readable synchronization progress status
 */
const char * siridb_initsync_sync_progress(siridb_t * siridb)
{
    if (siridb->replica == NULL || siridb->replicate->initsync == NULL)
    {
        sprintf(sync_progress, "not available");
    }
    else
    {
        size_t num = siridb->replicate->initsync->size / sizeof(uint32_t);
        size_t total = siridb->series_map->len;
        double percent = 100 * (double) (total - num) / total;

        sprintf(sync_progress,
                "approximately at %0.2f%%",
                (0 > percent) ? 0 : percent);
    }
    return sync_progress;
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
            log_critical("Error reading file descriptor: '%s'", initsync->fn);
            fclose(initsync->fp);
            initsync->fp = NULL;
        }
    }
}

/*
 * Read the next series id and truncate the synchronization file to remove
 * the last synchronization id.
 *
 * This function might destroy 'replicate->initsync' when initial
 * synchronization is finished.
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
    assert (initsync->size == fseeko(initsync->fp, 0, SEEK_END);
#endif

    /* free the current package (can be NULL already) */
    free(initsync->pkg);
    initsync->pkg = NULL;

    if (initsync->size >= SIZE2)
    {
        initsync->size -= sizeof(uint32_t);
        if (fseeko(initsync->fp, -SIZE2, SEEK_END) ||
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
        siri_optimize_continue();
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

/*
 * Type: uv_timer_cb
 *
 * This function sends a packed series to the replica server.
 */
static void INITSYNC_send(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
#ifdef DEBUG
    assert (siridb->replicate->initsync->pkg != NULL);
#endif

    if (siridb->replicate->status == REPLICATE_STOPPING)
    {
        INITSYNC_pause(siridb->replicate);
    }
    else
    {
        if (siridb_server_is_synchronizing(siridb->replica))
        {
            siridb_server_send_pkg(
                    siridb->replica,
                    siridb->replicate->initsync->pkg,
                    INITSYNC_TIMEOUT,
                    INITSYNC_on_insert_response,
                    siridb,
                    FLAG_KEEP_PKG);
        }
        else
        {
            log_info("Cannot send initial replica package to '%s' "
                    "(try again in %d seconds)",
                    siridb->replica->name,
                    INITSYNC_RETRY / 1000);
            uv_timer_start(
                    siridb->replicate->timer,
                    INITSYNC_send,
                    INITSYNC_RETRY,
                    0);
        }
    }
}

static void INITSYNC_work(uv_timer_t * timer)
{
    siridb_t * siridb = (siridb_t *) timer->data;
#ifdef DEBUG
    assert (siridb->replicate->status == REPLICATE_RUNNING ||
            siridb->replicate->status == REPLICATE_STOPPING);
    assert (siridb->replicate->initsync != NULL);
    assert (siridb->replicate->initsync->fp != NULL);
    assert (siridb->replicate->initsync->pkg == NULL);
#endif

    if (siridb->insert_tasks)
    {
    	siridb_initsync_run(timer);
    	return;
    }

    siridb_initsync_t * initsync = siridb->replicate->initsync;
    siridb_series_t * series;

    series = imap_get(siridb->series_map, *initsync->next_series_id);

    if (series != NULL)
    {
        uv_mutex_lock(&siridb->series_mutex);

        siridb_points_t * points = siridb_series_get_points(
                series,
                NULL,
                NULL);

        uv_mutex_unlock(&siridb->series_mutex);

        if (points != NULL)
        {
            qp_packer_t * packer = sirinet_packer_new(QP_SUGGESTED_SIZE);
            if (packer != NULL)
            {
                qp_add_type(packer, QP_MAP1);

                /* add name including string terminator */
                qp_add_raw(packer, series->name, series->name_len + 1);

                if (siridb_points_pack(points, packer) == 0)
                {
                    series->flags &= ~SIRIDB_SERIES_INIT_REPL;

                    initsync->pkg = sirinet_packer2pkg(
                            packer,
                            0,
                            BPROTO_INSERT_SERVER);

                    uv_timer_start(
                            siridb->replicate->timer,
                            INITSYNC_send,
                            0,
                            0);

                }
                else
                {
                    qp_packer_free(packer);  /* signal is raised */
                }
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
        uv_timer_start(
                siridb->replicate->timer,
                INITSYNC_send,
                INITSYNC_RETRY,
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
        /* TODO: maybe write pkg to an error queue ? */
        INITSYNC_next_series_id(siridb);
        break;
    case PROMISE_SUCCESS:
        if (sirinet_protocol_is_error(pkg->tp))
        {
            log_error(
                    "Error occurred while processing data on the replica: "
                    "(response type: %u)", pkg->tp);
            /* TODO: maybe write pkg to an error queue ? */
        }
        INITSYNC_next_series_id(siridb);
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
