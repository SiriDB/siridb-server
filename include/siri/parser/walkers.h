/*
 * walkers.h - Helpers for listener (walking series, pools etc.)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-05-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/db/series.h>

typedef struct siridb_series_s siridb_series_t;

int walk_drop_series(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle);

void walk_drop_shard(
        siridb_series_t * series,
        uv_async_t * handle);

int walk_list_series(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle);

int walk_select(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle);




