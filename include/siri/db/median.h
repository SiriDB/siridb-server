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
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        uint64_t n);

/* sets points real, even when the given points are integer type */
int siridb_median_real(
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        double percentage);
