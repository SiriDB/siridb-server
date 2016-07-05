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

#define SIRIDB_POINTS_TP_INT 0  //SIRIDB_SERIES_TP_INT
#define SIRIDB_POINTS_TP_DOUBLE 1  //SIRIDB_SERIES_TP_DOUBLE
#define SIRIDB_POINTS_TP_STRING 2  //SIRIDB_SERIES_TP_STRING

typedef struct siridb_point_s
{
    uint64_t ts;
    qp_via_t val;
} siridb_point_t;

typedef struct siridb_points_s
{
    size_t len;
    uint8_t tp;
    siridb_point_t * data;
} siridb_points_t;

siridb_points_t * siridb_points_new(size_t size, uint8_t tp);
void siridb_points_free(siridb_points_t * points);
void siridb_points_add_point(
        siridb_points_t * points,
        uint64_t * ts,
        qp_via_t * val);

siridb_points_t * siridb_points_extend(siridb_points_t * points[], size_t len);
siridb_points_t * siridb_points_merge(siridb_points_t * points[], size_t len);
