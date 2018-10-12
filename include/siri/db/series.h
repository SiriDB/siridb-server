/*
 * series.c - SiriDB Time Series.
 *
 *
 * Info siridb->series_mutex:
 *
 *  Main thread:
 *      siridb->series_map :    read (no lock)      write (lock)
 *      series->idx :           read (lock)         write (lock)
 *
 *  Other threads:
 *      siridb->series_map :    read (lock)          write (not allowed)
 *      series->idx :           read (lock)         write (lock)
 *
 *  Note:   One exception to 'not allowed' are the free functions
 *          since they only run when no other references to the object exist.
 */
#ifndef SIRIDB_SERIES_H_
#define SIRIDB_SERIES_H_

/* Series Flags */
#define SIRIDB_SERIES_HAS_OVERLAP 1
#define SIRIDB_SERIES_IS_DROPPED 2
#define SIRIDB_SERIES_INIT_REPL 4
#define SIRIDB_SERIES_IS_SERVER_ONE 8     /* if not set its server_id 0 */
#define SIRIDB_SERIES_IS_32BIT_TS 16    /* if not set its a 64 bit ts */

/* the max length including terminator char */
#define SIRIDB_SERIES_NAME_LEN_MAX 65535

#define siridb_series_isnum(series) (series->tp != TP_STRING)

#define SIRIDB_QP_MAP2_TP(TP)                                 \
    (TP == QP_INT64) ? TP_INT :                               \
    (TP == QP_DOUBLE) ? TP_DOUBLE : TP_STRING

extern const char series_type_map[3][8];

typedef struct idx_s idx_t;
typedef struct siridb_series_s siridb_series_t;

#include <inttypes.h>

#include <siri/db/points.h>
typedef points_tp series_tp;

#include <siri/db/db.h>
#include <siri/db/pcache.h>
#include <siri/db/buffer.h>
#include <qpack/qpack.h>
#include <cexpr/cexpr.h>

/* order here matters since shard.h is using a full series definition */
struct siridb_series_s
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
    long int bf_offset;
    siridb_points_t * buffer;
    char * name;
    idx_t * idx;
    siridb_t * siridb;
};
#include <siri/db/shard.h>

int siridb_series_load(siridb_t * siridb);
siridb_series_t * siridb_series_new(
        siridb_t * siridb,
        const char * series_name,
        uint8_t tp);
int siridb_series_add_idx(
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard,
        uint64_t start_ts,
        uint64_t end_ts,
        uint32_t pos,
        uint16_t len,
        uint16_t cinfo);
int siridb_series_add_point(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        uint64_t * ts,
        qp_via_t * val);
int siridb_series_add_pcache(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_pcache_t *__restrict pcache);
siridb_points_t * siridb_series_get_points(
        siridb_series_t *__restrict series,
        uint64_t *__restrict start_ts,
        uint64_t *__restrict end_ts);
void siridb_series_remove_shard(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard);
int siridb_series_optimize_shard(
        siridb_t *__restrict siridb,
        siridb_series_t *__restrict series,
        siridb_shard_t *__restrict shard);
void siridb_series_update_props(siridb_t * siridb, siridb_series_t * series);
int siridb_series_cexpr_cb(siridb_series_t * series, cexpr_condition_t * cond);
int siridb_series_replicate_file(siridb_t * siridb);
int siridb_series_drop(siridb_t * siridb, siridb_series_t * series);
void siridb_series_drop_prepare(siridb_t * siridb, siridb_series_t * series);
int siridb_series_drop_commit(siridb_t * siridb, siridb_series_t * series);
int siridb_series_flush_dropped(siridb_t * siridb);
uint8_t siridb_series_server_id_by_name(const char * name);
int siridb_series_open_store(siridb_t * siridb);
void siridb__series_free(siridb_series_t *__restrict series);
void siridb__series_decref(siridb_series_t * series);
siridb_points_t * siridb_series_get_first(
        siridb_series_t * series, int * required_shard);
siridb_points_t * siridb_series_get_last(
        siridb_series_t * series, int * required_shard);
siridb_points_t * siridb_series_get_count(siridb_series_t * series);
void siridb_series_ensure_type(siridb_series_t * series, qp_obj_t * qp_obj);
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

struct idx_s
{
    siridb_shard_t * shard;
    uint32_t pos;
    uint16_t len;
    uint16_t cinfo;  /* reserved for log values or used for compression */
    uint64_t start_ts;
    uint64_t end_ts;
};



#endif  /* SIRIDB_SERIES_H_ */
