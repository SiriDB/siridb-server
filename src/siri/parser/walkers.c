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

/*
 * Do not forget to flush the dropped file.
 *  using: fflush(siridb->dropped_fp);
 * We do not flush here since we want this function to be as fast as
 * possible.
 */
int walk_drop_series(siridb_series_t * series, uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    if (q_drop->where_expr == NULL || cexpr_run(
            q_drop->where_expr,
            (cexpr_cb_t) siridb_series_cexpr_cb,
            series))
    {
        siridb_series_drop(
            ((sirinet_socket_t *) query->client->data)->siridb,
            series);

        q_drop->n++;
    }

    return 0;
}

int walk_list_servers(
        siridb_server_t * server,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    slist_t * props = ((query_list_t *) query->data)->props;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    size_t i;

    siridb_server_walker_t wserver = {
        .server=server,
        .siridb=siridb
    };

    if (where_expr != NULL && !cexpr_run(
            where_expr,
            (cexpr_cb_t) siridb_server_cexpr_cb,
            &wserver))
    {
        return 0;  // false
    }

    qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; i < props->len; i++)
    {
        switch(*((uint32_t *) props->data[i]))
        {
        case CLERI_GID_K_ADDRESS:
            qp_add_string(query->packer, server->address);
            break;
        case CLERI_GID_K_BUFFER_PATH:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                            siridb->buffer_path :
                            (server->buffer_path != NULL) ?
                                    server->buffer_path : "");
            break;
        case CLERI_GID_K_BUFFER_SIZE:
            qp_add_int64(
                    query->packer,
                    (siridb->server == server) ?
                            siridb->buffer_size : server->buffer_size);
            break;
        case CLERI_GID_K_DBPATH:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                        siridb->dbpath :
                        (server->dbpath != NULL) ?
                            server->dbpath : "");
            break;
        case CLERI_GID_K_NAME:
            qp_add_string(query->packer, server->name);
            break;
        case CLERI_GID_K_ONLINE:
            qp_add_type(
                    query->packer,
                    (siridb->server == server || server->socket != NULL) ?
                            QP_TRUE : QP_FALSE);
            break;
        case CLERI_GID_K_POOL:
            qp_add_int16(query->packer, server->pool);
            break;
        case CLERI_GID_K_PORT:
            qp_add_int32(query->packer, server->port);
            break;
        case CLERI_GID_K_STARTUP_TIME:
            qp_add_int32(
                    query->packer,
                    (siridb->server == server) ?
                            siri.startup_time : server->startup_time);
            break;
        case CLERI_GID_K_STATUS:
            {
                char * status = siridb_server_str_status(server);
                qp_add_string(query->packer, status);
                free(status);
            }

            break;
        case CLERI_GID_K_UUID:
            {
                char uuid[37];
                uuid_unparse_lower(server->uuid, uuid);
                qp_add_string(query->packer, uuid);
            }
            break;
        case CLERI_GID_K_VERSION:
            qp_add_string(
                    query->packer,
                    (siridb->server == server) ?
                            SIRIDB_VERSION :
                            (server->version != NULL) ?
                                    server->version : "");
            break;
        /* all properties below are 'remote properties'. if a remote property
         * is detected we should perform the query on each server and only for
         * that specific server.
         */
        case CLERI_GID_K_LOG_LEVEL:
#ifdef DEBUG
            assert (siridb->server == server);
#endif
            qp_add_string(query->packer, Logger.level_name);
            break;
        case CLERI_GID_K_MAX_OPEN_FILES:
            qp_add_int32(
                    query->packer,
                    (int32_t) abs(siri.cfg->max_open_files));
            break;
        case CLERI_GID_K_MEM_USAGE:
            qp_add_int32(
                    query->packer,
                    (int32_t) (procinfo_total_physical_memory() / 1024));
            break;
        case CLERI_GID_K_OPEN_FILES:
            qp_add_int32(query->packer, siridb_open_files(siridb));
            break;
        case CLERI_GID_K_RECEIVED_POINTS:
            qp_add_int64(query->packer, siridb->received_points);
            break;
        case CLERI_GID_K_REINDEX_PROGRESS:
            qp_add_string(query->packer, siridb_reindex_progress(siridb));
            break;
        case CLERI_GID_K_SYNC_PROGRESS:
            qp_add_string(query->packer, siridb_initsync_sync_progress(siridb));
            break;
        case CLERI_GID_K_UPTIME:
            qp_add_int32(
                    query->packer,
                    (int32_t) (time(NULL) - siridb->start_ts));
            break;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    return 1;  // true
}

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
        qp_add_string(query->packer, series->name);
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
