/*
 * shard.h - SiriDB shard file.
 */
#ifndef SIRIDB_SHARD_H_
#define SIRIDB_SHARD_H_



/* flags */
#define SIRIDB_SHARD_OK 0
#define SIRIDB_SHARD_HAS_INDEX 1
#define SIRIDB_SHARD_HAS_OVERLAP 2
#define SIRIDB_SHARD_HAS_NEW_VALUES 4
#define SIRIDB_SHARD_HAS_DROPPED_SERIES 8
#define SIRIDB_SHARD_IS_REMOVED 16
#define SIRIDB_SHARD_IS_LOADING 32
#define SIRIDB_SHARD_IS_CORRUPT 64
#define SIRIDB_SHARD_IS_COMPRESSED 128

/* HAS_OVERLAP + HAS_NEW_VALUES + HAS_DROPPED_SERIES + IS_CORRUPT   */
#define SIRIDB_SHARD_NEED_OPTIMIZE 78

/* types */
#define SIRIDB_SHARD_TP_NUMBER 0
#define SIRIDB_SHARD_TP_LOG 1

#define SIRIDB_SHARD_STATUS_STR_MAX 128

extern const char shard_type_map[2][7];

typedef struct siridb_shard_flags_repr_s siridb_shard_flags_repr_t;
typedef struct siridb_shard_s siridb_shard_t;
typedef struct siridb_shard_view_s siridb_shard_view_t;

#include <stdio.h>
#include <siri/db/db.h>
#include <siri/db/points.h>
#include <siri/db/series.h>
#include <siri/file/handler.h>

siridb_shard_t * siridb_shard_create(
        siridb_t * siridb,
        uint64_t id,
        uint64_t duration,
        uint8_t tp,
        siridb_shard_t * replacing);
int siridb_shard_cexpr_cb(
        siridb_shard_view_t * vshard,
        cexpr_condition_t * cond);
int siridb_shard_status(char * str, siridb_shard_t * shard);
int siridb_shard_load(siridb_t * siridb, uint64_t id);
void siridb_shard_drop(siridb_shard_t * shard, siridb_t * siridb);
size_t siridb_shard_write_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_shard_t * shard,
        siridb_points_t * points,
        uint_fast32_t start,
        uint_fast32_t end,
        FILE * idx_fp,
        uint16_t * cinfo);
typedef int (*siridb_shard_get_points_cb)(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_num32(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_num64(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_log32(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_log64(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_num_compressed(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_get_points_log_compressed(
        siridb_points_t * points,
        idx_t * idx,
        uint64_t * start_ts,
        uint64_t * end_ts,
        uint8_t has_overlap);
int siridb_shard_optimize(siridb_shard_t * shard, siridb_t * siridb);
void siridb__shard_free(siridb_shard_t * shard);
void siridb__shard_decref(siridb_shard_t * shard);

struct siridb_shard_flags_repr_s
{
    const char * repr;
    uint8_t flag;
};

struct siridb_shard_s
{
    uint32_t ref;   /* keep ref on top */
    uint8_t tp; /* TP_NUMBER, TP_LOG */
    uint8_t flags;
    uint16_t max_chunk_sz;
    uint64_t id;
    size_t len;
    size_t size;
    siri_fp_t * fp;
    char * fn;
    siridb_shard_t * replacing;
};

struct siridb_shard_view_s
{
    siridb_shard_t * shard;
    siridb_server_t * server;
    uint64_t start;
    uint64_t end;
};

static inline siridb_shard_get_points_cb siridb_shard_get_points_callback(
        uint8_t shard_flags,
        siridb_series_t * series)
{
    return shard_flags & SIRIDB_SHARD_IS_COMPRESSED ?
            (series->tp == TP_STRING ?
                    siridb_shard_get_points_log_compressed :
                    siridb_shard_get_points_num_compressed) :
            (series->tp == TP_STRING ?
                    (series->flags & SIRIDB_SERIES_IS_32BIT_TS ?
                            siridb_shard_get_points_log32 :
                            siridb_shard_get_points_log64) :
                    (series->flags & SIRIDB_SERIES_IS_32BIT_TS ?
                            siridb_shard_get_points_num32 :
                            siridb_shard_get_points_num64));
}

/*
 * Increment the shard reference counter.
 */
#define siridb_shard_incref(shard) shard->ref++

/*
 * Decrement the reference counter, when 0 the shard will be destroyed.
 *
 * In case the shard will be destroyed and flag SIRIDB_SHARD_WILL_BE_REMOVED
 * is set, the file will be removed.
 *
 * A signal can be raised in case closing the shard file fails.
 */
#define siridb_shard_decref(shard__)     \
        if (!--shard__->ref) siridb__shard_free(shard__)


#define siridb_shard_idx_file(Name__, Fn__)         \
        size_t Len__ = strlen(Fn__);                \
        char Name__[Len__ + 1];                     \
        memcpy(Name__, Fn__, Len__ - 3);            \
        memcpy(Name__ + Len__ - 3, "idx", 4)

#endif  /* SIRIDB_SHARD_H_ */
