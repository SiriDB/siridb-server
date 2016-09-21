/*
 * shard.c - SiriDB Shard.

 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <imap/imap.h>
#include <siri/db/series.h>
#include <logger/logger.h>
#include <stdio.h>
#include <siri/siri.h>
#include <ctree/ctree.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <slist/slist.h>
#include <siri/file/pointer.h>

#define GET_FN(shrd)                                                       \
/* we are sure this fits since the max possible length is checked */        \
shrd->fn = (char *) malloc(PATH_MAX * sizeof(char) ];                                                          \
sprintf(fn, "%s%s%ld%s", siridb->dbpath,                                    \
            SIRIDB_SHARDS_PATH, shrd->id, ".sdb");

/* shard schema (schemas below 20 are reserved for Python SiriDB) */
#define SIRIDB_SHARD_SHEMA 20

/*
 * Header schema layout
 *
 * Total Size 20
 * 0    (uint8_t)   SHEMA
 * 1    (uint64_t)  ID
 * 9    (uint64_t)  DURATION
 * 17   (uint8_t)   TP
 * 18   (uint8_t)   TIME_PRECISION
 * 19   (uint8_t)   FLAGS
 *
 */
#define HEADER_SIZE 20
#define HEADER_SCHEMA 0
#define HEADER_ID 1
#define HEADER_DURATION 9
#define HEADER_TP 17
#define HEADER_TIME_PRECISION 18
#define HEADER_FLAGS 19

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint32_t)  START_TS
 * 8    (uint32_t)  END_TS
 * 12   (uint16_t)  LEN
 */
#define IDX_NUM32_SZ 14

/* 0    (uint32_t)  SERIES_ID
 * 4    (uint64_t)  START_TS
 * 12   (uint64_t)  END_TS
 * 20   (uint16_t)  LEN
 */
#define IDX_NUM64_SZ 22

#define SHARD_STATUS_SIZE 7

static const siridb_shard_flags_repr_t flags_map[SHARD_STATUS_SIZE] = {
        {.repr="optimize-scheduled", .flag=SIRIDB_SHARD_MANUAL_OPTIMIZE},
        {.repr="overlap", .flag=SIRIDB_SHARD_HAS_OVERLAP},
        {.repr="new-values", .flag=SIRIDB_SHARD_HAS_NEW_VALUES},
        {.repr="dropped-series", .flag=SIRIDB_SHARD_HAS_DROPPED_SERIES},
        {.repr="dropped", .flag=SIRIDB_SHARD_IS_REMOVED},
        {.repr="loading", .flag=SIRIDB_SHARD_IS_LOADING},
        {.repr="corrupt", .flag=SIRIDB_SHARD_IS_CORRUPT},
};

const char shard_type_map[2][7] = {
        "number",
        "log"
};

static int SHARD_load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp);

static void SHARD_free(siridb_shard_t * shard);
static int SHARD_init_fn(siridb_t * siridb, siridb_shard_t * shard);

/*
 * Returns 0 if successful or -1 in case of an error.
 * When an error occurs, a SIGNAL can be raised in some cases but not for sure.
 */
int siridb_shard_load(siridb_t * siridb, uint64_t id)
{
    siridb_shard_t * shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    if (shard == NULL)
    {
        return -1;
    }
    shard->fp = siri_fp_new();
    if (shard->fp == NULL)
    {
        free(shard);
        return -1;  /* signal is raised */
    }
    shard->id = id;
    shard->ref = 1;
    shard->replacing = NULL;
    if (SHARD_init_fn(siridb, shard) < 0)
    {
        siridb_shard_decref(shard);
        return -1;
    }
    FILE * fp;

    log_debug("Loading shard %ld", id);

    if ((fp = fopen(shard->fn, "r")) == NULL)
    {
        siridb_shard_decref(shard);
        log_error("Cannot open file for reading: '%s'", shard->fn);
        return -1;
    }

    char header[HEADER_SIZE];

    if (fread(&header, HEADER_SIZE, 1, fp) != 1)
    {
        /* cannot read header from shard file,
         * close file decrement reference shard and return -1
         */
        fclose(fp);
        siridb_shard_decref(shard);
        log_critical("Missing header in shard file: '%s'", shard->fn);
        return -1;
    }

    /* set shard type and status flag */
    shard->tp = (uint8_t) header[HEADER_TP];
    shard->flags = (uint8_t) header[HEADER_FLAGS] | SIRIDB_SHARD_IS_LOADING;

    SHARD_load_idx_num32(siridb, shard, fp);

    if (fclose(fp))
    {
        siridb_shard_decref(shard);
        log_critical("Cannot close shard file: '%s'", shard->fn);
        return -1;
    }

    if (imap_add(siridb->shards, id, shard) == -1)
    {
        siridb_shard_decref(shard);
        return -1;  /* signal is raised */
    }

    /* remove LOADING flag from shard status */
    shard->flags &= ~SIRIDB_SHARD_IS_LOADING;

    return 0;
}

/*
 * Create a new shard file and return a siridb_shard_t object.
 *
 * In case of an error the return value is NULL and a SIGNAL is raised.
 */
siridb_shard_t *  siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp,
        siridb_shard_t * replacing)
{
    siridb_shard_t * shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    if (shard == NULL)
    {
        ERR_ALLOC
        return NULL;
    }
    if ((shard->fp = siri_fp_new()) == NULL)
    {
        free(shard);
        return NULL;  /* signal is raised */
    }
    shard->id = id;
    shard->ref = 1;
    shard->flags = SIRIDB_SHARD_OK;
    shard->tp = tp;
    shard->replacing = replacing;
    FILE * fp;
    if (SHARD_init_fn(siridb, shard) < 0)
    {
        ERR_ALLOC
        siridb_shard_decref(shard);
        return NULL;
    }

    if ((fp = fopen(shard->fn, "w")) == NULL)
    {
        ERR_FILE
        siridb_shard_decref(shard);
        log_critical("Cannot create shard file: '%s'", shard->fn);
        return NULL;
    }

    /* 0    (uint8_t)   SHEMA
     * 1    (uint64_t)  ID
     * 9    (uint64_t)  DURATION
     * 17   (uint8_t)   TP
     * 18   (uint8_t)   TIME_PRECISION
     * 19   (uint8_t)   FLAGS
     */
    if (    fputc(SIRIDB_SHARD_SHEMA, fp) == EOF ||
            fwrite(&id, sizeof(uint64_t), 1, fp) != 1 ||
            fwrite(&duration, sizeof(uint64_t), 1, fp) != 1 ||
            fputc(tp, fp) == EOF ||
            fputc(siridb->time->precision, fp) == EOF ||
            fputc(shard->flags, fp) == EOF)
    {
        ERR_FILE
        fclose(fp);
        siridb_shard_decref(shard);
        log_critical("Cannot write to shard file: '%s'", shard->fn);
        return NULL;
    }

    if (fclose(fp))
    {

        ERR_FILE
        siridb_shard_decref(shard);
        log_critical("Cannot close shard file: '%s'", shard->fn);
        return NULL;
    }

    if (imap_add(siridb->shards, id, shard) == -1)
    {
        ERR_ALLOC
        siridb_shard_decref(shard);
        return NULL;
    }

    /*
     * This is not critical at this point and it's hard to imagine this
     * fails if all the above was successful
     */
    siri_fopen(siri.fh, shard->fp, shard->fn, "r+");

    return shard;
}

/*
 * Call-back function used to validate shards in a where expression.
 *
 * Returns 0 or 1 (false or true).
 */
int siridb_shard_cexpr_cb(
        siridb_shard_view_t * vshard,
        cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_SID:
        return cexpr_int_cmp(cond->operator, vshard->shard->id, cond->int64);
    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(cond->operator, vshard->server->pool, cond->int64);
    case CLERI_GID_K_SIZE:
        {
            ssize_t size = siridb_shard_get_size(vshard->shard);
            return cexpr_int_cmp(cond->operator, size, cond->int64);
        }
    case CLERI_GID_K_START:
        return cexpr_int_cmp(cond->operator, vshard->start, cond->int64);
    case CLERI_GID_K_END:
        return cexpr_int_cmp(cond->operator, vshard->end, cond->int64);
    case CLERI_GID_K_TYPE:
        return cexpr_int_cmp(cond->operator, vshard->shard->tp, cond->int64);
    case CLERI_GID_K_SERVER:
        return cexpr_str_cmp(cond->operator, vshard->server->name, cond->str);
    case CLERI_GID_K_STATUS:
        {
            char buffer[SIRIDB_SHARD_STATUS_STR_MAX];
            siridb_shard_status(buffer, vshard->shard);
            return cexpr_str_cmp(cond->operator, buffer, cond->str);
        }
    }
    log_critical("Unexpected shard property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * Make sure 'str' is a pointer to a string which can hold at least
 * SIRIDB_SHARD_STR_MAX.
 */
void siridb_shard_status(char * str, siridb_shard_t * shard)
{
    char * pt = str;

    if (shard->replacing != NULL)
    {
        pt += sprintf(pt, "optimizing");
    }

    uint8_t flags = shard->flags;

    for (int i = 0; i < SHARD_STATUS_SIZE && flags; i++)
    {
        if ((flags & flags_map[i].flag) == flags_map[i].flag)
        {
            flags -= flags_map[i].flag;
            pt += (pt == str) ? sprintf(pt, "%s", flags_map[i].repr) :
                    sprintf(pt, " | %s", flags_map[i].repr);
        }
    }

    if (pt == str)
    {
        sprintf(pt, "ok");
    }
}

/*
 * Return the size or -1 in case of an error;
 */
ssize_t siridb_shard_get_size(siridb_shard_t * shard)
{
    if (shard->fp->fp == NULL)
    {
        struct stat st;
        return (stat(shard->fn, &st) == 0) ? st.st_size : -1;
    }

    fseeko(shard->fp->fp, 0, SEEK_END);
    return ftello(shard->fp->fp);
}

/*
 * Writes an index and points to a shard. The return value is the position
 * where the points start in the shard file.
 *
 * If an error has occurred, EOF will be returned and a SIGNAL will be raised.
 */
long int siridb_shard_write_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end)
{
    FILE * fp;
    uint16_t len = end - start;
    uint_fast32_t i;
    long int pos;

    if (shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, shard->fp, shard->fn, "r+"))
        {
            ERR_FILE
            log_critical("Cannot open file '%s'", shard->fn);
            return EOF;
        }
    }
    fp = shard->fp->fp;

    if (
        fseeko(fp, 0, SEEK_END) ||
        fwrite(&series->id, sizeof(uint32_t), 1, fp) != 1 ||
        fwrite(&points->data[start].ts, siridb->time->ts_sz, 1, fp) != 1 ||
        fwrite(&points->data[end - 1].ts, siridb->time->ts_sz, 1, fp) != 1 ||
        fwrite(&len, sizeof(uint16_t), 1, fp) != 1 ||
        (pos = ftello(fp)) < 0)
    {
        ERR_FILE
        log_critical("Cannot write index header to file '%s'", shard->fn);
        return EOF;
    }

    /* TODO: this works for both double and integer.
     * Add size values for strings and write string using 'old' way
     */
    for (i = start; i < end; i++)
    {
        if (fwrite(&points->data[i].ts, siridb->time->ts_sz, 1, fp) != 1 ||
            fwrite(&points->data[i].val, 8, 1, fp) != 1)
        {
            ERR_FILE
            log_critical("Cannot write points to file '%s'", shard->fn);
            return EOF;
        }
    }

    if (fflush(fp))
    {
        ERR_FILE
        log_critical("Cannot write flush file '%s'", shard->fn);
        return EOF;
    }

    return pos;
}

/*
 * Returns 0 if successful or -1 in case of an error. SiriDB might recover
 * from this error so we do not consider this critical.
 */
int siridb_shard_get_points_num32(
        siridb_points_t * points,
        idx_num32_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    size_t len = points->len + idx->len;
    /*
     * Index length is limited to max_chunck_points so we are able to store
     * one chunk in stack memory.
     */
    uint32_t temp[idx->len * 3];
    uint32_t * pt;

    if (idx->shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, idx->shard->fp, idx->shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip reading points",
                    idx->shard->fn);
            return -1;
        }
    }

    if (fseeko(idx->shard->fp->fp, idx->pos, SEEK_SET) ||
        fread(
            temp,
            12,
            idx->len,
            idx->shard->fp->fp) != idx->len)
    {
        if (idx->shard->flags & SIRIDB_SHARD_IS_CORRUPT)
        {
            log_error("Cannot read from shard id %lu", idx->shard->id);
        }
        else
        {
            log_critical(
                    "Cannot read from shard id %lu. The next optimize cycle "
                    "will fix this shard but you might loose some data.",
                    idx->shard->id);
            idx->shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
        }
        return -1;
    }

    /* set pointer to start */
    pt = temp;

    /* crop from start if needed */
    if (start_ts != NULL)
    {
        for (; *pt < *start_ts; pt += 3, len--);
    }

    /* crop from end if needed */
    if (end_ts != NULL)
    {
        for (   uint32_t * p = temp + 3 * (idx->len - 1);
                *p >= *end_ts;
                p -= 3, len--);
    }

    if (    has_overlap &&
            points->len &&
            (idx->shard->flags & SIRIDB_SHARD_HAS_OVERLAP))
    {
        for (uint64_t ts; points->len < len; pt += 3)
        {
            ts = *pt;
            siridb_points_add_point(points, &ts, ((qp_via_t *) (pt + 1)));
        }
    }
    else
    {
        for (; points->len < len; points->len++, pt += 3)
        {
            points->data[points->len].ts = *pt;
            points->data[points->len].val = *((qp_via_t *) (pt + 1));
        }
    }

    return 0;
}

/*
 * This function will be called from the 'optimize' thread.
 *
 * Returns 0 if successful or -1 and a SIGNAL is raised in case of an error.
 */
int siridb_shard_optimize(siridb_shard_t * shard, siridb_t * siridb)
{
    siridb_shard_t * new_shard = NULL;

    uint64_t duration = (shard->tp == SIRIDB_SHARD_TP_NUMBER) ?
            siridb->duration_num : siridb->duration_log;
    siridb_series_t * series;

    uv_mutex_lock(&siridb->shards_mutex);

    if ((siridb_shard_t *) imap_pop(siridb->shards, shard->id) == shard)
    {
        if ((new_shard = siridb_shard_create(
            siridb,
            shard->id,
            duration,
            shard->tp,
            shard)) == NULL)
        {
            /* signal is raised */
            log_critical(
                    "Cannot create shard id '%lu' for optimizing.",
                    shard->id);
        }
        else
        {
            siridb_shard_incref(new_shard);
        }
    }
    else
    {
        log_warning(
                "Skip optimizing shard id '%lu' since the shard has changed "
                "since building the optimize shard list.", shard->id);
    }

    uv_mutex_unlock(&siridb->shards_mutex);

    if (new_shard == NULL)
    {
        /* creating the new shard has failed, we exit here so the mutex is
         * is unlocked. (signal is raised)
         */
        return -1;
    }

    /* at this point the references should be as following (unless dropped):
     *  shard->ref (2)
     *      - simple list
     *      - new_shard->replacing
     *  new_shard->ref (2)
     *      - siridb->shards
     *      - this method
     */

    sleep(1);

    uv_mutex_lock(&siridb->series_mutex);

    slist_t * slist = imap_2slist_ref(siridb->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    if (slist == NULL)
    {
        return -1;  /* signal is raised */
    }

    sleep(1);

    for (size_t i = 0; i < slist->len; i++)
    {
        /* its possible that another database is paused, but we wait anyway */
        if (siri.optimize->pause)
        {
            siri_optimize_wait();
        }

        series = slist->data[i];

        /* TODO: get correct function based on shard and time precision */

        if (    !siri_err &&
                siri.optimize->status != SIRI_OPTIMIZE_CANCELLED &&
                shard->id % siridb->duration_num == series->mask &&
                (~series->flags & SIRIDB_SERIES_IS_DROPPED) &&
                (~new_shard->flags & SIRIDB_SHARD_IS_REMOVED))
        {
            uv_mutex_lock(&siridb->series_mutex);

            if (    (~new_shard->flags & SIRIDB_SHARD_IS_REMOVED) &&
                    siridb_series_optimize_shard_num32(
                            siridb,
                            series,
                            new_shard))
            {
                log_critical(
                        "Optimizing shard '%s' has failed due to a critical "
                        "error", shard->fn);
            }

            uv_mutex_unlock(&siridb->series_mutex);

            /* make this sleep depending on the active handles */
            usleep( siri.loop->active_handles *
                    siri.loop->active_handles * 50);
        }

        siridb_series_decref(series);
    }

    slist_free(slist);

    if (new_shard->flags & SIRIDB_SHARD_IS_REMOVED)
    {
        log_warning(
                "Cancel optimizing shard '%s' because the shard is dropped",
                new_shard->fn);
        siridb_shard_decref(new_shard);
        return siri_err;
    }

    /* this is a good time to copy the file name */
    char * tmp = strdup(new_shard->replacing->fn);

    if (tmp == NULL)
    {
        ERR_ALLOC
    }

    if (siri_err || siri.optimize->status == SIRI_OPTIMIZE_CANCELLED)
    {
        /*
         * Error occurred or the optimize task is cancelled. By decrementing
         * only the reference counter for the new_shard we keep this shard as
         * if it is still optimizing so remaining points can still be written.
         */
        free(tmp);
        siridb_shard_decref(new_shard);
        return siri_err;
    }



    sleep(1);

    uv_mutex_lock(&siridb->series_mutex);

    /* make sure both shards files are closed */
    siri_fp_close(new_shard->replacing->fp);
    siri_fp_close(new_shard->fp);

    /*
     * Closing files or writing to the new shard might have produced
     * critical errors. This seems to be a good point to check for errors.
     */
    if (!siri_err)
    {
        /* remove the old shard file, this is not critical */
        unlink(new_shard->replacing->fn);

        /* rename the temporary shard file to the correct shard filename */
        if (rename(new_shard->fn, new_shard->replacing->fn))
        {
            free(tmp);
            log_critical(
                    "Could not rename file '%s' to '%s'",
                    new_shard->fn,
                    new_shard->replacing->fn);
            ERR_FILE
        }
        else
        {
            /* free the original allocated memory and set the new filename */
            free(new_shard->fn);
            new_shard->fn = tmp;

            /* decrement reference to old shard and set
             * new_shard->replacing to NULL
             */
            siridb_shard_decref(new_shard->replacing);
            new_shard->replacing = NULL;
        }
    }
    else
    {
        free(tmp);
    }

    uv_mutex_unlock(&siridb->series_mutex);

    /* can raise an error only if the shard is dropped, in any other case we
     * still have a reference left and an error cannot be raised.
     */
    siridb_shard_decref(new_shard);

    sleep(1);
    return siri_err;
}

/*
 * Returns 0 if successful or EOF in case of an error.
 */
int siridb_shard_write_flags(siridb_shard_t * shard)
{
    if (shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, shard->fp, shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip writing status",
                    shard->fn);
            return EOF;
        }
    }
    return (fseeko(shard->fp->fp, HEADER_FLAGS, SEEK_SET) ||
            fputc(shard->flags, shard->fp->fp) == EOF ||
            fflush(shard->fp->fp)) ? EOF : 0;
}

/*
 * Increment the shard reference counter.
 */
inline void siridb_shard_incref(siridb_shard_t * shard)
{
    shard->ref++;
}

/*
 * Decrement the reference counter, when 0 the shard will be destroyed.
 *
 * In case the shard will be destroyed and flag SIRIDB_SHARD_WILL_BE_REMOVED
 * is set, the file will be removed.
 *
 * A signal can be raised in case closing the shard file fails.
 */
inline void siridb_shard_decref(siridb_shard_t * shard)
{
    if (!--shard->ref)
    {
        SHARD_free(shard);
    }
}

/*
 * Returns 0 when successful or a negative value in case of an error.
 */
int siridb_shard_remove(siridb_shard_t * shard)
{
    int rc = 0;

    if (shard->replacing != NULL)
    {
        rc = siridb_shard_remove(shard->replacing);
    }

    siri_fp_close(shard->fp);

    rc += unlink(shard->fn);

    if (rc == 0)
    {
        log_debug("Shard file removed: %s", shard->fn);
    }
    else
    {
        log_critical(
                "Cannot remove shard file: %s (error code: %d)",
                shard->fn,
                rc);
    }

    return rc;
}

/*
 * Destroy shard.
 * When flag SIRIDB_SHARD_WILL_BE_REMOVED is set, the file will be removed.
 *
 * A signal can be raised in case closing the shard file fails.
 */
static void SHARD_free(siridb_shard_t * shard)
{
    if (shard->replacing != NULL)
    {
        /* in case shard->replacing is set we also need to free this shard */
        siridb_shard_decref(shard->replacing);
    }

    /* this will close the file, even when other references exist */
    siri_fp_decref(shard->fp);

    /* check if we need to remove the shard file */
    if (shard->flags & SIRIDB_SHARD_IS_REMOVED)
    {
        int rc;
        rc = unlink(shard->fn);
        if (rc == 0)
        {
            log_debug("Shard file removed: %s", shard->fn);
        }
        else
        {
            log_critical(
                    "Cannot remove shard file: %s (error code: %d)",
                    shard->fn,
                    rc);
        }
    }

#ifdef DEBUG
    log_debug("Free shard id: %lu", shard->id);
#endif

    free(shard->fn);
    free(shard);
}

/*
 * Returns 0 if successful or -1 in case of an error.
 *
 * A SIGNAL might be raised in case of memory errors. We mark the shard as
 * corrupt in case of disk errors and try to recover on the next optimize
 * cycle.
 */
static int SHARD_load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp)
{
    char idx[IDX_NUM32_SZ];
    siridb_series_t * series;
    uint16_t len;
    uint32_t series_id;
    char * pt;
    int rc;
    long int pos;
    size_t size;

    while ((size = fread(&idx, 1, IDX_NUM32_SZ, fp)) == IDX_NUM32_SZ)
    {
        pt = idx;
        series_id = *((uint32_t *) pt);
        len = *((uint16_t *) (idx + 12));
        pos = ftello(fp);

        if (pos < 0)
        {
            log_error("Cannot read position in shard %lu (%s). Mark this shard "
                    "as corrupt. The next optimize cycle will most likely fix "
                    "this shard but you might loose some data.",
                    shard->id,
                    shard->fn);
            shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
            return -1;
        }

        series = imap_get(siridb->series_map, series_id);

        if (series == NULL)
        {
            if (!series_id || series_id > siridb->max_series_id)
            {
                log_error(
                        "Unexpected Series ID %u is found in shard %lu (%s) at "
                        "position %d. This indicates that this shard is "
                        "probably corrupt. The next optimize cycle will most "
                        "likely fix this shard but you might loose some data.",
                        series_id,
                        shard->id,
                        shard->fn,
                        pos);
                shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
                return -1;
            }

            /* this shard has remove series, make sure the flag is set */
            if (!(shard->flags & SIRIDB_SHARD_HAS_DROPPED_SERIES))
            {
                log_debug(
                        "At least Series ID %u is found in shard %lu (%s) but "
                        "does not exist anymore. We will remove the series on "
                        "the next optimize cycle.",
                        series_id,
                        shard->id,
                        shard->fn);
                shard->flags |= SIRIDB_SHARD_HAS_DROPPED_SERIES;
            }
        }
        else
        {
            if (siridb_series_add_idx_num32(
                    series,
                    shard,
                    *((uint32_t *) (idx + 4)),
                    *((uint32_t *) (idx + 8)),
                    (uint32_t) pos,
                    len) == 0)
            {
                /* update the series length property */
                series->length += len;
            }
            else
            {
                /* signal is raised */
                log_critical("Cannot load index for Series ID %u", series->id);
            }
        }

        rc = fseeko(fp, len * 12, SEEK_CUR);
        if (rc != 0)
        {
            log_error("Seek error in shard %lu (%s) at position %ld. Mark this "
                    "shard as corrupt. The next optimize cycle will most "
                    "likely fix this shard but you might loose some data.",
                    shard->id,
                    shard->fn,
                    pos);
            shard->flags |= SIRIDB_SHARD_IS_CORRUPT;
            return -1;
        }
    }
    if (size != 0)
    {
        log_debug("Size: %lu", size);
    }
    return siri_err;
}

/*
 * Set shard->fn to the correct file name.
 *
 * Returns the length of 'fn' or a negative value in case of an error.
 */
inline static int SHARD_init_fn(siridb_t * siridb, siridb_shard_t * shard)
{
     return asprintf(
             &shard->fn,
             "%s%s%s%ld%s",
             siridb->dbpath,
             SIRIDB_SHARDS_PATH,
             (shard->replacing == NULL) ? "" : "__",
             shard->id,
             ".sdb");
}
