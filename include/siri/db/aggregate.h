/*
 * aggregate.h - SiriDB aggregation methods.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 15-04-2016
 *
 */
#pragma once

#include <siri/db/points.h>
#include <siri/grammar/gramp.h>

typedef struct siridb_point_s siridb_point_t;
typedef struct siridb_points_s siridb_points_t;
typedef struct siridb_aggr_s siridb_aggr_t;

typedef int (* siridb_aggr_cb)(
        siridb_point_t * point,
        siridb_points_t * points,
        siridb_aggr_t * aggr,
        char * err_msg);

typedef struct siridb_aggr_s
{
    siridb_aggr_cb cb;
    uint64_t group_by;
} siridb_aggr_t;

siridb_aggr_cb siridb_aggregates[KW_COUNT];

siridb_points_t * siridb_aggregate(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);

void siridb_init_aggregates(void);
