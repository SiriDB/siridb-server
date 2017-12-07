/*
 * queries.h - Querie helpers for listener
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
#include <imap/imap.h>
#include <slist/slist.h>
#include <cexpr/cexpr.h>
#include <cleri/cleri.h>
#include <ctree/ctree.h>
#include <siri/db/presuf.h>
#include <siri/db/group.h>
#include <siri/db/series.h>
#include <siri/db/user.h>

#define QUERIES_IGNORE_DROP_THRESHOLD 1

enum
{
    QUERIES_ALTER,
    QUERIES_COUNT,
    QUERIES_DROP,
    QUERIES_LIST,
    QUERIES_SELECT
};

typedef enum
{
    QUERY_ALTER_NONE,
    QUERY_ALTER_DATABASE,
    QUERY_ALTER_GROUP,
    QUERY_ALTER_SERVER,
    QUERY_ALTER_SERVERS,
    QUERY_ALTER_USER
} query_alter_tp;

#define QUERY_DEF               \
uint8_t tp;                     \
imap_t * series_map;            \
imap_t * series_tmp;            \
imap_t * pmap;                  \
slist_t * slist;                \
size_t slist_index;             \
imap_update_cb update_cb;       \
cexpr_t * where_expr;           \
pcre2_code * regex;             \
pcre2_match_data * match_data;


/* wrappers */
typedef struct query_wrapper_s
{
    QUERY_DEF
} query_wrapper_t;

union query_alter_u
{
    siridb_group_t * group;
    siridb_server_t * server;
    siridb_user_t * user;
    void * dummy;
};

typedef struct query_alter_s
{
    QUERY_DEF
    query_alter_tp alter_tp;
    union query_alter_u via;
    size_t n;   // can be used as counter
} query_alter_t;

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
    slist_t * shards_list;
} query_drop_t;

typedef struct query_list_s
{
    QUERY_DEF
    slist_t * props;  // will be freed
    size_t limit;
} query_list_t;

typedef struct query_select_s
{
    QUERY_DEF
    size_t n;
    size_t nselects;
    uint64_t * start_ts;  // will NOT be freed
    uint64_t * end_ts;  // will NOT be freed
    siridb_presuf_t * presuf;
    char * merge_as;
    ct_t * result;
    imap_t * points_map;    // points_map for caching
    slist_t * alist;        // aggregation list (can be used multiple times)
    slist_t * mlist;        // merge aggregation list
} query_select_t;

query_alter_t * query_alter_new(void);
void query_alter_free(uv_handle_t * handle);

query_count_t * query_count_new(void);
void query_count_free(uv_handle_t * handle);

query_drop_t * query_drop_new(void);
void query_drop_free(uv_handle_t * handle);

query_list_t * query_list_new(void);
void query_list_free(uv_handle_t * handle);

query_select_t * query_select_new(void);
void query_select_free(uv_handle_t * handle);

void query_help_free(uv_handle_t * handle);
