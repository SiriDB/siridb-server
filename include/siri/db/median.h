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

void siridb_median_find_n(
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        uint64_t n);

void siridb_median_real(
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        double percentage);
