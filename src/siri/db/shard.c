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


#define GET_FN                                                              \
/* we are sure this fits since the max possible length is checked */        \
char fn[SIRI_CFG_MAX_LEN_PATH];                                             \
sprintf(fn, "%s%s%ld%s", siridb->dbpath, SIRIDB_SHARDS_PATH, id, ".sdb");

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


void siridb_free_shard(siridb_shard_t * shard)
{
    if (shard->fp != NULL)
    {
        fclose(shard->fp);
        shard->fp = NULL;
    }
    free(shard);
}

int siridb_load_shard(siridb_t * siridb, uint64_t id)
{
    siridb_shard_t * shard;
    shard = (siridb_shard_t *) malloc(sizeof(siridb_shard_t));
    shard->id = id;

    /* we are sure this fits since the max possible length is checked */
    GET_FN
    if ((shard->fp = fopen(fn, "r")) == NULL)
    {
        siridb_free_shard(shard);
        log_error("Cannot open file for reading: '%s'", fn);
        return -1;
    }

    char header[HEADER_SIZE];
    if (fread(&header, HEADER_SIZE, 1, shard->fp) != 1)
    {
        siridb_free_shard(shard);
        log_critical("Missing header in shard file: '%s'", fn);
        return -1;
    }
    shard->tp = (uint8_t) header[HEADER_TP];

    fclose(shard->fp);
    shard->fp = NULL;

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
    shard->id = id;
    shard->tp = tp;
    shard->status = 0;

    GET_FN
    if ((shard->fp = fopen(fn, "w")) == NULL)
    {
        free(shard);
        perror("Error");
        log_critical("Cannot create shard file: '%s'", fn);
        return NULL;
    }

    fwrite(&duration, sizeof(uint64_t), 1, shard->fp);
    fputc(SIRIDB_SHARD_SHEMA, shard->fp);
    fputc(tp, shard->fp);
    fputc(siridb->time_precision, shard->fp);
    fputc(shard->status, shard->fp);

    fclose(shard->fp);
    shard->fp = NULL;

    imap64_add(siridb->shards, id, shard);

    return shard;
}

int siridb_shard_write_points(
        struct siridb_s * siridb,
        siridb_shard_t * shard,
        struct siridb_points_s * points,
        uint32_t start,
        uint32_t end)
{
    if (shard->fp == NULL)
    {

    }
    return 0;
}
