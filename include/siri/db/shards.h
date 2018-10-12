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

int siridb_shards_load(siridb_t * siridb);
int siridb_shards_add_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points);

#endif  /* SIRIDB_SHARDS_H_ */
