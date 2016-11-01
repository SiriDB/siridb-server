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
#include <siri/db/pcache.h>
#include <siri/db/buffer.h>
#include <qpack/qpack.h>
#include <cexpr/cexpr.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_buffer_s siridb_buffer_t;
typedef struct siridb_points_s siridb_points_t;
typedef struct siridb_shard_s siridb_shard_t;

typedef points_tp series_tp;

/* Series Flags */
#define SIRIDB_SERIES_HAS_OVERLAP 1
#define SIRIDB_SERIES_IS_DROPPED 2
#define SIRIDB_SERIES_INIT_REPL 4
#define SIRIDB_SERIES_IS_SERVER_ONE 8 	/* if not set its server_id 0 */
#define SIRIDB_SERIES_IS_32BIT_TS 16    /* if not set its a 64 bit ts */
#define SIRIDB_SERIES_IS_LOG 32         /* if not set its numeric (int/real) */

/* the max length including terminator char */
#define SIRIDB_SERIES_NAME_LEN_MAX 65535

#define siridb_series_isnum(series) (series->tp != TP_STRING)

#define SIRIDB_QP_MAP2_TP(TP)                                 \
    (TP == QP_INT64) ? TP_INT :                               \
    (TP == QP_DOUBLE) ? TP_DOUBLE : TP_STRING

extern const char series_type_map[3][8];

typedef struct idx_s
{
    siridb_shard_t * shard;
    uint32_t pos;
    uint16_t len;
    uint16_t log_sz;  /* reserved for log values */
    uint64_t start_ts;
    uint64_t end_ts;
} idx_t;

typedef struct siridb_series_s
{
    uint32_t ref;  /* keep ref on top */
    uint32_t id;
    uint16_t mask;
    uint16_t pool;
    uint16_t name_len;
    uint8_t flags;
    uint8_t tp;
    uint64_t start;
    uint64_t end;
    uint32_t length;
    uint32_t idx_len;
    char * name;
    idx_t * idx;
    siridb_buffer_t * buffer;
} siridb_series_t;

int siridb_series_load(siridb_t * siridb);

siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp);

int siridb_series_add_idx(
        siridb_series_t * series,
        siridb_shard_t * shard,
        uint64_t start_ts,
        uint64_t end_ts,
        uint32_t pos,
        uint16_t len);

int siridb_series_add_point(
        siridb_t * siridb,
        siridb_series_t * series,
        uint64_t * ts,
        qp_via_t * val);

int siridb_series_add_pcache(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_pcache_t * pcache);

siridb_points_t * siridb_series_get_points(
        siridb_series_t * series,
        uint64_t * start_ts,
        uint64_t * end_ts);

void siridb_series_remove_shard(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard);

int siridb_series_optimize_shard(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard);


void siridb_series_update_props(siridb_t * siridb, siridb_series_t * series);
int siridb_series_cexpr_cb(siridb_series_t * series, cexpr_condition_t * cond);
int siridb_series_replicate_file(siridb_t * siridb);
int siridb_series_drop(siridb_t * siridb, siridb_series_t * series);
void siridb_series_drop_prepare(siridb_t * siridb, siridb_series_t * series);
int siridb_series_drop_commit(siridb_t * siridb, siridb_series_t * series);
int siridb_series_flush_dropped(siridb_t * siridb);
uint8_t siridb_series_server_id_by_name(const char * name);
int siridb_series_open_store(siridb_t * siridb);
void siridb__series_free(siridb_series_t * series);
void siridb__series_decref(siridb_series_t * series);
/*
 * Increment the series reference counter.
 */
#define siridb_series_incref(series) series->ref++

/*
 * Decrement reference counter for series and free the series when zero is
 * reached.
 */
#define siridb_series_decref(series__) \
		if (!--series__->ref) siridb__series_free(series__)


#define siridb_series_server_id(series) \
((series->flags & SIRIDB_SERIES_IS_SERVER_ONE) == SIRIDB_SERIES_IS_SERVER_ONE)
