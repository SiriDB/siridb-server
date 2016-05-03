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


void walk_series_select(
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
            ((sirinet_handle_t *) query->client->data)->siridb,
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
    siridb_free_points(points);

    qp_add_type(query->packer, QP_ARRAY_CLOSE);
}
