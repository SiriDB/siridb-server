/*
 * reindex.h - SiriDB Re-index.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 27-07-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <uv.h>
#include <siri/db/db.h>
#include <siri/db/series.h>

#define REINDEX_FN ".reindex"

typedef struct siridb_s siridb_t;
typedef struct siridb_series_s siridb_series_t;

typedef struct siridb_reindex_s
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
} siridb_reindex_t;

siridb_reindex_t * siridb_reindex_open(siridb_t * siridb, int create_new);
void siridb_reindex_fopen(siridb_reindex_t * reindex, const char * opentype);
void siridb_reindex_free(siridb_reindex_t ** reindex);
void siridb_reindex_status_update(siridb_t * siridb);
void siridb_reindex_close(siridb_reindex_t * reindex);
void siridb_reindex_start(uv_timer_t * timer);
const char * siridb_reindex_progress(siridb_t * siridb);
