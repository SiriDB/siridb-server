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
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <imap64/imap64.h>
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

/* 0    (uint64_t)  DURATION
 * 8    (uint8_t)   SHEMA
 * 9    (uint8_t)   TP
 * 10   (uint8_t)   TIME_PRECISION
 * 11   (uint8_t)   STATUS
 */
#define HEADER_SIZE 12

#define HEADER_SCHEMA 8
#define HEADER_TP 9
#define HEADER_STATUS 11

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

static int SHARD_load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp);

static void SHARD_free(siridb_shard_t * shard);
static void SHARD_create_slist(siridb_series_t * series, slist_t * slist);

static void init_fn(siridb_t * siridb, siridb_shard_t * shard)
{
    char fn[PATH_MAX];
    sprintf(fn,
            "%s%s%s%ld%s",
            siridb->dbpath,
            SIRIDB_SHARDS_PATH,
            (shard->replacing == NULL) ? "" : "__",
            shard->id,
            ".sdb");
    shard->fn = strdup(fn);
}

int siridb_shard_load(siridb_t * siridb, uint64_t id)
{
    siridb_shard_t * shard;
    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->fp = siri_fp_new();
    shard->id = id;
    shard->ref = 1;
    shard->replacing = NULL;
    init_fn(siridb, shard);
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
    shard->flags = (uint8_t) header[HEADER_STATUS] | SIRIDB_SHARD_IS_LOADING;

    SHARD_load_idx_num32(siridb, shard, fp);

    fclose(fp);

    imap64_add(siridb->shards, id, shard);

    /* remove LOADING flag from shard status */
    shard->flags ^= SIRIDB_SHARD_IS_LOADING & shard->flags;

    return 0;
}

siridb_shard_t *  siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp,
        siridb_shard_t * replacing)
{
    siridb_shard_t * shard;

    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->fp = siri_fp_new();
    shard->id = id;
    shard->ref = 1;
    shard->flags = SIRIDB_SHARD_OK;
    shard->tp = tp;
    shard->replacing = replacing;
    FILE * fp;
    init_fn(siridb, shard);

    if ((fp = fopen(shard->fn, "w")) == NULL)
    {
        siridb_shard_decref(shard);
        log_critical("Cannot create shard file: '%s'", shard->fn);
        return NULL;
    }

    fwrite(&duration, sizeof(uint64_t), 1, fp);
    fputc(SIRIDB_SHARD_SHEMA, fp);
    fputc(tp, fp);
    fputc(siridb->time->precision, fp);
    fputc(shard->flags, fp);

    fclose(fp);

    imap64_add(siridb->shards, id, shard);

    /* this is not critical at this point */
    siri_fopen(siri.fh, shard->fp, shard->fn, "r+");

    return shard;
}

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
            log_critical(
                    "Cannot open file '%s', skip writing points",
                    shard->fn);
            return -1;
        }
    }
    fp = shard->fp->fp;

    fseek(fp, 0, SEEK_END);
    fwrite(&series->id, sizeof(uint32_t), 1, fp);
    fwrite(&points->data[start].ts, siridb->time->ts_sz, 1, fp);
    fwrite(&points->data[end - 1].ts, siridb->time->ts_sz, 1, fp);
    fwrite(&len, sizeof(uint16_t), 1, fp);

    pos = ftell(fp);

    /* TODO: this works for both double and integer.
     * Add size values for strings and write string using 'old' way
     */
    for (i = start; i < end; i++)
    {
        fwrite(&points->data[i].ts, siridb->time->ts_sz, 1, fp);
        fwrite(&points->data[i].val, 8, 1, fp);
    }

    fflush(fp);

    return pos;
}

int siridb_shard_get_points_num32(
        siridb_points_t * points,
        idx_num32_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap)
{
    size_t len = points->len + idx->len;
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

    fseek(idx->shard->fp->fp, idx->pos, SEEK_SET);
    if (fread(
            temp,
            12,
            idx->len,
            idx->shard->fp->fp) != idx->len)
    {
        if (idx->shard->flags & (
                SIRIDB_SHARD_IS_CORRUPT | SIRIDB_SHARD_WILL_BE_REPLACED))
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

void siridb_shard_optimize(siridb_shard_t * shard, siridb_t * siridb)
{
    if (    siri.optimize->status == SIRI_OPTIMIZE_CANCELLED ||
            shard->flags == SIRIDB_SHARD_OK ||
            (shard->flags & SIRIDB_SHARD_WILL_BE_REMOVED))
        return;

    siridb_shard_t * new_shard = NULL;

    uint64_t duration = (shard->tp == SIRIDB_POINTS_TP_STRING) ?
            siridb->duration_log : siridb->duration_num;
    siridb_series_t * series;

    log_info("Start optimizing shard id %lu (%u)", shard->id, shard->flags);

    uv_mutex_lock(&siridb->shards_mutex);

    if ((siridb_shard_t *) imap64_pop(siridb->shards, shard->id) == shard)
    {
        if ((new_shard = siridb_shard_create(
            siridb,
            shard->id,
            duration,
            shard->tp,
            shard)) == NULL)
        {
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
         * is unlocked.
         */
        return;
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

    slist_t * slist = slist_new(siridb->series_map->len);

    imap32_walk(
            siridb->series_map,
            (imap32_cb_t) &SHARD_create_slist,
            (void *) slist);

    uv_mutex_unlock(&siridb->series_mutex);

    sleep(1);

    for (size_t i = 0; i < slist->len; i++)
    {
        /* its possible that another database is paused, but we wait anyway */
        while (siri.optimize->status == SIRI_OPTIMIZE_PAUSED)
        {
            log_info("Optimize task is paused, wait for 30 seconds...");
            sleep(30);
        }

        series = slist->data[i];

        /* TODO: get correct function based on shard and time precision */

        if (    siri.optimize->status != SIRI_OPTIMIZE_CANCELLED &&
                shard->id % siridb->duration_num == series->mask)
        {
            uv_mutex_lock(&siridb->series_mutex);

            siridb_series_optimize_shard_num32(siridb, series, new_shard);

            uv_mutex_unlock(&siridb->series_mutex);

            usleep(10000);
        }

        siridb_series_decref(series);
    }

    slist_free(slist);

    sleep(1);

    uv_mutex_lock(&siridb->series_mutex);
    /* this will close the file but does not free the shard yet since we still
     * have a reference to the simple list.
     */
    siridb_shard_decref(new_shard->replacing);

    /* make sure the temporary shard file is also closed */
    siri_fp_close(new_shard->fp);

    /* remove the old shard file */
    unlink(new_shard->replacing->fn);

    /* rename the temporary shard file to the correct shard filename */
    rename(new_shard->fn, new_shard->replacing->fn);

    /* free the original allocated memory and set the correct filename */
    free(new_shard->fn);
    new_shard->fn = strdup(new_shard->replacing->fn);

    /* set replacing shard to NULL, reference is decremented earlier. */
    new_shard->replacing = NULL;

    uv_mutex_unlock(&siridb->series_mutex);

    log_info("Finished optimizing shard id %ld", new_shard->id);

    siridb_shard_decref(new_shard);

    sleep(1);
}

void siridb_shard_write_status(siridb_shard_t * shard)
{
    if (shard->fp->fp == NULL)
    {
        if (siri_fopen(siri.fh, shard->fp, shard->fn, "r+"))
        {
            log_critical(
                    "Cannot open file '%s', skip writing status",
                    shard->fn);
            return;
        }
    }
    fseek(shard->fp->fp, HEADER_STATUS, SEEK_SET);
    fputc(shard->flags, shard->fp->fp);
    fflush(shard->fp->fp);
}

inline void siridb_shard_incref(siridb_shard_t * shard)
{
    shard->ref++;
}

inline void siridb_shard_decref(siridb_shard_t * shard)
{
    if (!--shard->ref)
        SHARD_free(shard);
}

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
    if (shard->flags & SIRIDB_SHARD_WILL_BE_REMOVED)
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
    free(shard->fn);
    free(shard);
}

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
        pos = ftell(fp);

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

        series = imap32_get(siridb->series_map, series_id);

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
            if (!(shard->flags & SIRIDB_SHARD_HAS_REMOVED_SERIES))
            {
                log_debug(
                        "At least Series ID %u is found in shard %lu (%s) but "
                        "does not exist anymore. We will remove the series on "
                        "the next optimize cycle.",
                        series_id,
                        shard->id,
                        shard->fn);
                shard->flags |= SIRIDB_SHARD_HAS_REMOVED_SERIES;
            }
        }
        else
        {
            siridb_series_add_idx_num32(
                    series->index,
                    shard,
                    *((uint32_t *) (idx + 4)),
                    *((uint32_t *) (idx + 8)),
                    (uint32_t) pos,
                    len);

            /* update the series length property */
            series->length += len;
        }

        rc = fseek(fp, len * 12, SEEK_CUR);
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
    return 0;
}

static void SHARD_create_slist(siridb_series_t * series, slist_t * slist)
{
    siridb_series_incref(series);
    slist_append(slist, series);
}

