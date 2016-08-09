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

int walk_select_master(const char * name, siridb_points_t * points, uv_async_t * handle);



