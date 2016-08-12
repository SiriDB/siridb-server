/*
 * queries.h - Helpers for listener (walking series, pools etc.)
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
#include <imap/imap.h>
#include <slist/slist.h>
#include <cexpr/cexpr.h>
#include <ctree/ctree.h>
#include <siri/db/presuf.h>

#define QUERIES_IGNORE_DROP_THRESHOLD 1

enum
{
    QUERIES_LIST,
    QUERIES_COUNT,
    QUERIES_DROP,
    QUERIES_SELECT
};

#define QUERY_DEF           \
uint8_t tp;                 \
imap_t * series_map;        \
imap_t * series_tmp;        \
imap_t * pmap;              \
slist_t * slist;            \
size_t slist_index;         \
imap_update_cb update_cb;   \
cexpr_t * where_expr;       \
pcre * regex;               \
pcre_extra * regex_extra;


/* wrappers */
typedef struct query_wrapper_s
{
    QUERY_DEF
} query_wrapper_t;

typedef struct query_list_s
{
    QUERY_DEF
    slist_t * props;  // will be freed
    size_t limit;
} query_list_t;

typedef struct query_count_s
{
    QUERY_DEF
    size_t n;   // can be used as counter
} query_count_t;

typedef struct query_drop_s
{
    QUERY_DEF
    size_t n;  // keep a counter for number of drops.
    uint8_t flags;  // flags like ignore threshold
} query_drop_t;

typedef struct query_select_s
{
    QUERY_DEF
    size_t n;
    uint64_t * start_ts;  // will NOT be freed
    uint64_t * end_ts;  // will NOT be freed
    siridb_presuf_t * presuf;
    char * merge_as;
    ct_t * result;
    imap_t * points_map;    // TODO: use points_map for caching
    slist_t * alist;        // aggregation list (can be used multiple times)
    slist_t * mlist;        // merge aggregation list
} query_select_t;


query_select_t * query_select_new(void);
void query_select_free(uv_handle_t * handle);

query_list_t * query_list_new(void);
void query_list_free(uv_handle_t * handle);

query_count_t * query_count_new(void);
void query_count_free(uv_handle_t * handle);

query_drop_t * query_drop_new(void);
void query_drop_free(uv_handle_t * handle);
