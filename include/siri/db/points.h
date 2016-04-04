/*
 * points.h - Array object for points.
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
#include <stdlib.h>
#include <inttypes.h>
#include <qpack/qpack.h>

typedef struct siridb_point_s
{
    uint64_t ts;
    qp_via_t val;
} siridb_point_t;

typedef struct siridb_points_s
{
    size_t len;
    size_t size;
    siridb_point_t * data;
} siridb_points_t;


siridb_points_t * siridb_new_points(size_t size);
void siridb_free_points(siridb_points_t * points);
void siridb_points_add_point(
        siridb_points_t * points,
        uint64_t * ts,
        qp_via_t * val);

