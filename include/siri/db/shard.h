/*
 * shard.h - SiriDB Shard.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-04-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <siri/db/points.h>
#include <siri/db/series.h>
#include <siri/file/handler.h>

#define SIRIDB_SHARD_OK 0
#define SIRIDB_SHARD_MANUAL_OPTIMIZE 1
#define SIRIDB_SHARD_HAS_OVERLAP 2
#define SIRIDB_SHARD_HAS_NEW_VALUES 4
#define SIRIDB_SHARD_HAS_REMOVED_SERIES 8
#define SIRIDB_SHARD_WILL_BE_REMOVED 16
#define SIRIDB_SHARD_WILL_BE_REPLACED 32
#define SIRIDB_SHARD_IS_LOADING 64

typedef struct siridb_s siridb_t;
typedef struct siridb_points_s siridb_points_t;
typedef struct siridb_series_s siridb_series_t;
typedef struct idx_num32_s idx_num32_t;
typedef struct idx_num64_s idx_num64_t;

typedef struct siridb_shard_s siridb_shard_t;

typedef struct siridb_shard_s
{
    uint64_t id;
    uint8_t tp; /* SIRIDB_SERIES_TP_INT, ...DOUBLE or ...STRING */
    uint8_t status;
    uint8_t ref;
    siri_fp_t * fp;
    char * fn;
    siridb_shard_t * replacing;
} siridb_shard_t;

siridb_shard_t * siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp,
        siridb_shard_t * replacing);

int siridb_shard_load(siridb_t * siridb, uint64_t id);

void siridb_shard_incref(siridb_shard_t * shard);
void siridb_shard_decref(siridb_shard_t * shard);

long int siridb_shard_write_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end);

int siridb_shard_get_points_num32(
        siridb_points_t * points,
        idx_num32_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);

int siridb_shard_get_points_num64(
        siridb_t * siridb,
        siridb_points_t * points,
        idx_num64_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);

void siridb_shard_optimize(siridb_shard_t * shard, siridb_t * siridb);
void siridb_shard_write_status(siridb_shard_t * shard);
