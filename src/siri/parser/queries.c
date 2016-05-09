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

    q_select->ct_series = NULL;
    q_select->where_node = NULL;
    q_select->start_ts = NULL;
    q_select->end_ts = NULL;
    return q_select;
}

query_list_t * query_list_new(void)
{
    query_list_t * q_list =
            (query_list_t *) malloc(sizeof(query_list_t));
    q_list->ct_series = NULL;
    q_list->where_node = NULL;
    q_list->columms = NULL;
    q_list->limit = 0;

    return q_list;
}

query_count_t * query_count_new(void)
{
    query_count_t * q_count =
            (query_count_t *) malloc(sizeof(query_count_t));
    q_count->ct_series = NULL;
    q_count->where_node = NULL;

    return q_count;
}

void query_select_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query_select_t * q_select = (query_select_t *) query->data;

    if (q_select->ct_series != NULL)
        ct_free(q_select->ct_series);

    free(q_select);

    /* normal free call */
    siridb_free_query(handle);
}

void query_list_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query_list_t * q_list = (query_list_t *) query->data;

    if (q_list->ct_series != NULL)
        ct_free(q_list->ct_series);

    free(q_list);

    /* normal free call */
    siridb_free_query(handle);
}

void query_count_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query_count_t * q_count = (query_count_t *) query->data;

    if (q_count->ct_series != NULL)
        ct_free(q_count->ct_series);

    free(q_count);

    /* normal free call */
    siridb_free_query(handle);
}
