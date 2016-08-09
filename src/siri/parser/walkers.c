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

int walk_select(const char * name, siridb_points_t * points, uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    qp_add_string(query->packer, name);
    siridb_points_pack(points, query->packer);

    return 0;
}

int walk_select_other(const char * name, siridb_points_t * points, uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    qp_add_string(query->packer, name);
    siridb_points_raw_pack(points, query->packer);

    return 0;
}
