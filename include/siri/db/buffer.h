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
#include <siri/db/points.h>

struct siridb_s;
struct siridb_series_s;
struct siridb_points_s;

typedef struct siridb_buffer_s
{
    long int bf_offset;
    struct siridb_points_s * points;
} siridb_buffer_t;

int siridb_buffer_new_series(
        struct siridb_s * siridb,
        struct siridb_series_s * series);

int siridb_open_buffer(struct siridb_s * siridb);

int siridb_load_buffer(struct siridb_s * siridb);

void siridb_free_buffer(siridb_buffer_t * buffer);

void siridb_buffer_write_len(
        struct siridb_s * siridb,
        struct siridb_series_s * series);

/* Waring: we must check if the new point fits inside the buffer before using
 * the 'siridb_buffer_add_point()' function.
 */
void siridb_buffer_write_point(
        struct siridb_s * siridb,
        struct siridb_series_s * series,
        uint64_t * ts,
        qp_via_t * val);

void siridb_buffer_to_shards(
        struct siridb_s * siridb,
        struct siridb_series_s * series);
