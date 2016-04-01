/*
 * buffer.h - Buffer for integer and double series.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 01-04-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <siri/db/series.h>

struct siridb_s;
struct siridb_series_s;

int siridb_buffer_new_series(
        struct siridb_s * siridb,
        struct siridb_series_s * series);

int siridb_open_buffer(struct siridb_s * siridb);

int siridb_load_buffer(struct siridb_s * siridb);

