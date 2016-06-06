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

static int load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp);

static void shard_free(siridb_shard_t * shard);

static void init_fn(siridb_t * siridb, siridb_shard_t * shard)
{
    char fn[PATH_MAX];
    sprintf(fn,
            "%s%s%ld%s",
            siridb->dbpath,
            SIRIDB_SHARDS_PATH,
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
        siridb_shard_decref(shard);
        log_critical("Missing header in shard file: '%s'", shard->fn);
        return -1;
    }
    shard->tp = (uint8_t) header[HEADER_TP];
    shard->status = (uint8_t) header[HEADER_STATUS];

    load_idx_num32(siridb, shard, fp);
    fclose(fp);

    imap64_add(siridb->shards, id, shard);
    return 0;
}

siridb_shard_t *  siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp)
{
    siridb_shard_t * shard;

    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->fp = siri_fp_new();
    shard->id = id;
    shard->tp = tp;
    shard->status = 0;
    FILE * fp;
    init_fn(siridb, shard);

    if ((fp = fopen(shard->fn, "w")) == NULL)
    {
        free(shard);
        perror("Error");
        log_critical("Cannot create shard file: '%s'", shard->fn);
        return NULL;
    }

    fwrite(&duration, sizeof(uint64_t), 1, fp);
    fputc(SIRIDB_SHARD_SHEMA, fp);
    fputc(tp, fp);
    fputc(siridb->time->precision, fp);
    fputc(shard->status, fp);

    fclose(fp);

    imap64_add(siridb->shards, id, shard);

    /* this is not critical at this point */
    siri_fopen(siri.fh, shard->fp, shard->fn, "r+");

    return shard;
}

int siridb_shard_write_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard,
        siridb_points_t * points,
        uint32_t start,
        uint32_t end)
{
    FILE * fp;
    uint16_t len = end - start;

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

    /* TODO: Add index for 32 and 64 bit time-stamps, number and log values */
    siridb_series_add_idx_num32(
            series->index,
            shard,
            (uint32_t) points->data[start].ts,
            (uint32_t) points->data[end - 1].ts,
            (uint32_t) ftell(fp),
            len);

    /* TODO: this works for both double and integer.
     * add size values for strings and write string using 'old' way
     */
    for (; start < end; start++)
    {
        fwrite(&points->data[start].ts, siridb->time->ts_sz, 1, fp);
        fwrite(&points->data[start].val, 8, 1, fp);
    }

    return 0;
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
        log_critical("Cannot read from shard id: %ld", idx->shard->id);
        return -1;
    }
    /* set pointer to start */
    pt = temp;

    /* crop from start if needed */
    if (start_ts != NULL)
        for (; *pt < *start_ts; pt += 3, len--);

    /* crop from end if needed */
    if (end_ts != NULL)
        for (   uint32_t * p = temp + 3 * (idx->len - 1);
                *p >= *end_ts;
                p -= 3, len--);

    if (    has_overlap &&
            points->len &&
            (idx->shard->status & SIRIDB_SHARD_HAS_OVERLAP))
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
            (shard->status & (SIRIDB_SHARD_OK | SIRIDB_SHARD_WILL_BE_REMOVED)))
        return;
    siridb_shard_incref(shard);
    log_info("Start optimizing shard id %ld (%d)", shard->id, shard->status);

    log_info("Finished optimizing shard id %ld", shard->id);
    siridb_shard_decref(shard);

    sleep(1);

}

void siridb_shard_write_status(siridb_t * siridb, siridb_shard_t * shard)
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
    fputc(shard->status, shard->fp->fp);
}

inline void siridb_shard_incref(siridb_shard_t * shard)
{
    shard->ref++;
}

inline void siridb_shard_decref(siridb_shard_t * shard)
{
    if (!--shard->ref)
        shard_free(shard);
}

static void shard_free(siridb_shard_t * shard)
{
    log_debug("FREE SHARD!");

    /* this will close the file, even when other references exist */
    siri_fp_decref(shard->fp);

    /* check if we need to remove the shard file */
    if (shard->status & SIRIDB_SHARD_WILL_BE_REMOVED)
    {
        int rc;
        rc = remove(shard->fn);
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

static int load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp)
{
    char idx[IDX_NUM32_SZ];
    siridb_series_t * series;
    uint16_t len;

    while (fread(&idx, IDX_NUM32_SZ, 1, fp) == 1)
    {
        char * pt = idx;

        series = imap32_get(siridb->series_map, *((uint32_t *) pt));

        if (series == NULL)
        {
            if (!(shard->status & SIRIDB_SHARD_HAS_REMOVED_SERIES))
                log_debug(
                        "At least Series ID %d is found in shard %d but does "
                        "not exist anymore. We will remove the series on the "
                        "next optimize cycle.",
                        *((uint32_t *) pt),
                        shard->id);
            shard->status |= SIRIDB_SHARD_HAS_REMOVED_SERIES;
        }
        else
        {
            siridb_series_add_idx_num32(
                    series->index,
                    shard,
                    *((uint32_t *) (idx + 4)),
                    *((uint32_t *) (idx + 8)),
                    (uint32_t) ftell(fp),
                    (len = *((uint16_t *) (idx + 12))));
        }
        fseek(fp, len * 12, SEEK_CUR);
    }
    return 0;
}


