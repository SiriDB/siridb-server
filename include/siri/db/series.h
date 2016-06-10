/*
 * series.h - Series
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 29-03-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <siri/db/db.h>
#include <siri/db/points.h>
#include <siri/db/buffer.h>
#include <qpack/qpack.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_buffer_s siridb_buffer_t;
typedef struct siridb_points_s siridb_points_t;
typedef struct siridb_shard_s siridb_shard_t;

#define SIRIDB_SERIES_TP_INT SIRIDB_POINTS_TP_INT
#define SIRIDB_SERIES_TP_DOUBLE SIRIDB_POINTS_TP_DOUBLE
#define SIRIDB_SERIES_TP_STRING SIRIDB_POINTS_TP_STRING

#define SIRIDB_SERIES_ISNUM(SERIES) (SERIES->tp != SIRIDB_SERIES_TP_STRING)

#define SIRIDB_QP_MAP2_TP(TP)                                               \
    (TP == QP_INT64) ? SIRIDB_SERIES_TP_INT :                               \
    (TP == QP_DOUBLE) ? SIRIDB_SERIES_TP_DOUBLE : SIRIDB_SERIES_TP_STRING

extern const char series_type_map[3][8];

typedef struct idx_num32_s
{
    uint32_t start_ts;
    uint32_t end_ts;
    siridb_shard_t * shard;
    uint32_t pos;
    uint16_t len;
} idx_num32_t;

typedef struct idx_num64_s
{
    uint64_t start_ts;
    uint64_t end_ts;
    siridb_shard_t * shard;
    uint32_t pos;
    uint16_t len;
} idx_num64_t;

typedef struct siridb_series_idx_s
{
    uint32_t len;
    uint8_t has_overlap;
    void * idx;
} siridb_series_idx_t;

typedef struct siridb_series_s
{
    uint32_t id;
    uint16_t ref;
    uint16_t mask;
    uint64_t start;
    uint64_t end;
    siridb_buffer_t * buffer;
    siridb_series_idx_t * index;
    uint32_t length;
    uint8_t tp;
} siridb_series_t;

int siridb_series_load(siridb_t * siridb);

siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp);

void siridb_series_incref(siridb_series_t * series);
void siridb_series_decref(siridb_series_t * series);

void siridb_series_add_idx_num32(
        siridb_series_idx_t * index,
        siridb_shard_t * shard,
        uint32_t start_ts,
        uint32_t end_ts,
        uint32_t pos,
        uint16_t len);

void siridb_series_add_idx_num64(
        siridb_series_idx_t * index,
        siridb_shard_t * shard,
        uint64_t start_ts,
        uint64_t end_ts,
        uint32_t pos,
        uint16_t len);

void siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val);

siridb_points_t * siridb_series_get_points_num32(
        siridb_series_t * series,
        uint64_t * start_ts,
        uint64_t * end_ts);

void siridb_series_remove_shard_num32(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard);

void siridb_series_optimize_shard_num32(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard);

void siridb_series_update_props(siridb_series_t * series, void * args);

