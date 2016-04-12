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
#include <siri/db/buffer.h>
#include <qpack/qpack.h>

struct siridb_s;
struct siridb_buffer_s;

#define SIRIDB_SERIES_TP_INT 0
#define SIRIDB_SERIES_TP_DOUBLE 1
#define SIRIDB_SERIES_TP_STRING 2

#define SIRIDB_SERIES_ISNUM(SERIES) (SERIES->tp != SIRIDB_SERIES_TP_STRING)

#define SIRIDB_QP_MAP2_TP(TP)                                               \
    (TP == QP_INT64) ? SIRIDB_SERIES_TP_INT :                               \
    (TP == QP_DOUBLE) ? SIRIDB_SERIES_TP_DOUBLE : SIRIDB_SERIES_TP_STRING


typedef struct idx_num32_s
{
    uint32_t start_ts;
    uint32_t end_ts;
    struct siridb_shard_s * shard;
    uint32_t pos;
    uint16_t len;
} idx_num32_t;

typedef struct idx_num64_s
{
    uint64_t start_ts;
    uint64_t end_ts;
    struct siridb_shard_s * shard;
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
    uint8_t tp;
    uint16_t mask;
    struct siridb_buffer_s * buffer;
    siridb_series_idx_t * index;
} siridb_series_t;

void siridb_add_idx_num32(
        siridb_series_idx_t * index,
        struct siridb_shard_s * shard,
        uint32_t start_ts,
        uint32_t end_ts,
        uint32_t pos,
        uint16_t len);

void siridb_add_idx_num64(
        siridb_series_idx_t * index,
        struct siridb_shard_s * shard,
        uint64_t start_ts,
        uint64_t end_ts,
        uint32_t pos,
        uint16_t len);

void siridb_free_series(siridb_series_t * series);

int siridb_load_series(struct siridb_s * siridb);

siridb_series_t * siridb_create_series(
        struct siridb_s * siridb,
        const char * series_name,
        uint8_t tp);

void siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val);
