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

struct siridb_point_s;
struct siridb_points_s;
struct siridb_aggr_s;

typedef int (* siridb_aggr_cb)(
        struct siridb_point_s * point,
        struct siridb_points_s * points,
        struct siridb_aggr_s * aggr,
        char * err_msg);

typedef struct siridb_aggr_s
{
    siridb_aggr_cb cb;
    uint64_t group_by;
} siridb_aggr_t;

siridb_aggr_cb siridb_aggregates[KW_COUNT];

struct siridb_points_s * siridb_aggregate(
        struct siridb_points_s * source,
        siridb_aggr_t * aggr,
        char * err_msg);

void siridb_init_aggregates(void);
