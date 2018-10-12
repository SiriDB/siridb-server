/*
 * reindex.h - SiriDB Re-index.
 *
 * Differences while re-indexing:
 *
 *  - Group information like number of series will be updated at a lower
 *    interval which leads to probably incorrect number of series per group.
 *    Selections for series in a group or a list of series per group are still
 *    correct and can only lack of brand new series. (newer than 30 seconds)
 *
 *  - Selecting an unknown series usually raises a QueryError but we do not
 *    raise this error during re-indexing since the series might be in either
 *    the old- or new pool. (selecting series during re-indexing has therefore
 *    the same behavior as a regular expression selection)
 *
 *  - Drop server is not allowed while re-indexing.
 */
#ifndef SIRIDB_REINDEX_H_
#define SIRIDB_REINDEX_H_

#define REINDEX_FN ".reindex"

typedef struct siridb_reindex_s siridb_reindex_t;

#include <inttypes.h>
#include <uv.h>
#include <siri/db/db.h>
#include <siri/db/series.h>

siridb_reindex_t * siridb_reindex_open(siridb_t * siridb, int create_new);
void siridb_reindex_fopen(siridb_reindex_t * reindex, const char * opentype);
void siridb_reindex_free(siridb_reindex_t ** reindex);
void siridb_reindex_status_update(siridb_t * siridb);
void siridb_reindex_close(siridb_reindex_t * reindex);
void siridb_reindex_start(uv_timer_t * timer);
const char * siridb_reindex_progress(siridb_t * siridb);

struct siridb_reindex_s
{
    FILE * fp;
    char * fn;
    int fd;
    long int size;
    uint32_t * next_series_id;
    sirinet_pkg_t * pkg;
    siridb_series_t * series;
    siridb_server_t * server;
    uv_timer_t * timer;
};

#endif  /* SIRIDB_REINDEX_H_ */
