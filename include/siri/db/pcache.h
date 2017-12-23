/*
 * pcache.h - Points structure with notion or its size
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-10-2016
 *
 */
#pragma once

#include <siri/db/points.h>

typedef struct siridb_pcache_s
{
    size_t len;
    points_tp tp;
    char * content;     /* string content */
    siridb_point_t * data;
    size_t size;   /* addition to normal points type */
    size_t str_sz;
} siridb_pcache_t;

siridb_pcache_t * siridb_pcache_new(points_tp tp);

int siridb_pcache_add_point(
        siridb_pcache_t * pcache,
        uint64_t * ts,
        qp_via_t * val);

int siridb_pcache_add_ts_obj(
        siridb_pcache_t * pcache,
        uint64_t * ts,
        qp_obj_t * obj);

#define siridb_pcache_free(pcache) \
    siridb_points_free((siridb_points_t *) pcache)

