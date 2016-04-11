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
#include <siri/cfg/cfg.h>
#include <imap64/imap64.h>
#include <siri/db/series.h>
#include <logger/logger.h>
#include <stdio.h>
#include <siri/siri.h>
#include <ctree/ctree.h>

#define GET_FN                                                              \
/* we are sure this fits since the max possible length is checked */        \
char fn[SIRI_CFG_MAX_LEN_PATH];                                             \
sprintf(fn, "%s%s%ld%s", siridb->dbpath, SIRIDB_SHARDS_PATH, shard->id, ".sdb");

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

void siridb_free_shard(siridb_shard_t * shard)
{
    siri_decref_fp(shard->fp);
    free(shard);
}

static int load_idx_num32(
        siridb_t * siridb,
        siridb_shard_t * shard,
        FILE * fp)
{
    char idx[IDX_NUM32_SZ];
    siridb_series_t * series;

    while (fread(&idx, IDX_NUM32_SZ, 1, fp) == 1)
    {
        series = imap32_get(siridb->series_map, *((uint32_t *) idx));
        if (series == NULL)
        {
            if (!(shard->status & SIRIDB_SHARD_HAS_REMOVED_SERIES))
                log_debug(
                        "At least Series ID %d is found in shard %d but does "
                        "not exist anymore. We will remove the series on the "
                        "next optimize cycle.",
                        *((uint32_t *) idx),
                        shard->id);
            shard->status |= SIRIDB_SHARD_HAS_REMOVED_SERIES;
        }
        siridb_add_idx_num32(
                series->index,
                shard,
                *((uint32_t *) (idx + 4)),
                *((uint32_t *) (idx + 8)),
                *((uint16_t *) (idx + 12)));
        fseek(fp, *((uint16_t *) (idx + 12)) * 12, SEEK_CUR);
    }
    return 0;
}

int siridb_load_shard(siridb_t * siridb, uint64_t id)
{
    siridb_shard_t * shard;
    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->fp = siri_new_fp();
    shard->id = id;
    FILE * fp;

    /* we are sure this fits since the max possible length is checked */
    GET_FN
    if ((fp = fopen(fn, "r")) == NULL)
    {
        siridb_free_shard(shard);
        log_error("Cannot open file for reading: '%s'", fn);
        return -1;
    }

    char header[HEADER_SIZE];
    if (fread(&header, HEADER_SIZE, 1, fp) != 1)
    {
        siridb_free_shard(shard);
        log_critical("Missing header in shard file: '%s'", fn);
        return -1;
    }
    shard->tp = (uint8_t) header[HEADER_TP];

    load_idx_num32(siridb, shard, fp);

    fclose(fp);

    imap64_add(siridb->shards, id, shard);

    return 0;
}

siridb_shard_t *  siridb_create_shard(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp)
{
    siridb_shard_t * shard;

    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->fp = siri_new_fp();
    shard->id = id;
    shard->tp = tp;
    shard->status = 0;
    FILE * fp;

    GET_FN
    if ((fp = fopen(fn, "w")) == NULL)
    {
        free(shard);
        perror("Error");
        log_critical("Cannot create shard file: '%s'", fn);
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
    siri_fopen(siri.fh, shard->fp, fn, "r+");

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
        GET_FN
        if (siri_fopen(siri.fh, shard->fp, fn, "r+"))
        {
            log_critical("Cannot open file '%s', skip writing points", fn);
            return -1;
        }
    }
    fp = shard->fp->fp;

    fseek(fp, 0, SEEK_END);
    fwrite(&series->id, sizeof(uint32_t), 1, fp);
    log_debug("Write: start ts %d, end ts %d, start %d, end %d",
            points->data[start].ts,
            points->data[end - 1].ts,
            start, end);
    fwrite(&points->data[start].ts, siridb->time->ts_sz, 1, fp);
    fwrite(&points->data[end - 1].ts, siridb->time->ts_sz, 1, fp);
    fwrite(&len, sizeof(uint16_t), 1, fp);

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
