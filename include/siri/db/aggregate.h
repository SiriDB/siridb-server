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
#include <slist/slist.h>

typedef struct siridb_point_s siridb_point_t;
typedef struct siridb_points_s siridb_points_t;
typedef struct siridb_aggr_s siridb_aggr_t;

typedef struct siridb_aggr_s
{
    uint32_t gid;
    int64_t group_by;
    int64_t timespan;  // used for derivative
} siridb_aggr_t;

siridb_points_t * siridb_aggregate_run(
        siridb_points_t * source,
        siridb_aggr_t * aggr,
        char * err_msg);

void siridb_init_aggregates(void);
slist_t * siridb_aggregate_list(cleri_node_t * node, char * err_msg);
void siridb_aggregate_list_free(slist_t * alist);
