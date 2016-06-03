#include <siri/parser/queries.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctree/ctree.h>
#include <siri/db/query.h>
#include <logger/logger.h>

#define FREE(q_type)                                                          \
{                                                                             \
    siridb_query_t * query = (siridb_query_t *) handle->data;                 \
    q_type * q = (q_type *) query->data;                                      \
    if (q->ct_series != NULL)                                                 \
        ct_free_cb(q->ct_series, (ct_free_cb_t) &siridb_series_decref);       \
    free(q);                                                                  \
    /* normal free call */                                                    \
    siridb_free_query(handle);                                                \
}

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

query_drop_t * query_drop_new(void)
{
    query_drop_t * q_drop =
            (query_drop_t *) malloc(sizeof(query_drop_t));
    q_drop->ct_series = NULL;
    q_drop->where_node = NULL;
    q_drop->data = NULL; // will not be freed
    q_drop->n = 0;
    return q_drop;
}

void query_select_free(uv_handle_t * handle)
    FREE(query_select_t)

void query_list_free(uv_handle_t * handle)
    FREE(query_list_t)

void query_count_free(uv_handle_t * handle)
    FREE(query_count_t)

void query_drop_free(uv_handle_t * handle)
    FREE(query_drop_t)
