#include <siri/parser/queries.h>
#include <stddef.h>
#include <stdlib.h>
#include <siri/db/query.h>
#include <logger/logger.h>


#define DEFAULT_LIST_LIMIT 1000

#define QUERIES_NEW(q)              \
q->series_map = NULL;               \
q->series_tmp = NULL;               \
q->slist = NULL;                    \
q->slist_index = 0;                 \
q->update_cb = NULL;                \
q->where_expr = NULL;               \
q->regex = NULL;                    \
q->regex_extra = NULL;


#define QUERIES_FREE(q, handle)                                 \
if (q->series_map != NULL)                                      \
{                                                               \
    imap_free_cb(                                               \
            q->series_map,                                      \
            (imap_cb) &siridb_series_decref,                    \
            NULL);                                              \
}                                                               \
if (q->series_tmp != NULL)                                      \
{                                                               \
    imap_free_cb(                                               \
            q->series_tmp,                                      \
            (imap_cb) &siridb_series_decref,                    \
            NULL);                                              \
}                                                               \
if (q->slist != NULL)                                           \
{                                                               \
    for (; q->slist_index < q->slist->len; q->slist_index++)    \
    {                                                           \
        siridb_series_decref(q->slist->data[q->slist_index]);   \
    }                                                           \
    slist_free(q->slist);                                       \
}                                                               \
if (q->where_expr != NULL)                                      \
{                                                               \
    cexpr_free(q->where_expr);                                  \
}                                                               \
free(q->regex);                                                 \
free(q->regex_extra);                                           \
free(q);                                                        \
siridb_query_free(handle);

query_select_t * query_select_new(void)
{
    query_select_t * q_select =
            (query_select_t *) malloc(sizeof(query_select_t));

    QUERIES_NEW(q_select)

    q_select->tp = QUERIES_SELECT;
    q_select->start_ts = NULL;
    q_select->end_ts = NULL;
    q_select->presuf = siridb_presuf_new();
    q_select->merge_as = NULL;
    return q_select;
}

query_list_t * query_list_new(void)
{
    query_list_t * q_list =
            (query_list_t *) malloc(sizeof(query_list_t));

    QUERIES_NEW(q_list)

    q_list->tp = QUERIES_LIST;
    q_list->props = NULL;
    q_list->limit = DEFAULT_LIST_LIMIT;
    return q_list;
}

query_count_t * query_count_new(void)
{
    query_count_t * q_count =
            (query_count_t *) malloc(sizeof(query_count_t));

    QUERIES_NEW(q_count)

    q_count->tp = QUERIES_COUNT;
    q_count->n = 0;
    return q_count;
}

query_drop_t * query_drop_new(void)
{
    query_drop_t * q_drop =
            (query_drop_t *) malloc(sizeof(query_drop_t));

    QUERIES_NEW(q_drop)

    q_drop->tp = QUERIES_DROP;
    q_drop->n = 0;
    q_drop->flags = 0;

    return q_drop;
}

void query_select_free(uv_handle_t * handle)
{
    query_select_t * q_select =
            (query_select_t *) ((siridb_query_t *) handle->data)->data;

    siridb_presuf_free(q_select->presuf);
    free(q_select->merge_as);
    if (q_select->result != NULL)
    {
        ct_free(q_select->result, (ct_free_cb) &siridb_points_free);
    }

    QUERIES_FREE(q_select, handle)
}

void query_list_free(uv_handle_t * handle)
{
    query_list_t * q_list =
            (query_list_t *) ((siridb_query_t *) handle->data)->data;

    if (q_list->props != NULL)
    {
        slist_free(q_list->props);
    }

    QUERIES_FREE(q_list, handle)
}


void query_count_free(uv_handle_t * handle)
{
    query_count_t * q_count =
            (query_count_t *) ((siridb_query_t *) handle->data)->data;

    QUERIES_FREE(q_count, handle)
}


void query_drop_free(uv_handle_t * handle)
{
    query_drop_t * q_drop =
            (query_drop_t *) ((siridb_query_t *) handle->data)->data;

    QUERIES_FREE(q_drop, handle)
}
