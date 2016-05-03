#include <siri/parser/queries.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctree/ctree.h>
#include <siri/db/query.h>
#include <logger/logger.h>

query_select_t * query_select_new(void)
{
    query_select_t * q_select =
            (query_select_t *) malloc(sizeof(query_select_t));
    q_select->ct_series = ct_new();

    /* we point to the node result so we do not need to free */
    q_select->start_ts = NULL;
    q_select->end_ts = NULL;
    return q_select;
}

query_list_t * query_list_new(void)
{
    query_list_t * q_list =
            (query_list_t *) malloc(sizeof(query_list_t));
    q_list->columms = NULL;
    q_list->limit = 0;

    return q_list;
}

query_list_series_t * query_list_series_new(void)
{
    query_list_series_t * q_list_series =
            (query_list_series_t *) malloc(sizeof(query_list_series_t));

    q_list_series->ct_series = ct_new();
    q_list_series->columms = NULL;
    q_list_series->limit = 0;

    return q_list_series;
}

void query_select_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query_select_t * q_select = (query_select_t *) query->data;

    ct_free(q_select->ct_series);
    free(q_select);

    /* normal free call */
    siridb_free_query(handle);
}

void query_list_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    free((query_list_t *) query->data);

    /* normal free call */
    siridb_free_query(handle);
}

void query_list_series_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query_list_series_t * q_list_series = (query_list_series_t *) query->data;

    ct_free(q_list_series->ct_series);
    free(q_list_series);

    /* normal free call */
    siridb_free_query(handle);
}
