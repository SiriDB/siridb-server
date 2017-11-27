/*
 * median.h - Calculate median, median high and median low.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-04-2016
 *
 */
#pragma once

#include <siri/db/points.h>
#include <inttypes.h>

struct siridb_point_s;
struct siridb_points_s;

/* sets point int64 or real depending on the given points */
int siridb_median_find_n(
        siridb_point_t * point,
        siridb_points_t * points,
        uint64_t n);

/* sets points real, even when the given points are integer type */
int siridb_median_real(
        siridb_point_t * point,
        siridb_points_t * points,
        double percentage);
