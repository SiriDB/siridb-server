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
#include <slist/slist.h>
#include <cexpr/cexpr.h>

#define W0_CT_SERIES ct_t * ct_series;
#define W1_WHERE_EXPR cexpr_t * where_expr;

/* wrappers */
typedef struct query_wrapper_ct_series_s
{
    W0_CT_SERIES
} query_wrapper_ct_series_t;

typedef struct query_wrapper_where_node_s
{
    W0_CT_SERIES    // padding
    W1_WHERE_EXPR
} query_wrapper_where_node_t;

typedef struct query_list_s
{
    W0_CT_SERIES
    W1_WHERE_EXPR
    slist_t * props;  // will be freed
    size_t limit;
} query_list_t;

typedef struct query_count_s
{
    W0_CT_SERIES
    W1_WHERE_EXPR
    size_t n;   // can be used as counter
} query_count_t;

typedef struct query_drop_s
{
    W0_CT_SERIES
    W1_WHERE_EXPR
    void * data; // data will NOT be freed, make sure to use it correct.
    size_t n;  // keep a counter for number of drops.
} query_drop_t;

/* TODO: probably we need to add ct_results or something to store the results
 */
typedef struct query_select_s
{
    W0_CT_SERIES
    W1_WHERE_EXPR
    uint64_t * start_ts;  // will NOT be freed
    uint64_t * end_ts;  // will NOT be freed
} query_select_t;


query_select_t * query_select_new(void);
void query_select_free(uv_handle_t * handle);

query_list_t * query_list_new(void);
void query_list_free(uv_handle_t * handle);

query_count_t * query_count_new(void);
void query_count_free(uv_handle_t * handle);

query_drop_t * query_drop_new(void);
void query_drop_free(uv_handle_t * handle);
