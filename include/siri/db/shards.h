/*
 * shards.h - SiriDB shards.
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

#define SIRIDB_SHARDS_PATH "shards/"

typedef struct siridb_s siridb_t;

int siridb_shards_load(siridb_t * siridb);
void siridb_shards_drop_work(uv_work_t * work);
void siridb_shards_drop_work_finish(uv_work_t * work, int status);
int siridb_shards_add_points(
        siridb_t * siridb,
        siridb_series_t * series,
        siridb_points_t * points);
