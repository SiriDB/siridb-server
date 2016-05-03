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

#define SIRIPARSER_QUERY_LIST           \
    cleri_children_t * columms;     \
    size_t limit;

#define SIRIPARSER_QUERY_SERIES         \
    ct_node_t * ct_series;

/* wrapper both list and select */
typedef struct query_wrapper_series_s
{
    SIRIPARSER_QUERY_SERIES
} query_wrapper_series_t;

typedef struct query_list_s
{
    void * dummy;
    SIRIPARSER_QUERY_LIST
} query_list_t;

typedef struct query_select_s
{
    SIRIPARSER_QUERY_SERIES
    uint64_t * start_ts;
    uint64_t * end_ts;
} query_select_t;

query_select_t * query_select_new(void);
void query_select_free(uv_handle_t * handle);

query_list_t * query_list_new(void);
void query_list_free(uv_handle_t * handle);
