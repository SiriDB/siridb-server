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

void walk_select(
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

    qp_add_type(query->packer, QP_ARRAY_CLOSE);
}

void walk_drop_shard(
        siridb_series_t * series,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;

    siridb_series_remove_shard_num32(
            siridb,
            series,
            (siridb_shard_t *) ((query_drop_t *) query->data)->data);
}

void walk_drop_series(
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
}
