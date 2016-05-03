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

struct siridb_series_s;

void walk_select(
        const char * series_name,
        struct siridb_series_s * series,
        uv_async_t * handle);
