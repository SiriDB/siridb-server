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
#include <slist/slist.h>

typedef enum
{
    TP_INT,
    TP_DOUBLE,
    TP_STRING
} points_tp;

typedef struct siridb_point_s
{
    uint64_t ts;
    qp_via_t val;
} siridb_point_t;

typedef struct siridb_points_s
{
    size_t len;
    points_tp tp;
    char * content;     /* string content */
    siridb_point_t * data;
} siridb_points_t;

siridb_points_t * siridb_points_new(size_t size, points_tp tp);
void siridb_points_free(siridb_points_t * points);
void siridb_points_add_point(
        siridb_points_t *__restrict points,
        uint64_t * ts,
        qp_via_t * val);
int siridb_points_pack(siridb_points_t * points, qp_packer_t * packer);
int siridb_points_raw_pack(siridb_points_t * points, qp_packer_t * packer);
siridb_points_t * siridb_points_merge(slist_t * plist, char * err_msg);
