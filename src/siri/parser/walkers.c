/*
 * walkers.c - Helpers for listener (walking series, pools etc.)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-05-2016
 *
 */
#include <assert.h>
#include <logger/logger.h>
#include <procinfo/procinfo.h>
#include <siri/db/aggregate.h>
#include <siri/db/initsync.h>
#include <siri/db/query.h>
#include <siri/db/reindex.h>
#include <siri/db/series.h>
#include <siri/db/shard.h>
#include <siri/grammar/gramp.h>
#include <siri/net/socket.h>
#include <siri/parser/queries.h>
#include <siri/parser/walkers.h>
#include <siri/siri.h>
#include <siri/version.h>

int walk_select(siridb_series_t * series, uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;
    siridb_points_t * points = siridb_series_get_points_num32(
            series,
            q_select->start_ts,
            q_select->end_ts);

    if (points != NULL)
    {
        qp_add_raw(query->packer, series->name, series->name_len);
        siridb_points_pack(points, query->packer);
        siridb_points_free(points);
    }

    /*
    siridb_aggr_t aggr;
    siridb_points_t * aggr_points;
    aggr.cb = siridb_aggregates[CLERI_GID_K_COUNT - KW_OFFSET];
    aggr.group_by = 20;
    aggr_points = siridb_aggregate(points, &aggr, query->err_msg);
    */

    /*
     * the best solution seems to be creating another tree containing a llist
     * with points where each points represents a result for one aggregation.
     */


//    siridb_points_free(aggr_points);


    return 0;
}
