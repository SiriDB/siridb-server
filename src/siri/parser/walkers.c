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
#include <siri/parser/walkers.h>
#include <siri/parser/queries.h>
#include <siri/db/query.h>
#include <siri/net/handle.h>
#include <siri/db/series.h>
#include <logger/logger.h>
#include <siri/grammar/gramp.h>
#include <siri/db/aggregate.h>
#include <siri/db/shard.h>

int walk_drop_series(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle)
{
    /* Do not forget to flush the dropped file.
     *  using: fflush(siridb->dropped_fp);
     * We do not flush here since we want this function to be as fast as
     * possible.
     */
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;

    /* we are sure the file pointer is at the end of file */
    if (fwrite(&series->id, sizeof(uint32_t), 1, siridb->dropped_fp) != 1)
    {
        log_critical("Cannot write %d to dropped cache file.", series->id);
    };

    /* remove series from map */
    imap32_pop(siridb->series_map, series->id);

    /* remove series from tree */
    ct_pop(siridb->series, series_name);

    /* decrement reference to series */
    siridb_series_decref(series);

    return 0;
}

void walk_drop_shard(
        siridb_series_t * series,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    siridb_shard_t * shard =
            (siridb_shard_t *) ((query_drop_t *) query->data)->data;

    if (shard->id % siridb->duration_num == series->mask)
    {
        siridb_series_remove_shard_num32(siridb, series, shard);
    }
}

int walk_list_series(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    slist_t * props = ((query_list_t *) query->data)->props;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    size_t i;

//    if (series->tp != SIRIDB_SERIES_TP_INT)
//    {
//        return 0; // false
//    }

    qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; i < props->len; i++)
    {
        switch(*((uint32_t *) props->data[i]))
        {
        case CLERI_GID_K_NAME:
            qp_add_string(query->packer, series_name);
            break;
        case CLERI_GID_K_LENGTH:
            qp_add_int32(query->packer, series->length);
            break;
        case CLERI_GID_K_TYPE:
            qp_add_string(query->packer, series_type_map[series->tp]);
            break;
        case CLERI_GID_K_POOL:
            qp_add_int16(query->packer, siridb->server->pool);
            break;
        case CLERI_GID_K_START:
            qp_add_int64(query->packer, series->start);
            break;
        case CLERI_GID_K_END:
            qp_add_int64(query->packer, series->end);
            break;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    return 1; // true
}

int walk_list_servers(
        siridb_server_t * server,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    slist_t * props = ((query_list_t *) query->data)->props;
//    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    size_t i;

    qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; i < props->len; i++)
    {
        switch(*((uint32_t *) props->data[i]))
        {
        case CLERI_GID_K_NAME:
            qp_add_string(query->packer, server->name);
            break;
        case CLERI_GID_K_ADDRESS:
            qp_add_string(query->packer, server->address);
            break;
        case CLERI_GID_K_PORT:
            qp_add_int32(query->packer, server->port);
            break;
        case CLERI_GID_K_POOL:
            qp_add_int16(query->packer, server->pool);
            break;
        case CLERI_GID_K_UUID:
            {
                char uuid[37];
                uuid_unparse_lower(server->uuid, uuid);
                qp_add_string(query->packer, uuid);
            }
            break;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    return 1;
}

int walk_select(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle)
{
    siridb_point_t * point;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;

    qp_add_string(query->packer, series_name);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    siridb_points_t * points = siridb_series_get_points_num32(
            series,
            q_select->start_ts,
            q_select->end_ts);

    /*
    siridb_aggr_t aggr;
    siridb_points_t * aggr_points;
    aggr.cb = siridb_aggregates[CLERI_GID_K_COUNT - KW_OFFSET];
    aggr.group_by = 20;
    aggr_points = siridb_aggregate(points, &aggr, query->err_msg);
    */

    point = points->data;
    switch (points->tp)
    {
    case SIRIDB_POINTS_TP_INT:
        for (size_t i = 0; i < points->len; i++, point++)
        {
            qp_add_type(query->packer, QP_ARRAY2);
            qp_add_int64(query->packer, (int64_t) point->ts);
            qp_add_int64(query->packer, point->val.int64);
        }
        break;
    case SIRIDB_POINTS_TP_DOUBLE:
        for (size_t i = 0; i < points->len; i++, point++)
        {
            qp_add_type(query->packer, QP_ARRAY2);
            qp_add_int64(query->packer, (int64_t) point->ts);
            qp_add_double(query->packer, point->val.real);
        }
        break;
    }
    siridb_points_free(points);
//    siridb_points_free(aggr_points);

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    return 0;
}
