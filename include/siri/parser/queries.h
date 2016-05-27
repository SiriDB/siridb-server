/*
 * walkers.h - Helpers for listener (walking series, pools etc.)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 03-05-2016
 *
 */
#pragma once

#include <uv.h>
#include <inttypes.h>
#include <cleri/parser.h>
#include <ctree/ctree.h>

#define W0_CT_SERIES ct_node_t * ct_series;
#define W1_WHERE_NODE cleri_node_t * where_node;
#define W2_COLUMNS cleri_children_t * columms;

/* wrappers */
typedef struct query_wrapper_ct_series_s
{
    W0_CT_SERIES
} query_wrapper_ct_series_t;

typedef struct query_wrapper_where_node_s
{
    void * pad0;
    W1_WHERE_NODE
} query_wrapper_where_node_t;

typedef struct query_wrapper_columns_s
{
    void * pad0;
    void * pad1;
    W2_COLUMNS
} query_wrapper_columns_t;

typedef struct query_list_s
{
    W0_CT_SERIES
    W1_WHERE_NODE
    W2_COLUMNS
    size_t limit;
} query_list_t;

typedef struct query_count_s
{
    W0_CT_SERIES
    W1_WHERE_NODE
} query_count_t;

typedef struct query_drop_s
{
    W0_CT_SERIES
    W1_WHERE_NODE
    void * data; // data will not be freed, make sure to use it correct.
} query_drop_t;

typedef struct query_select_s
{
    W0_CT_SERIES
    W1_WHERE_NODE
    uint64_t * start_ts;
    uint64_t * end_ts;
} query_select_t;



query_select_t * query_select_new(void);
void query_select_free(uv_handle_t * handle);

query_list_t * query_list_new(void);
void query_list_free(uv_handle_t * handle);

query_count_t * query_count_new(void);
void query_count_free(uv_handle_t * handle);

query_drop_t * query_drop_new(void);
void query_drop_free(uv_handle_t * handle);
