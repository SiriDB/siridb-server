/*
 * shards.h - Collection of SiriDB shards.
 *
 * Info shards->mutex:
 *
 *  Main thread:
 *      siridb->shards :    read (lock)         write (lock)

 *  Other threads:
 *      siridb->shards :    read (lock)         write (lock)
 *
 *  Note: since series->idx hold a reference to a shard, a lock to the
 *        series_mutex is required in some cases.
 */
#ifndef SIRIDB_SHARDS_H_
#define SIRIDB_SHARDS_H_

#define SIRIDB_SHARDS_PATH "shards/"

#include <siri/db/db.h>
#include <omap/omap.h>

void siridb_shards_destroy_cb(omap_t * shards);
int siridb_shards_load(siridb_t * siridb);
int siridb_shards_add_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points);
double siridb_shards_count_percent(
        siridb_t * siridb,
        uint64_t end_ts,
        uint8_t tp);
size_t siridb_shards_n(siridb_t * siridb);
vec_t * siridb_shards_vec(siridb_t * siridb);

#endif  /* SIRIDB_SHARDS_H_ */
