#include <siri/parser/queries.h>
#include <stddef.h>
#include <stdlib.h>
#include <siri/db/query.h>
#include <logger/logger.h>

#define DEFAULT_LIST_LIMIT 1000

#define FREE(q_type)                                                          \
{                                                                             \
    siridb_query_t * query = (siridb_query_t *) handle->data;                 \
    q_type * q = (q_type *) query->data;                                      \
    if (q->series_map != NULL)                                                \
        imap_free_cb(                                                       \
                q->series_map,                                                \
                (imap_cb) &siridb_series_decref,                            \
                NULL);                                                        \
    if (q->where_expr != NULL)                                                \
        cexpr_free(q->where_expr);                                            \
    free(q);                                                                  \
    siridb_query_free(handle);                                                \
}

query_select_t * query_select_new(void)
{
    query_select_t * q_select =
            (query_select_t *) malloc(sizeof(query_select_t));

    q_select->series_map = NULL;
    q_select->where_expr = NULL;
    q_select->start_ts = NULL;
    q_select->end_ts = NULL;
    return q_select;
}

query_list_t * query_list_new(void)
{
    query_list_t * q_list =
            (query_list_t *) malloc(sizeof(query_list_t));
    q_list->series_map = NULL;
    q_list->where_expr = NULL;
    q_list->props = NULL;
    q_list->limit = DEFAULT_LIST_LIMIT;
    return q_list;
}

query_count_t * query_count_new(void)
{
    query_count_t * q_count =
            (query_count_t *) malloc(sizeof(query_count_t));
    q_count->series_map = NULL;
    q_count->where_expr = NULL;
    q_count->n = 0;
    return q_count;
}

query_drop_t * query_drop_new(void)
{
    query_drop_t * q_drop =
            (query_drop_t *) malloc(sizeof(query_drop_t));
    q_drop->series_map = NULL;
    q_drop->where_expr = NULL;
    q_drop->data = NULL; // will not be freed
    q_drop->n = 0;
    return q_drop;
}

void query_select_free(uv_handle_t * handle)
    FREE(query_select_t)

void query_list_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_list_t * q_list = (query_list_t *) query->data;

    if (q_list->series_map != NULL)
    {
        imap_free_cb(
                q_list->series_map,
                (imap_cb) &siridb_series_decref,
                NULL);
    }

    if (q_list->where_expr != NULL)
    {
        cexpr_free(q_list->where_expr);
    }

    if (q_list->props != NULL)
    {
        slist_free(q_list->props);
    }

    free(q_list);

    /* normal free call */
    siridb_query_free(handle);
}


void query_count_free(uv_handle_t * handle)
    FREE(query_count_t)

void query_drop_free(uv_handle_t * handle)
    FREE(query_drop_t)
