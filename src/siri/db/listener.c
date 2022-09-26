/*
 * listener.c - Contains functions for processing queries.
 */
#include <assert.h>
#include <cexpr/cexpr.h>
#include <inttypes.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/db/aggregate.h>
#include <siri/db/group.h>
#include <siri/db/groups.h>
#include <siri/db/nodes.h>
#include <siri/db/presuf.h>
#include <siri/db/props.h>
#include <siri/db/props.h>
#include <siri/db/query.h>
#include <siri/db/re.h>
#include <siri/db/series.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/db/tags.h>
#include <siri/db/user.h>
#include <siri/db/users.h>
#include <siri/db/listener.h>
#include <siri/db/queries.h>
#include <siri/db/sset.h>
#include <siri/err.h>
#include <siri/grammar/gramp.h>
#include <siri/help/help.h>
#include <siri/net/promises.h>
#include <siri/net/protocol.h>
#include <siri/net/clserver.h>
#include <siri/net/tcp.h>
#include <siri/siri.h>
#include <xstr/xstr.h>
#include <sys/time.h>


static uv_async_cb SIRIDB_NODE_ENTER[CLERI_END];
static uv_async_cb SIRIDB_NODE_EXIT[CLERI_END];

uv_async_cb siridb_node_get_enter(enum cleri_grammar_ids gid)
{
    return SIRIDB_NODE_ENTER[gid];
}

uv_async_cb siridb_node_get_exit(enum cleri_grammar_ids gid)
{
    return SIRIDB_NODE_EXIT[gid];
}


#define MAX_ITERATE_COUNT 10000       /* ten-thousand  */
#define MAX_BATCH_REQUIRE_SHARD 100   /* after reading 100 shards, iterate  */

#define QP_ADD_SUCCESS qp_add_raw( \
    query->packer, (const unsigned char *) "success_msg", 11);
#define DEFAULT_ALLOC_COLUMNS 6
#define IS_MASTER (query->flags & SIRIDB_QUERY_FLAG_MASTER)

#define MASTER_CHECK_ONLINE(siridb)                                         \
if (IS_MASTER && !siridb_server_self_online(siridb->server))                \
{                                                                           \
    sprintf(query->err_msg,                                                 \
            "Server '%s' is currently not available to process "            \
            "this request",                                                 \
            siridb->server->name);                                          \
    siridb_query_send_error(handle, CPROTO_ERR_SERVER);                     \
    return;                                                                 \
}                                                                           \
if (IS_MASTER && !siridb_pools_online(siridb))                              \
{                                                                           \
    sprintf(query->err_msg,                                                 \
            "At least one pool is lacking an online server to process "     \
            "this request");                                                \
    siridb_query_send_error(handle, CPROTO_ERR_POOL);                       \
    return;                                                                 \
}

#define MASTER_CHECK_VERSION(siridb, _VERSION)                              \
if (IS_MASTER)                                                              \
{                                                                           \
    int nservers = siridb_servers_check_version(siridb, _VERSION);          \
    if (nservers)                                                           \
    {                                                                       \
        sprintf(query->err_msg,                                             \
                "At least %d server%s not running version %s "              \
                "or greater which is required for this query.",             \
                nservers, (nservers == 1) ? " is" : "s are", _VERSION);     \
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);                  \
        return;                                                             \
    }                                                                       \
}

#define MASTER_CHECK_ACCESSIBLE(siridb)                                     \
if (IS_MASTER && !siridb_server_self_accessible(siridb->server))            \
{                                                                           \
    sprintf(query->err_msg,                                                 \
            "Server '%s' is currently not accessible to process "           \
            "this request",                                                 \
            siridb->server->name);                                          \
    siridb_query_send_error(handle, CPROTO_ERR_SERVER);                     \
    return;                                                                 \
}                                                                           \
if (IS_MASTER && !siridb_pools_accessible(siridb))                          \
{                                                                           \
    sprintf(query->err_msg,                                                 \
            "At least one pool is lacking an accessible server to process " \
            "this request");                                                \
    siridb_query_send_error(handle, CPROTO_ERR_POOL);                       \
    return;                                                                 \
}

#define MASTER_CHECK_REINDEXING(siridb)                                     \
if (IS_MASTER && siridb_is_reindexing(siridb))                              \
{                                                                           \
   sprintf(query->err_msg,                                                  \
           "SiriDB cannot perform this request because the database is "    \
           "currently re-indexing");                                        \
    siridb_query_send_error(handle, CPROTO_ERR_POOL);                       \
    return;                                                                 \
}

#define ON_PROMISES                                             \
    siri_async_decref(&handle);                                 \
    if (promises == NULL)                                       \
    {                                                           \
        return;  /* signal is raised when handle is NULL */     \
    }                                                           \
    if (handle == NULL)                                         \
    {                                                           \
        sirinet_promises_llist_free(promises);                  \
        return;  /* signal is raised when handle is NULL */     \
    }

#define MEM_ERR_RET                                             \
        sprintf(query->err_msg, "Memory allocation error.");    \
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);      \
        return;

#define FILE_ERR_RET                                            \
        sprintf(query->err_msg, "File error occurred.");        \
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);      \
        return;

#define MSG_SUCCESS_CREATE_USER \
    "Successfully created user '%s'."
#define MSG_SUCCESS_DROP_USER \
    "Successfully dropped user '%s'."
#define MSG_SUCCESS_ALTER_USER \
    "Successfully updated user '%s'."
#define MSG_SUCCESS_GRANT_USER \
    "Successfully granted permissions to user '%s'."
#define MSG_SUCCESS_REVOKE_USER \
    "Successfully revoked permissions from user '%s'."
#define MSG_SUCCESS_CREATE_GROUP \
    "Successfully created group '%s'."
#define MSG_SUCCESS_DROP_GROUP \
    "Successfully dropped group '%s'."
#define MSG_SUCCESS_ALTER_GROUP \
    "Successfully updated group '%s'."
#define MSG_SUCCESS_ALTER_TAG \
    "Successfully updated tag '%s'."
#define MSG_SUCCESS_SET_DROP_THRESHOLD \
    "Successfully changed drop_threshold from %g to %g."
#define MSG_SUCCESS_SET_LIST_LIMIT \
    "Successfully changed list limit from %" PRIu32 " to %" PRIu32 "."
#define MSG_SUCCESS_SET_EXPIRATION \
    "Successfully changed expiration from %" PRIu64 " to %" PRIu64 "."
#define MSG_SUCCESS_SET_ADDR_PORT \
    "Successfully changed server address to '%s'."
#define MSG_SUCCESS_DROP_SERVER \
    "Successfully dropped server '%s'."
#define MSG_SUCCES_SET_LOG_LEVEL_MULTI \
    "Successfully set log level to '%s' on %lu servers."
#define MSG_SUCCES_SET_TEE_DISABLED \
    "Successfully disabled tee."
#define MSG_SUCCES_SET_TEE_ENABLED \
    "Successfully configured tee to %s."
#define MSG_SUCCES_SET_LOG_LEVEL \
    "Successfully set log level to '%s' on '%s'."
#define MSG_SUCCESS_SET_SELECT_POINTS_LIMIT \
    "Successfully changed select points limit from %" PRIu32 " to %" PRIu32 "."
#define MSG_SUCCES_DROP_SERIES \
    "Successfully dropped %lu series."
#define MSG_SUCCES_DROP_SHARDS \
    "Successfully dropped %lu shards. (this number does not include " \
    "shards which are dropped on replica servers)"
#define MSG_SUCCES_SET_BACKUP_MODE \
    "Successfully %s backup mode on '%s'."
#define MSG_SUCCES_SET_TIMEZONE \
    "Successfully changed timezone from '%s' to '%s'."
#define MSG_ERR_SERVER_ADDRESS \
    "Its only possible to change a servers address or port when the server " \
    "is not connected."
#define MSG_SUCCESS_TAG \
    "Successfully tagged %zu series."
#define MSG_SUCCESS_UNTAG \
    "Successfully untagged %zu series."
#define MSG_SUCCESS_DROP_TAG \
    "Successfully dropped tag '%s'."

static void enter_access_expr(uv_async_t * handle);
static void enter_alter_group(uv_async_t * handle);
static void enter_alter_series(uv_async_t * handle);
static void enter_alter_server(uv_async_t * handle);
static void enter_alter_servers(uv_async_t * handle);
static void enter_alter_stmt(uv_async_t * handle);
static void enter_alter_tag(uv_async_t * handle);
static void enter_alter_user(uv_async_t * handle);
static void enter_count_stmt(uv_async_t * handle);
static void enter_create_stmt(uv_async_t * handle);
static void enter_create_user(uv_async_t * handle);
static void enter_drop_stmt(uv_async_t * handle);
static void enter_grant_user(uv_async_t * handle);
static void enter_group_tag_match(uv_async_t * handle);
static void enter_help(uv_async_t * handle);
static void enter_limit_expr(uv_async_t * handle);
static void enter_list_stmt(uv_async_t * handle);
static void enter_merge_as(uv_async_t * handle);
static void enter_revoke_user(uv_async_t * handle);
static void enter_select_stmt(uv_async_t * handle);
static void enter_set_expression(uv_async_t * handle);
static void enter_set_ignore_threshold(uv_async_t * handle);
static void enter_set_name(uv_async_t * handle);
static void enter_set_password(uv_async_t * handle);
static void enter_series_all(uv_async_t * handle);
static void enter_series_name(uv_async_t * handle);
static void enter_series_match(uv_async_t * handle);
static void enter_series_parentheses(uv_async_t * handle);
static void enter_series_re(uv_async_t * handle);
static void enter_series_setopr(uv_async_t * handle);
static void enter_tag_series(uv_async_t * handle);
static void enter_timeit_stmt(uv_async_t * handle);
static void enter_untag_series(uv_async_t * handle);
static void enter_where_xxx(uv_async_t * handle);
static void enter_xxx_columns(uv_async_t * handle);

static void exit_after_expr(uv_async_t * handle);
static void exit_alter_group(uv_async_t * handle);
static void exit_alter_tag(uv_async_t * handle);
static void exit_alter_user(uv_async_t * handle);
static void exit_before_expr(uv_async_t * handle);
static void exit_between_expr(uv_async_t * handle);
static void exit_calc_stmt(uv_async_t * handle);
static void exit_count_groups(uv_async_t * handle);
static void exit_count_pools(uv_async_t * handle);
static void exit_count_series_length(uv_async_t * handle);
static void exit_count_series(uv_async_t * handle);
static void exit_count_servers_received(uv_async_t * handle);
static void exit_count_servers_selected(uv_async_t * handle);
static void exit_count_servers(uv_async_t * handle);
static void exit_count_shards_size(uv_async_t * handle);
static void exit_count_shards(uv_async_t * handle);
static void exit_count_tags(uv_async_t * handle);
static void exit_count_users(uv_async_t * handle);
static void exit_create_group(uv_async_t * handle);
static void exit_create_user(uv_async_t * handle);
static void exit_drop_group(uv_async_t * handle);
static void exit_drop_series(uv_async_t * handle);
static void exit_drop_server(uv_async_t * handle);
static void exit_drop_shards(uv_async_t * handle);
static void exit_drop_tag(uv_async_t * handle);
static void exit_drop_user(uv_async_t * handle);
static void exit_grant_user(uv_async_t * handle);
static void exit_head_expr(uv_async_t * handle);
static void exit_help_xxx(uv_async_t * handle);
static void exit_list_groups(uv_async_t * handle);
static void exit_list_pools(uv_async_t * handle);
static void exit_list_series(uv_async_t * handle);
static void exit_list_servers(uv_async_t * handle);
static void exit_list_shards(uv_async_t * handle);
static void exit_list_tags(uv_async_t * handle);
static void exit_list_users(uv_async_t * handle);
static void exit_revoke_user(uv_async_t * handle);
static void exit_select_aggregate(uv_async_t * handle);
static void exit_select_stmt(uv_async_t * handle);
static void exit_series_match(uv_async_t * handle);
static void exit_series_parentheses(uv_async_t * handle);
static void exit_set_address(uv_async_t * handle);
static void exit_set_backup_mode(uv_async_t * handle);
static void exit_set_drop_threshold(uv_async_t * handle);
static void exit_set_expiration_log(uv_async_t * handle);
static void exit_set_expiration_num(uv_async_t * handle);
static void exit_set_list_limit(uv_async_t * handle);
static void exit_set_log_level(uv_async_t * handle);
static void exit_set_port(uv_async_t * handle);
static void exit_set_select_points_limit(uv_async_t * handle);
static void exit_set_tee(uv_async_t * handle);
static void exit_set_timezone(uv_async_t * handle);
static void exit_show_stmt(uv_async_t * handle);
static void exit_tag_series(uv_async_t * handle);
static void exit_tail_expr(uv_async_t * handle);
static void exit_timeit_stmt(uv_async_t * handle);
static void exit_untag_series(uv_async_t * handle);

/* async loop functions */
static void async_count_series(uv_async_t * handle);
static void async_count_series_length(uv_async_t * handle);
static void async_drop_series(uv_async_t * handle);
static void async_drop_shards(uv_async_t * handle);
static void async_filter_series(uv_async_t * handle);
static void async_list_series(uv_async_t * handle);
static void async_no_points_aggregate(uv_async_t * handle);
static void async_select_aggregate(uv_async_t * handle);
static void async_series_re(uv_async_t * handle);

/* on response functions */
static void on_ack_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static void on_alter_xxx_response(vec_t * promises, uv_async_t * handle);
static void on_count_xxx_response(vec_t * promises, uv_async_t * handle);
static void on_drop_series_response(vec_t * promises, uv_async_t * handle);
static void on_drop_shards_response(vec_t * promises, uv_async_t * handle);
static void on_groups_response(vec_t * promises, uv_async_t * handle);
static void on_tags_response(vec_t * promises, uv_async_t * handle);
static void on_list_xxx_response(vec_t * promises, uv_async_t * handle);
static void on_select_response(vec_t * promises, uv_async_t * handle);
static void on_update_xxx_response(vec_t * promises, uv_async_t * handle);
static void on_tag_response(vec_t * promises, uv_async_t * handle);

/* helper functions */
static void master_select_work(uv_work_t * handle);
static void master_select_work_finish(uv_work_t * work, int status);
static int items_select_master(
        const char * name,
        size_t len,
        siridb_points_t * points,
        uv_async_t * handle);
static int items_select_master_merge(
        const char * name,
        size_t len,
        vec_t * plist,
        uv_async_t * handle);
static int items_select_other(
        const char * name,
        size_t len,
        siridb_points_t * points,
        uv_async_t * handle);
static int items_select_other_merge(
        const char * name,
        size_t len,
        vec_t * plist,
        uv_async_t * handle);
static void on_select_unpack_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points,
        uint32_t select_points_limit);
static void on_select_unpack_merged_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points,
        uint32_t select_points_limit);

static int values_list_groups(siridb_group_t * group, uv_async_t * handle);
static int values_count_groups(siridb_group_t * group, uv_async_t * handle);
static void finish_list_groups(uv_async_t * handle);
static void finish_count_groups(uv_async_t * handle);
static int values_list_tags(siridb_tag_t * tag, uv_async_t * handle);
static int values_count_tags(siridb_tag_t * tag, uv_async_t * handle);
static void finish_list_tags(uv_async_t * handle);
static void finish_count_tags(uv_async_t * handle);


/* address bindings for default list properties */
static uint32_t GID_K_NAME = CLERI_GID_K_NAME;
static uint32_t GID_K_POOL = CLERI_GID_K_POOL;
static uint32_t GID_K_VERSION = CLERI_GID_K_VERSION;
static uint32_t GID_K_ONLINE = CLERI_GID_K_ONLINE;
static uint32_t GID_K_STATUS = CLERI_GID_K_STATUS;
static uint32_t GID_K_SERVER = CLERI_GID_K_SERVER;
static uint32_t GID_K_SERVERS = CLERI_GID_K_SERVERS;
static uint32_t GID_K_SERIES = CLERI_GID_K_SERIES;
static uint32_t GID_K_SID = CLERI_GID_K_SID;
static uint32_t GID_K_START = CLERI_GID_K_START;
static uint32_t GID_K_END = CLERI_GID_K_END;

/*
 * Start SIRIPARSER_ASYNC_NEXT_NODE
 */
#define SIRIPARSER_ASYNC_NEXT_NODE                                          \
siridb_nodes_next(&query->nodes);                                           \
if (query->nodes == NULL)                                                   \
{                                                                           \
    siridb_send_query_result(handle);                                       \
}                                                                           \
else                                                                        \
{                                                                           \
    uv_close((uv_handle_t *) handle, (uv_close_cb) free);                   \
    handle = malloc(sizeof(uv_async_t));                                    \
    if (handle == NULL)                                                     \
    {                                                                       \
        ERR_ALLOC                                                           \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        handle->data = query;                                               \
        uv_async_init(siri.loop, handle, (uv_async_cb) query->nodes->cb);   \
        uv_async_send(handle);                                              \
    }                                                                       \
}

#define SIRIPARSER_NEXT_NODE            \
siridb_nodes_next(&query->nodes);       \
if (query->nodes == NULL)               \
{                                       \
    siridb_send_query_result(handle);   \
}                                       \
else                                    \
{                                       \
    handle->data = query;               \
    query->nodes->cb(handle);           \
}


/*
 * Start SIRIPARSER_MASTER_CHECK_ACCESS
 */
#define SIRIPARSER_MASTER_CHECK_ACCESS(user, ACCESS_BIT)                      \
if (IS_MASTER &&                                                              \
    !siridb_user_check_access(                                                \
        user,                                                                 \
        ACCESS_BIT,                                                           \
        query->err_msg))                                                      \
{                                                                             \
    siridb_query_send_error(handle, CPROTO_ERR_USER_ACCESS);                  \
    return;                                                                   \
}

void siridb_init_listener(void)
{
    uint_fast16_t i;

    for (i = 0; i < CLERI_END; i++)
    {
        SIRIDB_NODE_ENTER[i] = NULL;
        SIRIDB_NODE_EXIT[i] = NULL;
    }

    SIRIDB_NODE_ENTER[CLERI_GID_ACCESS_EXPR] = enter_access_expr;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_GROUP] = enter_alter_group;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_SERIES] = enter_alter_series;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_SERVER] = enter_alter_server;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_SERVERS] = enter_alter_servers;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_STMT] = enter_alter_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_TAG] = enter_alter_tag;
    SIRIDB_NODE_ENTER[CLERI_GID_ALTER_USER] = enter_alter_user;
    SIRIDB_NODE_ENTER[CLERI_GID_COUNT_STMT] = enter_count_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_CREATE_STMT] = enter_create_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_CREATE_USER] = enter_create_user;
    SIRIDB_NODE_ENTER[CLERI_GID_DROP_STMT] = enter_drop_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_GRANT_USER] = enter_grant_user;
    SIRIDB_NODE_ENTER[CLERI_GID_GROUP_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_GROUP_TAG_MATCH] = enter_group_tag_match;
    SIRIDB_NODE_ENTER[CLERI_GID_HELP_STMT] = enter_help;
    SIRIDB_NODE_ENTER[CLERI_GID_LIMIT_EXPR] = enter_limit_expr;
    SIRIDB_NODE_ENTER[CLERI_GID_LIST_STMT] = enter_list_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_MERGE_AS] = enter_merge_as;
    SIRIDB_NODE_ENTER[CLERI_GID_POOL_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_REVOKE_USER] = enter_revoke_user;
    SIRIDB_NODE_ENTER[CLERI_GID_SELECT_STMT] = enter_select_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_SET_EXPRESSION] = enter_set_expression;
    SIRIDB_NODE_ENTER[CLERI_GID_SET_IGNORE_THRESHOLD] = enter_set_ignore_threshold;
    SIRIDB_NODE_ENTER[CLERI_GID_SET_NAME] = enter_set_name;
    SIRIDB_NODE_ENTER[CLERI_GID_SET_PASSWORD] = enter_set_password;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_SERVER_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_ALL] = enter_series_all;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_NAME] = enter_series_name;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_MATCH] = enter_series_match;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_PARENTHESES] = enter_series_parentheses;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_RE] = enter_series_re;
    SIRIDB_NODE_ENTER[CLERI_GID_SERIES_SETOPR] = enter_series_setopr;
    SIRIDB_NODE_ENTER[CLERI_GID_SHARD_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_TAG_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_TAG_SERIES] = enter_tag_series;
    SIRIDB_NODE_ENTER[CLERI_GID_TIMEIT_STMT] = enter_timeit_stmt;
    SIRIDB_NODE_ENTER[CLERI_GID_UNTAG_SERIES] = enter_untag_series;
    SIRIDB_NODE_ENTER[CLERI_GID_USER_COLUMNS] = enter_xxx_columns;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_GROUP] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_POOL] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_SERIES] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_SERVER] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_SHARD] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_TAG] = enter_where_xxx;
    SIRIDB_NODE_ENTER[CLERI_GID_WHERE_USER] = enter_where_xxx;


    SIRIDB_NODE_EXIT[CLERI_GID_AFTER_EXPR] = exit_after_expr;
    SIRIDB_NODE_EXIT[CLERI_GID_ALTER_GROUP] = exit_alter_group;
    SIRIDB_NODE_EXIT[CLERI_GID_ALTER_TAG] = exit_alter_tag;
    SIRIDB_NODE_EXIT[CLERI_GID_ALTER_USER] = exit_alter_user;
    SIRIDB_NODE_EXIT[CLERI_GID_BEFORE_EXPR] = exit_before_expr;
    SIRIDB_NODE_EXIT[CLERI_GID_BETWEEN_EXPR] = exit_between_expr;
    SIRIDB_NODE_EXIT[CLERI_GID_CALC_STMT] = exit_calc_stmt;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_GROUPS] = exit_count_groups;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_POOLS] = exit_count_pools;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SERIES_LENGTH] = exit_count_series_length;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SERIES] = exit_count_series;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SERVERS_RECEIVED] = exit_count_servers_received;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SERVERS_SELECTED] = exit_count_servers_selected;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SERVERS] = exit_count_servers;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SHARDS_SIZE] = exit_count_shards_size;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_SHARDS] = exit_count_shards;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_TAGS] = exit_count_tags;
    SIRIDB_NODE_EXIT[CLERI_GID_COUNT_USERS] = exit_count_users;
    SIRIDB_NODE_EXIT[CLERI_GID_CREATE_GROUP] = exit_create_group;
    SIRIDB_NODE_EXIT[CLERI_GID_CREATE_USER] = exit_create_user;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_GROUP] = exit_drop_group;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_SERIES] = exit_drop_series;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_SERVER] = exit_drop_server;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_SHARDS] = exit_drop_shards;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_TAG] = exit_drop_tag;
    SIRIDB_NODE_EXIT[CLERI_GID_DROP_USER] = exit_drop_user;
    SIRIDB_NODE_EXIT[CLERI_GID_GRANT_USER] = exit_grant_user;
    SIRIDB_NODE_EXIT[CLERI_GID_HEAD_EXPR] = exit_head_expr;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_GROUPS] = exit_list_groups;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_POOLS] = exit_list_pools;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_SERIES] = exit_list_series;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_SERVERS] = exit_list_servers;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_SHARDS] = exit_list_shards;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_TAGS] = exit_list_tags;
    SIRIDB_NODE_EXIT[CLERI_GID_LIST_USERS] = exit_list_users;
    SIRIDB_NODE_EXIT[CLERI_GID_REVOKE_USER] = exit_revoke_user;
    SIRIDB_NODE_EXIT[CLERI_GID_SELECT_AGGREGATE] = exit_select_aggregate;
    SIRIDB_NODE_EXIT[CLERI_GID_SELECT_STMT] = exit_select_stmt;
    SIRIDB_NODE_EXIT[CLERI_GID_SERIES_MATCH] = exit_series_match;
    SIRIDB_NODE_EXIT[CLERI_GID_SERIES_PARENTHESES] = exit_series_parentheses;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_ADDRESS] = exit_set_address;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_BACKUP_MODE] = exit_set_backup_mode;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_DROP_THRESHOLD] = exit_set_drop_threshold;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_EXPIRATION_LOG] = exit_set_expiration_log;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_EXPIRATION_NUM] = exit_set_expiration_num;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_LIST_LIMIT] = exit_set_list_limit;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_LOG_LEVEL] = exit_set_log_level;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_PORT] = exit_set_port;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_SELECT_POINTS_LIMIT] = exit_set_select_points_limit;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_TEE] = exit_set_tee;
    SIRIDB_NODE_EXIT[CLERI_GID_SET_TIMEZONE] = exit_set_timezone;
    SIRIDB_NODE_EXIT[CLERI_GID_SHOW_STMT] = exit_show_stmt;
    SIRIDB_NODE_EXIT[CLERI_GID_TAG_SERIES] = exit_tag_series;
    SIRIDB_NODE_EXIT[CLERI_GID_TAIL_EXPR] = exit_tail_expr;
    SIRIDB_NODE_EXIT[CLERI_GID_TIMEIT_STMT] = exit_timeit_stmt;
    SIRIDB_NODE_EXIT[CLERI_GID_UNTAG_SERIES] = exit_untag_series;

    for (i = HELP_OFFSET; i < HELP_OFFSET + HELP_COUNT; i++)
    {
        SIRIDB_NODE_EXIT[i] = exit_help_xxx;
    }
}

/******************************************************************************
 * Enter functions
 *****************************************************************************/

static void enter_access_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    /* bind ACCESS_EXPR children to query */
    query->data = query->nodes->node->children;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_group(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * group_node = \
            cleri_gn(query->nodes->node->children->next);
    siridb_group_t * group;

    char name[group_node->len - 1];
    xstr_extract_string(name, group_node->str, group_node->len);

    if ((group = ct_get(siridb->groups->groups, name)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find group: '%s'",
                name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        q_alter->alter_tp = QUERY_ALTER_GROUP;
        q_alter->via.group = group;
        siridb_group_incref(group);

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_alter_tag(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * tag_node = cleri_gn(query->nodes->node->children->next);
    siridb_tag_t * tag;

    char name[tag_node->len - 1];
    xstr_extract_string(name, tag_node->str, tag_node->len);

    if ((tag = ct_get(siridb->tags->tags, name)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find tag: '%s'",
                name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        q_alter->alter_tp = QUERY_ALTER_TAG;
        q_alter->via.tag = tag;
        siridb_tag_incref(tag);

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_alter_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    q_alter->alter_tp = QUERY_ALTER_SERIES;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_server(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;
    siridb_server_t * server = siridb_server_from_node(
            siridb,
            cleri_gn(cleri_gn(query->nodes->node->children->next)->children),
            query->err_msg);

    if (server == NULL)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        q_alter->alter_tp = QUERY_ALTER_SERVER;
        q_alter->via.server = server;
        siridb_server_incref(server);

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_alter_servers(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    ((query_alter_t *) query->data)->alter_tp = QUERY_ALTER_SERVERS;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_ALTER)

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(1024);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_alter_new();

    if (query->data == NULL)
    {
        MEM_ERR_RET
    }
    query->free_cb = (uv_close_cb) query_alter_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * user_node =
            cleri_gn(query->nodes->node->children->next);
    query_alter_t * q_alter = (query_alter_t *) query->data;
    siridb_user_t * user;

    char name[user_node->len - 1];
    xstr_extract_string(name, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb, name, NULL)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'",
                name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        q_alter->alter_tp = QUERY_ALTER_USER;
        q_alter->via.user = user;
        siridb_user_incref(user);

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_count_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_COUNT)

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(256);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_count_new();

    if (query->data == NULL)
    {
        MEM_ERR_RET
    }

    query->free_cb = (uv_close_cb) query_count_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_create_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_CREATE)

    SIRIPARSER_NEXT_NODE
}

static void enter_create_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    /* bind user object to data and set correct free call */
    query_alter_t * q_alter = query_alter_new();

    query->data = q_alter;

    if (q_alter == NULL)
    {
        MEM_ERR_RET
    }

    query->free_cb = (uv_close_cb) query_alter_free;
    q_alter->via.user = siridb_user_new();

    if (q_alter->via.user == NULL)
    {
        MEM_ERR_RET
    }

    q_alter->alter_tp = QUERY_ALTER_USER;

    SIRIPARSER_NEXT_NODE
}

static void enter_drop_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * db_user = query->client->origin;

    assert (query->packer == NULL);

    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_DROP)

    query->packer = sirinet_packer_new(1024);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_drop_new();

    if (query->data == NULL)
    {
        MEM_ERR_RET
    }

    query->free_cb = (uv_close_cb) query_drop_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_grant_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_GRANT)
    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * user_node =
            cleri_gn(query->nodes->node->children->next);
    siridb_user_t * user;
    char username[user_node->len - 1];
    xstr_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb, username, NULL)) == NULL)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'", username);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        user->access_bit |=
                siridb_access_from_children((cleri_children_t *) query->data);

        query_alter_t * q_alter = query->data = query_alter_new();
        if (q_alter == NULL)
        {
            MEM_ERR_RET
        }

        siridb_user_incref(user);

        query->free_cb = (uv_close_cb) query_alter_free;
        q_alter->alter_tp = QUERY_ALTER_USER;
        q_alter->via.user = user;

        SIRIPARSER_NEXT_NODE
    }
}
static void enter_group_tag_match(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    cleri_node_t * node = query->nodes->node;
    query_wrapper_t * q_wrapper = query->data;
    siridb_group_t * group;
    siridb_tag_t * tag = NULL;


    /* we must send this query to all pools */
    if (q_wrapper->pmap != NULL)
    {
        imap_free(q_wrapper->pmap, NULL);
        q_wrapper->pmap = NULL;
    }

    char group_or_tag_name[node->len - 1];

    /* extract series name */
    xstr_extract_string(group_or_tag_name, node->str, node->len);

    if ((group = ct_get(siridb->groups->groups, group_or_tag_name)) == NULL &&
        (tag = ct_get(siridb->tags->tags, group_or_tag_name)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find group or tag '%s'",
                group_or_tag_name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        siridb_series_t * series;
        size_t i;

        q_wrapper->series_tmp = (q_wrapper->update_cb == NULL) ?
                q_wrapper->series_map : imap_new();

        if (q_wrapper->series_tmp == NULL)
        {
            MEM_ERR_RET
        }

        if (group)
        {
            uv_mutex_lock(&siridb->groups->mutex);

            for (i = 0; i < group->series->len; i++)
            {
                series = (siridb_series_t *) group->series->data[i];
                siridb_series_incref(series);
                if (imap_add(q_wrapper->series_tmp, series->id, series))
                {
                    log_critical("Cannot add series to temporary map.");
                    siridb_series_decref(series);
                }
            }

            uv_mutex_unlock(&siridb->groups->mutex);
        }
        else if (tag)  /* check to prevent compile warning */
        {
            vec_t * tag_series;

            assert (tag != NULL);

            uv_mutex_lock(&siridb->tags->mutex);

            tag_series = imap_vec(tag->series);

            if (tag_series != NULL)
            {
                for (i = 0; i < tag_series->len; i++)
                {
                    series = (siridb_series_t *) tag_series->data[i];
                    siridb_series_incref(series);
                    if (imap_add(q_wrapper->series_tmp, series->id, series))
                    {
                        log_critical("Cannot add series to temporary map.");
                        siridb_series_decref(series);
                    }
                }
            }

            uv_mutex_unlock(&siridb->tags->mutex);
        }

        if (q_wrapper->update_cb != NULL)
        {
            (*q_wrapper->update_cb)(
                    q_wrapper->series_map,
                    q_wrapper->series_tmp,
                    (imap_free_cb) &siridb__series_decref);
        }

        q_wrapper->series_tmp = NULL;

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void enter_help(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    cleri_node_t * node = query->nodes->node;

    query->data = strndup(node->str, node->len);

    if (query->data == NULL)
    {
        MEM_ERR_RET
    }

    query->free_cb = (uv_close_cb) query_help_free;

    xstr_split_join(query->data, ' ', '_');

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_limit_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_list_t * qlist = (query_list_t *) query->data;
    int64_t limit = CLERI_NODE_DATA(cleri_gn(query->nodes->node->children->next));

    if (limit <= 0 || limit > siridb->list_limit)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Limit must be a value between 1 and %" PRIu32
                " but received: %" PRId64
                " (optionally the limit can be changed, "
                "see 'help alter database')",
                siridb->list_limit,
                limit);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        qlist->limit = limit;
        SIRIPARSER_NEXT_NODE
    }
}

static void enter_list_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_LIST)

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(QP_SUGGESTED_SIZE);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    qp_add_raw(query->packer, (const unsigned char *) "columns", 7);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    query->data = query_list_new();

    if (query->data == NULL)
    {
        MEM_ERR_RET
    }

    query->free_cb = (uv_close_cb) query_list_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_merge_as(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;
    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    q_select->merge_as = malloc(node->len - 1);

    if (q_select->merge_as == NULL)
    {
        MEM_ERR_RET
    }

    xstr_extract_string(q_select->merge_as, node->str, node->len);

    if (IS_MASTER && query->nodes->node->children->next->next->next != NULL)
    {
        q_select->mlist = siridb_aggregate_list(
                cleri_gn(cleri_gn(cleri_gn(
                    query->nodes->node->children->next->next->next)->
                    children)->children->next)->children,
                query->err_msg);

        if (q_select->mlist == NULL)
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_revoke_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_REVOKE)
    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * user_node =
            cleri_gn(query->nodes->node->children->next);
    siridb_user_t * user;
    char username[user_node->len - 1];
    xstr_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb, username, NULL)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'",
                username);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        user->access_bit &=
                ~siridb_access_from_children((cleri_children_t *) query->data);

        query_alter_t * q_alter = query->data = query_alter_new();

        if (q_alter == NULL)
        {
            MEM_ERR_RET
        }

        siridb_user_incref(user);

        query->free_cb = (uv_close_cb) query_alter_free;
        q_alter->alter_tp = QUERY_ALTER_USER;
        q_alter->via.user = user;

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_select_t * q_select;
    cleri_children_t * child;
    int skip_get_points;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_SELECT)
    MASTER_CHECK_ACCESSIBLE(siridb)

    assert (query->packer == NULL && query->data == NULL);

    query->data = q_select = query_select_new();

    if (q_select == NULL)
    {
        MEM_ERR_RET
    }

    /* this is not critical since pmap is allowed to be NULL */
    ((query_select_t *) query->data)->pmap =
            (!IS_MASTER || siridb_is_reindexing(siridb)) ?
                    NULL : imap_new();

    /* child is always the ',' and child->next the node */
    child = cleri_gn(query->nodes->node->children->next)->children;
    skip_get_points = siridb_aggregate_can_skip(child);

    child = child->next;
    while (child != NULL)
    {
        if (skip_get_points && !siridb_aggregate_can_skip(child->next))
        {
            skip_get_points = 0;
        }
        q_select->nselects++;
        child = child->next->next;
    }

    if (skip_get_points)
    {
        q_select->flags |= QUERIES_SKIP_GET_POINTS;
    }

    query->free_cb = (uv_close_cb) query_select_free;
    query->packer = sirinet_packer_new(QP_SUGGESTED_SIZE);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_set_expression(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    query_alter_t * q_alter = (query_alter_t *) query->data;

    if (siridb_group_update_expression(
            siridb->groups,
            q_alter->via.group,
            node->str,
            node->len,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void enter_set_ignore_threshold(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;

    if (    cleri_gn(cleri_gn(
            query->nodes->node->children->next->next)->children)->
            cl_obj->gid == CLERI_GID_K_TRUE)
    {
        q_wrapper->flags |= QUERIES_IGNORE_DROP_THRESHOLD;
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_set_name(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    cleri_node_t * name_node =
            cleri_gn(query->nodes->node->children->next->next);

    char name[name_node->len - 1];
    xstr_extract_string(name, name_node->str, name_node->len);

    query_alter_t * q_alter = (query_alter_t *) query->data;
    switch (q_alter->alter_tp)
    {
    case QUERY_ALTER_USER:
        if (siridb_user_set_name(
                    siridb,
                    q_alter->via.user,
                    name,
                    query->err_msg))
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
        break;
    case QUERY_ALTER_GROUP:
        if (siridb_group_set_name(
                    siridb,
                    q_alter->via.group,
                    name,
                    query->err_msg))
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
        break;
    case QUERY_ALTER_TAG:
        if (siridb_tag_set_name(
                    siridb,
                    q_alter->via.tag,
                    name,
                    query->err_msg))
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
        break;
    case QUERY_ALTER_NONE:
    case QUERY_ALTER_DATABASE:
    case QUERY_ALTER_SERVER:
    default:
        assert (0);
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_set_password(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_user_t * user = ((query_alter_t *) query->data)->via.user;

    cleri_node_t * pw_node =
            cleri_gn(query->nodes->node->children->next->next);

    char password[pw_node->len - 1];
    xstr_extract_string(password, pw_node->str, pw_node->len);

    if (siridb_user_set_password(user, password, query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

static void enter_series_name(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cleri_node_t * node = query->nodes->node;
    siridb_t * siridb = query->siridb;
    query_wrapper_t * q_wrapper = query->data;
    siridb_series_t * series = NULL;
    uint16_t pool;
    char series_name[node->len - 1];

    /* extract series name */
    xstr_extract_string(series_name, node->str, node->len);

    if (siridb_is_reindexing(siridb))
    {
        series = (siridb_series_t *) ct_get(siridb->series, series_name);
    }
    else
    {
        /* get pool for series name */
        pool = siridb_lookup_sn(siridb->pools->lookup, series_name);

        /* check if this series belongs to 'this' pool and if so get the series */
        if (pool == siridb->server->pool)
        {
            series = (siridb_series_t *) ct_get(siridb->series, series_name);
#ifdef SERIESMUSTEXIST
            if (series == NULL)
            {
                /* the series does not exist */
                snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                        "Cannot find series: '%s'", series_name);

                /* free series_name and return with send_errror.. */
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
#endif
        }
        else if (q_wrapper->pmap != NULL && imap_set(
                q_wrapper->pmap,
                pool,
                (siridb_pool_t *) (siridb->pools->pool + pool)) < 0)
        {
            log_critical("Cannot add pool to pool map.");
        }
    }

    if (series == NULL)
    {
        if (q_wrapper->update_cb == &imap_intersection_ref)
        {
            imap_free(
                    q_wrapper->series_map,
                    (imap_free_cb) &siridb__series_decref);

            q_wrapper->series_map = imap_new();

            if (q_wrapper->series_map == NULL)
            {
                MEM_ERR_RET
            }
        }
    }
    else
    {
        if (    q_wrapper->update_cb == NULL ||
                q_wrapper->update_cb == &imap_union_ref)
        {
            if (imap_set(q_wrapper->series_map, series->id, series) == 1)
            {
                siridb_series_incref(series);
            }
        }
        else if (q_wrapper->update_cb == &imap_difference_ref)
        {
            series = (siridb_series_t *) imap_pop(
                    q_wrapper->series_map,
                    series->id);
            if (series != NULL)
            {
                siridb_series_decref(series);
            }
        }
        else if (q_wrapper->update_cb == &imap_intersection_ref)
        {
            series = (siridb_series_t *) imap_get(
                    q_wrapper->series_map,
                    series->id);

            if (series != NULL)
            {
                siridb_series_incref(series);
            }

            imap_free(
                    q_wrapper->series_map,
                    (imap_free_cb) &siridb__series_decref);

            q_wrapper->series_map = imap_new();

            if (q_wrapper->series_map == NULL)
            {
                if (series != NULL)
                {
                    siridb_series_decref(series);
                }
                MEM_ERR_RET
            }

            if (series != NULL)
            {
                if (imap_set(q_wrapper->series_map, series->id, series) != 1)
                {
                    siridb_series_decref(series);
                    MEM_ERR_RET
                }
            }
        }
        else if (q_wrapper->update_cb == &imap_symmetric_difference_ref)
        {
            switch (imap_set(q_wrapper->series_map, series->id, series))
            {
            case 0:
                series = (siridb_series_t *) imap_pop(
                        q_wrapper->series_map,
                        series->id);
                siridb_series_decref(series);
                break;

            case 1:
                siridb_series_incref(series);
                break;

            default:
                MEM_ERR_RET
            }
        }
        else
        {
            /* we should not get here */
            assert (0);
        }
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_series_match(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;

    if ((q_wrapper->series_map = imap_new()) == NULL)
    {
        MEM_ERR_RET
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_series_parentheses(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;
    siridb_sset_t * sset;

    assert (q_wrapper->series_map != NULL);

    if (q_wrapper->sset_vec == NULL)
    {
        if ((q_wrapper->sset_vec = vec_new(1)) == NULL)
        {
            MEM_ERR_RET
        }
    }

    if (q_wrapper->update_cb != NULL)
    {
        sset = siridb_sset_new(q_wrapper->series_map, q_wrapper->update_cb);
        if (sset == NULL ||
            vec_append_safe(&q_wrapper->sset_vec, sset) ||
            (q_wrapper->series_map = imap_new()) == NULL)
        {
            MEM_ERR_RET
        }

        q_wrapper->update_cb = NULL;
    }
    else
    {
        sset = siridb_sset_new(NULL, NULL);
        if (sset == NULL || vec_append_safe(&q_wrapper->sset_vec, sset))
        {
            MEM_ERR_RET
        }
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_series_all(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_series_t * series;
    query_wrapper_t * q_wrapper = query->data;

    /* we must send this query to all pools */
    if (q_wrapper->pmap != NULL)
    {
        imap_free(q_wrapper->pmap, NULL);
        q_wrapper->pmap = NULL;
    }

    uv_mutex_lock(&siridb->series_mutex);

    q_wrapper->vec = imap_2vec_ref(
            (   q_wrapper->update_cb == NULL ||
                q_wrapper->update_cb == &imap_union_ref ||
                q_wrapper->update_cb == &imap_symmetric_difference_ref) ?
                    siridb->series_map : q_wrapper->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    q_wrapper->series_tmp = (q_wrapper->update_cb == NULL) ?
            q_wrapper->series_map : imap_new();

    if (q_wrapper->vec == NULL || q_wrapper->series_tmp == NULL)
    {
        MEM_ERR_RET
    }

    for (q_wrapper->vec_index = 0;
         q_wrapper->vec_index < q_wrapper->vec->len;
         ++q_wrapper->vec_index)
    {
        series = q_wrapper->vec->data[q_wrapper->vec_index];
        if (imap_add(q_wrapper->series_tmp, series->id, series))
        {
            MEM_ERR_RET
        }
    }

    vec_free(q_wrapper->vec);
    q_wrapper->vec = NULL;
    q_wrapper->vec_index = 0;

    if (q_wrapper->update_cb != NULL)
    {
        (*q_wrapper->update_cb)(
                q_wrapper->series_map,
                q_wrapper->series_tmp,
                (imap_free_cb) &siridb__series_decref);
    }

    q_wrapper->series_tmp = NULL;
    SIRIPARSER_ASYNC_NEXT_NODE
}

static void enter_series_re(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    cleri_node_t * node = query->nodes->node;
    query_wrapper_t * q_wrapper = query->data;

    /* we must send this query to all pools */
    if (q_wrapper->pmap != NULL)
    {
        imap_free(q_wrapper->pmap, NULL);
        q_wrapper->pmap = NULL;
    }

    /* extract and compile regular expression */
    if (siridb_re_compile(
            &q_wrapper->regex,
            &q_wrapper->match_data,
            node->str,
            node->len,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uv_mutex_lock(&siridb->series_mutex);

        q_wrapper->vec = imap_2vec_ref(
                (   q_wrapper->update_cb == NULL ||
                    q_wrapper->update_cb == &imap_union_ref ||
                    q_wrapper->update_cb == &imap_symmetric_difference_ref) ?
                        siridb->series_map : q_wrapper->series_map);

        uv_mutex_unlock(&siridb->series_mutex);

        q_wrapper->series_tmp = (q_wrapper->update_cb == NULL) ?
                q_wrapper->series_map : imap_new();

        if (q_wrapper->vec == NULL || q_wrapper->series_tmp == NULL)
        {
            MEM_ERR_RET
        }

        uv_async_t * next = malloc(sizeof(uv_async_t));

        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_series_re);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }

    /* handle is handled or a signal is raised */
}

static void enter_series_setopr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;

    switch (cleri_gn(query->nodes->node->children)->cl_obj->gid)
    {
    case CLERI_GID_K_UNION:
        q_wrapper->update_cb = &imap_union_ref;
        break;
    case CLERI_GID_K_INTERSECTION:
        q_wrapper->update_cb = &imap_intersection_ref;
        break;
    case CLERI_GID_C_DIFFERENCE:
        q_wrapper->update_cb = &imap_difference_ref;
        break;
    case CLERI_GID_K_SYMMETRIC_DIFFERENCE:
        q_wrapper->update_cb = &imap_symmetric_difference_ref;
        break;
    default:
        assert (0);
        break;
    }

    SIRIPARSER_NEXT_NODE
}

typedef struct
{
    cexpr_t * where_expr;
    siridb_tag_t * tag;
} LISTENER_tag_t;

static int LISTENER_tag__cb(siridb_series_t * series, LISTENER_tag_t * w)
{
    int rc = cexpr_run(
        w->where_expr,
        (cexpr_cb_t) siridb_series_cexpr_cb,
        series);

    if (rc == 0 || imap_add(w->tag->series, series->id, series) != 0)
    {
        siridb_series_decref(series);
    }
    return rc;
}

static int LISTENER_untag__cb(siridb_series_t * series, LISTENER_tag_t * w)
{
    int rc = cexpr_run(
        w->where_expr,
        (cexpr_cb_t) siridb_series_cexpr_cb,
        series);

    if (rc == 1 && imap_pop(w->tag->series, series->id) == series)
    {
        siridb_series_decref(series);
    }

    siridb_series_decref(series);
    return rc;
}

static void enter_tag_series(uv_async_t * handle)
{

    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)
    MASTER_CHECK_VERSION(siridb, "2.0.38")

    cleri_node_t * tag_node =
            cleri_gn(query->nodes->node->children->next);
    siridb_tag_t * tag;
    char name[tag_node->len - 1];
    xstr_extract_string(name, tag_node->str, tag_node->len);

    tag = ct_get(siridb->tags->tags, name);
    if (tag == NULL)
    {
        if (ct_get(siridb->groups->groups, name) != NULL)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Cannot create tag `%s` because a group with this name "
                    "already exist.",
                    name);
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        if (siridb_tag_check_name(name, query->err_msg) != 0)
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        uv_mutex_lock(&siridb->tags->mutex);

        tag = siridb_tags_add(siridb->tags, name);

        if (tag == NULL)
        {
            uv_mutex_unlock(&siridb->tags->mutex);
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Unexpected error while creating tag: `%s`",
                    name);
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
    }
    else
    {
        uv_mutex_lock(&siridb->tags->mutex);
    }

    if (q_alter->where_expr == NULL)
    {
        q_alter->n = q_alter->series_map->len;

        imap_union_ref(
                tag->series,
                q_alter->series_map,
                (imap_free_cb) &siridb__series_decref);
    }
    else
    {
        LISTENER_tag_t w = {
                .where_expr = q_alter->where_expr,
                .tag = tag,
        };

        q_alter->n = imap_walk(
                q_alter->series_map, (imap_cb) LISTENER_tag__cb, &w);

        imap_free(q_alter->series_map, NULL);
    }

    siridb_tags_set_require_save(siridb->tags, tag);

    uv_mutex_unlock(&siridb->tags->mutex);

    q_alter->series_map = NULL;

    QP_ADD_SUCCESS

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_tag_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_alter->n);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void enter_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query->timeit = qp_packer_new(512);

    if (query->timeit == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_raw(query->timeit, (const unsigned char *) "__timeit__", 10);
    qp_add_type(query->timeit, QP_ARRAY_OPEN);

    SIRIPARSER_NEXT_NODE
}

static void enter_untag_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = query->siridb;
    query_alter_t * q_alter = (query_alter_t *) query->data;

    q_alter->tp = QUERY_ALTER_SERIES;

    MASTER_CHECK_ACCESSIBLE(siridb)
    MASTER_CHECK_VERSION(siridb, "2.0.38")

    cleri_node_t * tag_node =
            cleri_gn(query->nodes->node->children->next);
    siridb_tag_t * tag;

    char name[tag_node->len - 1];
    xstr_extract_string(name, tag_node->str, tag_node->len);

    tag = ct_get(siridb->tags->tags, name);

    if (tag == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find tag: '%s'",
                name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    uv_mutex_lock(&siridb->tags->mutex);

    if (q_alter->where_expr == NULL)
    {
        q_alter->n = q_alter->series_map->len;

        imap_difference_ref(
                tag->series,
                q_alter->series_map,
                (imap_free_cb) &siridb__series_decref);
    }
    else
    {
        LISTENER_tag_t w = {
                .where_expr = q_alter->where_expr,
                .tag = tag,
        };

        q_alter->n = imap_walk(
                q_alter->series_map, (imap_cb) LISTENER_untag__cb, &w);

        imap_free(q_alter->series_map, NULL);
    }


    siridb_tags_set_require_save(siridb->tags, tag);

    uv_mutex_unlock(&siridb->tags->mutex);

    q_alter->series_map = NULL;

    QP_ADD_SUCCESS

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_tag_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_alter->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void enter_where_xxx(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cexpr_t * cexpr =
            cexpr_from_node(cleri_gn(query->nodes->node->children->next));

    if (cexpr == NULL)
    {
        sprintf(query->err_msg, "Max depth reached in 'where' expression!");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        ((query_wrapper_t *) query->data)->where_expr = cexpr;
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void enter_xxx_columns(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cleri_children_t * columns = query->nodes->node->children;
    query_list_t * qlist = (query_list_t *) query->data;

    qlist->props = vec_new(DEFAULT_ALLOC_COLUMNS);

    if (qlist->props == NULL)
    {
        MEM_ERR_RET
    }

    while (1)
    {
        qp_add_raw(
                query->packer,
                (const unsigned char *) cleri_gn(columns)->str,
                cleri_gn(columns)->len);

        if (vec_append_safe(
                &qlist->props,
                &cleri_gn(cleri_gn(columns)->children)->cl_obj->gid))
        {
            MEM_ERR_RET
        }

        if (columns->next == NULL)
        {
            break;
        }

        columns = columns->next->next;
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

/******************************************************************************
 * Exit functions
 *****************************************************************************/

static void exit_after_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    ((query_select_t *) query->data)->start_ts =
            (uint64_t *) CLERI_NODE_DATA_ADDR(
                    cleri_gn(query->nodes->node->children->next));

    SIRIPARSER_NEXT_NODE
}

static void exit_head_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    ssize_t * head = (ssize_t *) CLERI_NODE_DATA_ADDR(
                cleri_gn(query->nodes->node->children->next));


    if (*head <= 0 || *head > MAX_HEADTAIL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Head must be a value between 1 and %ld, got %zd",
                MAX_HEADTAIL, *head);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    ((query_select_t *) query->data)->headtail = *head;

    SIRIPARSER_NEXT_NODE
}

static void exit_tail_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    ssize_t * tail = (ssize_t *) CLERI_NODE_DATA_ADDR(
                cleri_gn(query->nodes->node->children->next));

    if (*tail < 1 || *tail > MAX_HEADTAIL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Tail must be a value between 1 and %ld, got %zd",
                MAX_HEADTAIL, *tail);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    ((query_select_t *) query->data)->headtail = -(*tail);

    SIRIPARSER_NEXT_NODE
}

static void exit_alter_group(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    if (siridb_groups_save(siridb->groups))
    {
        FILE_ERR_RET
    }

    QP_ADD_SUCCESS
    char * name = ((query_alter_t *) query->data)->via.group->name;
    log_info(MSG_SUCCESS_ALTER_GROUP, name);
    qp_add_fmt_safe(query->packer, MSG_SUCCESS_ALTER_GROUP, name);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response,
                0);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_alter_tag(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_tag_t * tag = ((query_alter_t *) query->data)->via.tag;

    siridb_tags_set_require_save(siridb->tags, tag);

    QP_ADD_SUCCESS

    log_info(MSG_SUCCESS_ALTER_TAG, tag->name);
    qp_add_fmt_safe(query->packer, MSG_SUCCESS_ALTER_TAG, tag->name);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response,
                0);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_alter_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    if (siridb_users_save(siridb))
    {
        FILE_ERR_RET
    }

    QP_ADD_SUCCESS
    char * name = ((query_alter_t *) query->data)->via.user->name;
    log_info(MSG_SUCCESS_ALTER_USER, name);
    qp_add_fmt_safe(query->packer, MSG_SUCCESS_ALTER_USER, name);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response,
                0);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_before_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    ((query_select_t *) query->data)->end_ts =
            (uint64_t *) CLERI_NODE_DATA_ADDR(
                    cleri_gn(query->nodes->node->children->next));

    SIRIPARSER_NEXT_NODE
}

static void exit_between_expr(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;

    q_select->start_ts = (uint64_t *) CLERI_NODE_DATA_ADDR(
            cleri_gn(query->nodes->node->children->next));

    q_select->end_ts = (uint64_t *) CLERI_NODE_DATA_ADDR(
            cleri_gn(query->nodes->node->children->next->next->next));

    if (*q_select->start_ts > *q_select->end_ts)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Start time (%" PRIu64 ") "
                "should not be greater than end time (%" PRIu64 ")",
                *q_select->start_ts,
                *q_select->end_ts);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_calc_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cleri_node_t * calc_node = query->nodes->node;

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(64);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);
    qp_add_raw(query->packer, (const unsigned char *) "calc", 4);

    if (!query->factor)
    {
        qp_add_int64(query->packer, CLERI_NODE_DATA(calc_node));
    }
    else
    {
        double factor = (double) query->factor;
        qp_add_int64(
                query->packer,
                (int64_t) ((CLERI_NODE_DATA(calc_node) * factor)));
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_count_groups(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    if (q_count->where_expr == NULL || !cexpr_contains(
            q_count->where_expr,
            siridb_group_is_remote_prop))
    {
        finish_count_groups(handle);
    }
    else
    {
        sirinet_pkg_t * pkg = sirinet_pkg_new(0, 0, BPROTO_REQ_GROUPS, NULL);

        if (pkg != NULL)
        {
            siri_async_incref(handle);

            query->nodes->cb = (uv_async_cb) finish_count_groups;

            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    (sirinet_promises_cb) on_groups_response,
                    handle,
                    0);
        }
    }
}

static void exit_count_pools(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    siridb_pool_t * pool = siridb->pools->pool + siridb->server->pool;

    siridb_pool_walker_t wpool = {
            .servers=pool->len,
            .series=siridb->series->len,
            .pool=siridb->server->pool
    };

    qp_add_raw(query->packer, (const unsigned char *) "pools", 5);

    if (q_count->where_expr == NULL)
    {
        qp_add_int64(query->packer, siridb->pools->len);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
    else
    {
        MASTER_CHECK_ACCESSIBLE(siridb)

        q_count->n = cexpr_run(
                q_count->where_expr,
                (cexpr_cb_t) siridb_pool_cexpr_cb,
                &wpool);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_POOLS,
                    (sirinet_promises_cb) on_count_xxx_response,
                    0);
        }
        else
        {
            qp_add_int64(query->packer, q_count->n);
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_count_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    MASTER_CHECK_ONLINE(siridb)

    qp_add_raw(query->packer, (const unsigned char *) "series", 6);

    if (q_count->where_expr == NULL)
    {
        q_count->n = (q_count->series_map == NULL) ?
                siridb->series_map->len : q_count->series_map->len;

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_POOLS,
                    (sirinet_promises_cb) on_count_xxx_response,
                    0);
        }
        else
        {
            qp_add_int64(query->packer, q_count->n);

            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
    else
    {
        uv_mutex_lock(&siridb->series_mutex);

        q_count->vec = imap_2vec_ref(
                (q_count->series_map == NULL) ?
                        siridb->series_map : q_count->series_map);

        uv_mutex_unlock(&siridb->series_mutex);

        if (q_count->vec == NULL)
        {
            MEM_ERR_RET
        }

        uv_async_t * next = malloc(sizeof(uv_async_t));
        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_count_series);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
}

static void exit_count_series_length(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    qp_add_raw(query->packer, (const unsigned char *) "series_length", 13);

    if (q_count->where_expr == NULL)
    {
        size_t i;
        vec_t * vec;
        siridb_series_t * series;

        uv_mutex_lock(&siridb->series_mutex);

        vec = imap_2vec((q_count->series_map == NULL) ?
                siridb->series_map : q_count->series_map);

        uv_mutex_unlock(&siridb->series_mutex);

        if (vec == NULL)
        {
            MEM_ERR_RET
        }

        for (i = 0; i < vec->len; i++)
        {
            series = (siridb_series_t *) vec->data[i];
            q_count->n += series->length;
        }

        vec_free(vec);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_POOLS,
                    (sirinet_promises_cb) on_count_xxx_response,
                    0);
        }
        else
        {
            qp_add_int64(query->packer, q_count->n);

            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
    else
    {
        uv_async_t * next;

        uv_mutex_lock(&siridb->series_mutex);

        q_count->vec = imap_2vec_ref(
                (q_count->series_map == NULL) ?
                        siridb->series_map : q_count->series_map);

        uv_mutex_unlock(&siridb->series_mutex);

        if (q_count->vec == NULL)
        {
            MEM_ERR_RET
        }

        next = malloc(sizeof(uv_async_t));

        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_count_series_length);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
}

static void exit_count_servers(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    cexpr_t * where_expr = q_count->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_server_cexpr_cb;
    int is_local = IS_MASTER;

    qp_add_raw(query->packer, (const unsigned char *) "servers", 7);


    /* if is_local, check if we use 'remote' props in where expression */
    if (is_local && where_expr != NULL)
    {
        is_local = !cexpr_contains(where_expr, siridb_server_is_remote_prop);
    }

    if (is_local)
    {
        llist_node_t * node;
        for (   node = siridb->servers->first;
                node != NULL;
                node = node->next)
        {
            siridb_server_walker_t wserver = {node->data, siridb};
            if (where_expr == NULL || cexpr_run(where_expr, cb, &wserver))
            {
                q_count->n++;
            }
        }
    }
    else
    {
        siridb_server_walker_t wserver = {siridb->server, siridb};
        if (where_expr == NULL || cexpr_run(where_expr, cb, &wserver))
        {
            q_count->n++;
        }
    }

    if (IS_MASTER && !is_local)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_count_servers_received(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    cexpr_t * where_expr = q_count->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_server_cexpr_cb;

    qp_add_raw(
            query->packer,
            (const unsigned char *) "servers_received_points",
            23);

    siridb_server_walker_t wserver = {siridb->server, siridb};
    if (where_expr == NULL || cexpr_run(where_expr, cb, &wserver))
    {
        q_count->n += siridb->received_points;
    }

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_count_servers_selected(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    cexpr_t * where_expr = q_count->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_server_cexpr_cb;

    qp_add_raw(
            query->packer,
            (const unsigned char *) "servers_selected_points",
            23);

    siridb_server_walker_t wserver = {siridb->server, siridb};
    if (where_expr == NULL || cexpr_run(where_expr, cb, &wserver))
    {
        q_count->n += siridb->selected_points;
    }

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_count_shards(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    qp_add_raw(query->packer, (const unsigned char *) "shards", 6);

    if (q_count->where_expr == NULL)
    {
        q_count->n = siridb_shards_n(siridb);
    }
    else
    {
        uint64_t duration;
        siridb_shard_view_t vshard = {
                .server=siridb->server
        };
        size_t i;
        vec_t * shards_list;

        uv_mutex_lock(&siridb->shards_mutex);

        shards_list = siridb_shards_vec(siridb);

        uv_mutex_unlock(&siridb->shards_mutex);

        if (shards_list == NULL)
        {
            MEM_ERR_RET
        }

        for (i = 0; i < shards_list->len; i++)
        {
            vshard.shard = (siridb_shard_t *) shards_list->data[i];

            /* set start and end properties */
            duration = vshard.shard->duration;
            vshard.start = vshard.shard->id - vshard.shard->id % duration;
            vshard.end = vshard.start + duration;

            if (cexpr_run(
                    q_count->where_expr,
                    (cexpr_cb_t) siridb_shard_cexpr_cb,
                    &vshard))
            {
                q_count->n++;
            }

            siridb_shard_decref(vshard.shard);
        }

        vec_free(shards_list);
    }

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_count_shards_size(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    uint64_t duration;
    size_t i;
    vec_t * shards_list;
    siridb_shard_view_t vshard = {
            .server=siridb->server
    };

    qp_add_raw(query->packer, (const unsigned char *) "shards_size", 11);

    uv_mutex_lock(&siridb->shards_mutex);

    shards_list = siridb_shards_vec(siridb);

    uv_mutex_unlock(&siridb->shards_mutex);

    if (shards_list == NULL)
    {
        MEM_ERR_RET
    }

    for (i = 0; i < shards_list->len; i++)
    {
        vshard.shard = (siridb_shard_t *) shards_list->data[i];

        /* set start and end properties */
        duration = vshard.shard->duration;

        vshard.start = vshard.shard->id - vshard.shard->id % duration;
        vshard.end = vshard.start + duration;

        if (q_count->where_expr == NULL || cexpr_run(
                q_count->where_expr,
                (cexpr_cb_t) siridb_shard_cexpr_cb,
                &vshard))
        {
            q_count->n += vshard.shard->len;
        }

        siridb_shard_decref(vshard.shard);
    }

    vec_free(shards_list);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_count_tags(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    if (q_count->where_expr == NULL || !cexpr_contains(
            q_count->where_expr,
            siridb_tag_is_remote_prop))
    {
        finish_count_tags(handle);
    }
    else
    {
        sirinet_pkg_t * pkg = sirinet_pkg_new(0, 0, BPROTO_REQ_TAGS, NULL);

        if (pkg != NULL)
        {
            siri_async_incref(handle);

            query->nodes->cb = (uv_async_cb) finish_count_tags;

            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    (sirinet_promises_cb) on_tags_response,
                    handle,
                    0);
        }
    }
}

static void exit_count_users(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    llist_node_t * node = siridb->users->first;
    cexpr_t * where_expr = ((query_count_t *) query->data)->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_user_cexpr_cb;
    int n = 0;

    qp_add_raw(query->packer, (const unsigned char *) "users", 5);

    while (node != NULL)
    {
        if (where_expr == NULL || cexpr_run(where_expr, cb, node->data))
        {
            n++;
        }
        node = node->next;
    }

    qp_add_int64(query->packer, n);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_create_group(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    cleri_node_t * name_nd =
            cleri_gn(query->nodes->node->children->next);

    cleri_node_t * for_nd =
            cleri_gn(query->nodes->node->children->next->next->next);

    MASTER_CHECK_ACCESSIBLE(siridb)

    char group_name[name_nd->len - 1];
    xstr_extract_string(group_name, name_nd->str, name_nd->len);

    if (siridb_groups_add_group(
            siridb,
            group_name,
            for_nd->str,
            for_nd->len,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else if (siridb_groups_save(siridb->groups))
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot write groups to file: %s",
                siridb->groups->fn);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        log_critical("%s", query->err_msg);
    }
    else
    {
        assert (query->packer == NULL);
        query->packer = sirinet_packer_new(1024);
        if (query->packer == NULL)
        {
            MEM_ERR_RET
        }
        qp_add_type(query->packer, QP_MAP_OPEN);

        QP_ADD_SUCCESS
        log_info(MSG_SUCCESS_CREATE_GROUP, group_name);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_CREATE_GROUP, group_name);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_create_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_user_t * user = ((query_alter_t *) query->data)->via.user;
    cleri_node_t * user_node =
            cleri_gn(query->nodes->node->children->next);

    /* both name and packer should be NULL at this point */
    assert(user->name == NULL);
    assert(query->packer == NULL);

    MASTER_CHECK_ACCESSIBLE(siridb)

    char name[user_node->len - 1];
    xstr_extract_string(name, user_node->str, user_node->len);

    if (siridb_user_set_name(
            siridb,
            user,
            name,
            query->err_msg) ||
        siridb_users_add_user(
            siridb,
            user,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        /* success, increment the user reference counter */
        siridb_user_incref(user);

        assert (query->packer == NULL);
        query->packer = sirinet_packer_new(1024);

        if (query->packer == NULL)
        {
            MEM_ERR_RET
        }

        qp_add_type(query->packer, QP_MAP_OPEN);

        QP_ADD_SUCCESS

        log_info(MSG_SUCCESS_CREATE_USER, user->name);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_CREATE_USER, user->name);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_drop_group(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * group_node =
            cleri_gn(query->nodes->node->children->next);

    char name[group_node->len - 1];

    xstr_extract_string(name, group_node->str, group_node->len);

    if (siridb_groups_drop_group(siridb->groups, name, query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS
        log_info(MSG_SUCCESS_DROP_GROUP, name);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_DROP_GROUP, name);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_drop_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    /*
     * We transform or copy the references from imap to vec because we need
     * this list for both filtering or performing the actual drop.
     */
    uv_mutex_lock(&siridb->series_mutex);

    q_drop->vec = (q_drop->series_map == NULL) ?
        imap_2vec_ref(siridb->series_map) :
        imap_vec_pop(q_drop->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    if (q_drop->vec == NULL)
    {
        MEM_ERR_RET
    }

    if (q_drop->series_map != NULL)
    {
        /* now we can simply destroy the imap in case we had one */
        imap_free(q_drop->series_map, NULL);
        q_drop->series_map = NULL;
    }

    /*
     * This function will be called twice when using a where statement.
     * The second time the where_expr is NULL and the reason we do this is
     * so that we can honor a correct drop threshold.
     */
    if (q_drop->where_expr != NULL)
    {
        /* create a new one */
        q_drop->series_map = imap_new();

        if (q_drop->series_map == NULL)
        {
            MEM_ERR_RET
        }

        uv_async_t * next = malloc(sizeof(uv_async_t));

        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_filter_series);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);

    }
    else
    {
        double percent = siridb->series_map->len
                ? (double) q_drop->vec->len / siridb->series_map->len
                : 0.0;

        if (IS_MASTER &&
            q_drop->vec->len &&
            (~q_drop->flags & QUERIES_IGNORE_DROP_THRESHOLD) &&
            percent >= siridb->drop_threshold)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "This query would drop %0.2f%% of the series in pool %u. "
                    "Add \'set ignore_threshold true\' to the query "
                    "statement if you really want to do this.",
                    percent * 100,
                    siridb->server->pool);

            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            QP_ADD_SUCCESS

            q_drop->n = q_drop->vec->len;

            uv_async_t * next = malloc(sizeof(uv_async_t));

            if (next == NULL)
            {
                MEM_ERR_RET
            }

            next->data = handle->data;

            uv_async_init(
                    siri.loop,
                    next,
                    (uv_async_cb) async_drop_series);
            uv_async_send(next);

            uv_close((uv_handle_t *) handle, (uv_close_cb) free);
        }
    }
}

static void exit_drop_server(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_server_t * server = siridb_server_from_node(
            siridb,
            cleri_gn(cleri_gn(query->nodes->node->children->next)->children),
            query->err_msg);

    MASTER_CHECK_REINDEXING(siridb)
    MASTER_CHECK_ONLINE(siridb)

    if (server == NULL)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else if (siridb->pools->pool[server->pool].len == 1)
    {
        snprintf(
                query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot remove server '%s' because this is the only "
                "server for pool %u",
                server->name,
                server->pool);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else if (siridb->server == server || server->client != NULL)
    {
        snprintf(
                query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot remove server '%s' because the server is still "
                "online. (stop SiriDB on this server if you really want "
                "to remove the server)",
                server->name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS
        log_info(MSG_SUCCESS_DROP_SERVER, server->name);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_DROP_SERVER, server->name);

        if (IS_MASTER)
        {
            query->flags |= SIRIDB_QUERY_FLAG_REBUILD;
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    FLAG_ONLY_CHECK_ONLINE);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }

        /*
         * After calling this function, all references to the server
         * object might be gone
         */
        if (siridb_server_drop(siridb, server))
        {
            log_critical("Cannot save servers to file");
        }
    }
}

static void exit_drop_shards(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    MASTER_CHECK_ACCESSIBLE(siridb)

    uv_mutex_lock(&siridb->shards_mutex);

    q_drop->shards_list = siridb_shards_vec(siridb);

    uv_mutex_unlock(&siridb->shards_mutex);

    if (q_drop->shards_list == NULL)
    {
        MEM_ERR_RET
    }

    if (q_drop->where_expr != NULL)
    {
        uint64_t duration;
        siridb_shard_view_t vshard = {
                .server=siridb->server
        };
        size_t dropped = 0;
        size_t i;

        for (i = 0; i < q_drop->shards_list->len; i++)
        {
            vshard.shard = (siridb_shard_t *) q_drop->shards_list->data[i];

            /* set start and end properties */
            duration = vshard.shard->duration;

            vshard.start = vshard.shard->id - vshard.shard->id % duration;
            vshard.end = vshard.start + duration;

            if (!cexpr_run(
                    q_drop->where_expr,
                    (cexpr_cb_t) siridb_shard_cexpr_cb,
                    &vshard))
            {
                siridb_shard_decref(vshard.shard);
                dropped++;
            }
            else if (dropped)
            {
                q_drop->shards_list->data[i - dropped] = vshard.shard;
            }
        }

        q_drop->shards_list->len -= dropped;
    }

    size_t n = siridb_shards_n(siridb);
    double percent = n
            ? (double) q_drop->shards_list->len / n
            : 0.0;

    if (IS_MASTER &&
        q_drop->shards_list->len &&
        (~q_drop->flags & QUERIES_IGNORE_DROP_THRESHOLD) &&
        percent >= siridb->drop_threshold)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "This query would drop %0.2f%% of the shards in pool %u. "
                "Add \'set ignore_threshold true\' to the query "
                "statement if you really want to do this.",
                percent * 100,
                siridb->server->pool);

        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uv_async_t * next;

        QP_ADD_SUCCESS

        q_drop->n = q_drop->shards_list->len;

        next = malloc(sizeof(uv_async_t));

        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_drop_shards);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
}

static void exit_drop_tag(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * tag_node =
            cleri_gn(query->nodes->node->children->next);

    char name[tag_node->len - 1];

    xstr_extract_string(name, tag_node->str, tag_node->len);

    if (siridb_tags_drop_tag(siridb->tags, name, query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS
        log_info(MSG_SUCCESS_DROP_TAG, name);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_DROP_TAG, name);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_drop_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * user_node =
            cleri_gn(query->nodes->node->children->next);
    char username[user_node->len - 1];

    xstr_extract_string(username, user_node->str, user_node->len);

    if (siridb_users_drop_user(
            siridb,
            username,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS
        log_info(MSG_SUCCESS_DROP_USER, username);
        qp_add_fmt_safe(query->packer, MSG_SUCCESS_DROP_USER, username);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_grant_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    if (siridb_users_save(siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(1024);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    char * name = ((query_alter_t *) query->data)->via.user->name;
    log_info(MSG_SUCCESS_GRANT_USER, name);
    qp_add_fmt_safe(query->packer, MSG_SUCCESS_GRANT_USER, name);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response,
                0);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_help_xxx(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    if (query->data != NULL)
    {
        assert (query->packer == NULL);
        const char * help = siri_help_get(
                query->nodes->node->cl_obj->gid,
                (const char *) query->data,
                query->err_msg);

        if (help == NULL)
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        query->packer = sirinet_packer_new(4096);

        if (query->packer == NULL)
        {
            MEM_ERR_RET
        }

        qp_add_type(query->packer, QP_MAP_OPEN);
        qp_add_raw(query->packer, (const unsigned char *) "help", 4);
        qp_add_string(query->packer, help);

        free(query->data);
        query->data = NULL;
    }
    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_list_groups(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_list_t * q_list = (query_list_t *) query->data;

    int is_local = (q_list->props == NULL);

    /* if not is_local check for 'remote' columns */
    if (!is_local)
    {
        size_t i;
        is_local = 1;
        for (i = 0; i < q_list->props->len; i++)
        {
            if (siridb_group_is_remote_prop(
                    *((uint32_t *) q_list->props->data[i])))
            {
                is_local = 0;
                break;
            }
        }
    }

    /* if is_local, check if we use 'remote' props in where expression */
    if (is_local && q_list->where_expr != NULL)
    {
        is_local = !cexpr_contains(
                q_list->where_expr,
                siridb_group_is_remote_prop);
    }

    if (is_local)
    {
        finish_list_groups(handle);
    }
    else
    {
        sirinet_pkg_t * pkg = sirinet_pkg_new(0, 0, BPROTO_REQ_GROUPS, NULL);

        if (pkg != NULL)
        {
            siri_async_incref(handle);

            query->nodes->cb = (uv_async_cb) finish_list_groups;

            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    (sirinet_promises_cb) on_groups_response,
                    handle,
                    0);
        }
    }
}

static void exit_list_pools(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_pool_t * pool = siridb->pools->pool + siridb->server->pool;
    siridb_pool_walker_t wpool = {
            .servers=pool->len,
            .series=siridb->series->len,
            .pool=siridb->server->pool
    };
    uint_fast16_t prop;
    cexpr_t * where_expr = q_list->where_expr;

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(3);

        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }

        vec_append(q_list->props, &GID_K_POOL);
        vec_append(q_list->props, &GID_K_SERVERS);
        vec_append(q_list->props, &GID_K_SERIES);
        qp_add_raw(query->packer, (const unsigned char *) "pool", 4);
        qp_add_raw(query->packer, (const unsigned char *) "servers", 7);
        qp_add_raw(query->packer, (const unsigned char *) "series", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "pools", 5);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    if (    q_list->limit &&
            (where_expr == NULL || cexpr_run(
                    where_expr,
                    (cexpr_cb_t) siridb_pool_cexpr_cb,
                    &wpool)))
    {
        qp_add_type(query->packer, QP_ARRAY_OPEN);

        for (prop = 0; prop < q_list->props->len; prop++)
        {
            switch(*((uint32_t *) q_list->props->data[prop]))
            {
            case CLERI_GID_K_POOL:
                qp_add_int64(query->packer, (int64_t) wpool.pool);
                break;
            case CLERI_GID_K_SERVERS:
                qp_add_int64(query->packer, (int64_t) wpool.servers);
                break;
            case CLERI_GID_K_SERIES:
                qp_add_int64(query->packer, (int64_t) wpool.series);
                break;
            }
        }

        qp_add_type(query->packer, QP_ARRAY_CLOSE);
        q_list->limit--;
    }

    if (IS_MASTER && q_list->limit)
    {
        /* we have not reached the limit, send the query to other pools */
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_list_xxx_response,
                0);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_list_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_t * siridb = query->siridb;

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(1);

        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }

        vec_append(q_list->props, &GID_K_NAME);
        qp_add_raw(query->packer, (const unsigned char *) "name", 4);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "series", 6);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    uv_mutex_lock(&siridb->series_mutex);

    q_list->vec = imap_2vec_ref((q_list->series_map == NULL) ?
                    siridb->series_map : q_list->series_map);

    uv_mutex_unlock(&siridb->series_mutex);

    if (q_list->vec == NULL)
    {
        MEM_ERR_RET;
    }

    uv_async_t * next = malloc(sizeof(uv_async_t));

    if (next == NULL)
    {
        MEM_ERR_RET
    }

    next->data = handle->data;

    uv_async_init(
            siri.loop,
            next,
            (uv_async_cb) async_list_series);
    uv_async_send(next);

    uv_close((uv_handle_t *) handle, (uv_close_cb) free);
}

static void exit_list_servers(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    query_list_t * q_list = (query_list_t *) query->data;
    cexpr_t * where_expr = q_list->where_expr;
    int is_local = IS_MASTER;

    /* if is_local, check if we need ask for 'remote' columns */
    if (is_local && q_list->props != NULL)
    {
        size_t i;
        for (i = 0; i < q_list->props->len; i++)
        {
            if (siridb_server_is_remote_prop(
                    *((uint32_t *) q_list->props->data[i])))
            {
                is_local = 0;
                break;
            }
        }
    }

    /* if is_local, check if we use 'remote' props in where expression */
    if (is_local && where_expr != NULL)
    {
        is_local = !cexpr_contains(where_expr, siridb_server_is_remote_prop);
    }

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(5);

        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }

        vec_append(q_list->props, &GID_K_NAME);
        vec_append(q_list->props, &GID_K_POOL);
        vec_append(q_list->props, &GID_K_VERSION);
        vec_append(q_list->props, &GID_K_ONLINE);
        vec_append(q_list->props, &GID_K_STATUS);
        qp_add_raw(query->packer, (const unsigned char *) "name", 4);
        qp_add_raw(query->packer, (const unsigned char *) "pool", 4);
        qp_add_raw(query->packer, (const unsigned char *) "version", 7);
        qp_add_raw(query->packer, (const unsigned char *) "online", 6);
        qp_add_raw(query->packer, (const unsigned char *) "status", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "servers", 7);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    if (is_local)
    {
        llist_walkn(
                siridb->servers,
                &q_list->limit,
                (llist_cb) siridb_servers_list,
                handle);
    }
    else
    {
        q_list->limit -= siridb_servers_list(siridb->server, handle);
    }


    if (IS_MASTER && !is_local && q_list->limit)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_list_xxx_response,
                0);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_list_shards(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    query_list_t * q_list = (query_list_t *) query->data;
    uint_fast16_t prop;
    uint64_t duration;
    cexpr_t * where_expr = q_list->where_expr;
    siridb_shard_view_t vshard = {
            .server=siridb->server
    };
    size_t i;
    vec_t * shards_list;

    uv_mutex_lock(&siridb->shards_mutex);

    shards_list = siridb_shards_vec(siridb);

    uv_mutex_unlock(&siridb->shards_mutex);

    if (shards_list == NULL)
    {
        MEM_ERR_RET
    }

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(5);

        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }

        vec_append(q_list->props, &GID_K_SID);
        vec_append(q_list->props, &GID_K_POOL);
        vec_append(q_list->props, &GID_K_SERVER);
        vec_append(q_list->props, &GID_K_START);
        vec_append(q_list->props, &GID_K_END);
        qp_add_raw(query->packer, (const unsigned char *) "sid", 3);
        qp_add_raw(query->packer, (const unsigned char *) "pool", 4);
        qp_add_raw(query->packer, (const unsigned char *) "server", 6);
        qp_add_raw(query->packer, (const unsigned char *) "start", 5);
        qp_add_raw(query->packer, (const unsigned char *) "end", 3);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "shards", 6);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; i < shards_list->len; i++)
    {
        vshard.shard = (siridb_shard_t *) shards_list->data[i];

        /* set start and end properties */
        duration = vshard.shard->duration;

        vshard.start = vshard.shard->id - vshard.shard->id % duration;
        vshard.end = vshard.start + duration;

        if (q_list->limit && (where_expr == NULL || cexpr_run(
                where_expr,
                (cexpr_cb_t) siridb_shard_cexpr_cb,
                &vshard)))
        {
            qp_add_type(query->packer, QP_ARRAY_OPEN);

            for (prop = 0; prop < q_list->props->len; prop++)
            {
                switch(*((uint32_t *) q_list->props->data[prop]))
                {
                case CLERI_GID_K_SID:
                    qp_add_int64(query->packer, (int64_t) vshard.shard->id);
                    break;
                case CLERI_GID_K_POOL:
                    qp_add_int64(query->packer, (int64_t) vshard.server->pool);
                    break;
                case CLERI_GID_K_SIZE:
                    qp_add_int64(query->packer, (int64_t) vshard.shard->len);
                    break;
                case CLERI_GID_K_START:
                    qp_add_int64(query->packer, (int64_t) vshard.start);
                    break;
                case CLERI_GID_K_END:
                    qp_add_int64(query->packer, (int64_t) vshard.end);
                    break;
                case CLERI_GID_K_TYPE:
                    qp_add_string(
                            query->packer,
                            shard_type_map[vshard.shard->tp]);
                    break;
                case CLERI_GID_K_SERVER:
                    qp_add_string(query->packer, vshard.server->name);
                    break;
                case CLERI_GID_K_STATUS:
                    {
                        char buffer[SIRIDB_SHARD_STATUS_STR_MAX];
                        int n = siridb_shard_status(buffer, vshard.shard);
                        qp_add_raw(
                                query->packer,
                                (const unsigned char *) buffer,
                                n);
                    }
                    break;
                }
            }
            qp_add_type(query->packer, QP_ARRAY_CLOSE);

            q_list->limit--;
        }

        siridb_shard_decref(vshard.shard);
    }

    vec_free(shards_list);

    if (IS_MASTER && q_list->limit)
    {
        /* we have not reached the limit, send the query to other pools */
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_SERVERS,
                (sirinet_promises_cb) on_list_xxx_response,
                0);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_list_tags(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    query_list_t * q_list = (query_list_t *) query->data;

    int is_local = (q_list->props == NULL);

    /* if not is_local check for 'remote' columns */
    if (!is_local)
    {
        size_t i;
        is_local = 1;
        for (i = 0; i < q_list->props->len; i++)
        {
            if (siridb_tag_is_remote_prop(
                    *((uint32_t *) q_list->props->data[i])))
            {
                is_local = 0;
                break;
            }
        }
    }

    /* if is_local, check if we use 'remote' props in where expression */
    if (is_local && q_list->where_expr != NULL)
    {
        is_local = !cexpr_contains(
                q_list->where_expr,
                siridb_tag_is_remote_prop);
    }

    if (is_local)
    {
        finish_list_tags(handle);
    }
    else
    {
        sirinet_pkg_t * pkg = sirinet_pkg_new(0, 0, BPROTO_REQ_TAGS, NULL);

        if (pkg != NULL)
        {
            siri_async_incref(handle);

            query->nodes->cb = (uv_async_cb) finish_list_tags;

            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    (sirinet_promises_cb) on_tags_response,
                    handle,
                    0);
        }
    }
}

static void exit_list_users(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    llist_node_t * node = siridb->users->first;
    vec_t * props = ((query_list_t *) query->data)->props;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_user_cexpr_cb;
    query_list_t * q_list = (query_list_t *) query->data;
    cexpr_t * where_expr = q_list->where_expr;
    size_t i;
    siridb_user_t * user;

    if (props == NULL)
    {
        qp_add_raw(query->packer, (const unsigned char *) "name", 4);
        qp_add_raw(query->packer, (const unsigned char *) "access", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "users", 5);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    while (node != NULL && q_list->limit)
    {
        user = node->data;

        if (where_expr == NULL || cexpr_run(where_expr, cb, user))
        {
            q_list->limit--;

            qp_add_type(query->packer, QP_ARRAY_OPEN);

            if (props == NULL)
            {
                siridb_user_prop(user, query->packer, CLERI_GID_K_NAME);
                siridb_user_prop(user, query->packer, CLERI_GID_K_ACCESS);
            }
            else
            {
                for (i = 0; i < props->len; i++)
                {
                    siridb_user_prop(
                            user,
                            query->packer,
                            *((uint32_t *) props->data[i]));
                }
            }

            qp_add_type(query->packer, QP_ARRAY_CLOSE);

        }
        node = node->next;
    }
    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_revoke_user(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    if (siridb_users_save(siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(1024);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    char * name = ((query_alter_t *) query->data)->via.user->name;
    log_info(MSG_SUCCESS_REVOKE_USER, name);
    qp_add_fmt_safe(query->packer, MSG_SUCCESS_REVOKE_USER, name);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response,
                0);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void exit_series_match(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;

    if (q_wrapper->tp == QUERIES_SELECT)
    {
        query_select_t * q_select = (query_select_t *) q_wrapper;
        if ((q_select->flags & QUERIES_SKIP_GET_POINTS) &&
                (q_select->start_ts != NULL || q_select->end_ts != NULL))
        {
            q_select->flags &= ~QUERIES_SKIP_GET_POINTS;
        }

        if ((~q_select->flags & QUERIES_SKIP_GET_POINTS) &&
                q_select->nselects > 1)
        {
            /* We have more than one select request, let's use points caching.
             * (Not critical, everything works if points_map is NULL) */
            q_select->points_map = imap_new();
        }
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_series_parentheses(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;

    assert (q_wrapper->sset_vec);
    assert (q_wrapper->sset_vec->len);

    siridb_sset_t * sset = vec_pop(q_wrapper->sset_vec);

    if (sset->update_cb != NULL)
    {
        assert (sset->series_map != NULL);
        (*sset->update_cb)(
                sset->series_map,
                q_wrapper->series_map,
                (imap_free_cb) &siridb__series_decref);
        q_wrapper->series_map = sset->series_map;
    }
    else
    {
        assert (sset->series_map == NULL);
    }

    /* simple free to prevent cleaning sset->series_map */
    free (sset);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_select_aggregate(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;

    if (q_select->where_expr != NULL)
    {
        /* we transform the references from imap to vec */
        q_select->vec = imap_vec_pop(q_select->series_map);

        if (q_select->vec == NULL)
        {
            MEM_ERR_RET
        }

        /* now we can simply destroy the imap */
        imap_free(q_select->series_map, NULL);

        /* create a new one */
        q_select->series_map = imap_new();

        if (q_select->series_map == NULL)
        {
            MEM_ERR_RET
        }

        uv_async_t * next = malloc(sizeof(uv_async_t));

        if (next == NULL)
        {
            MEM_ERR_RET
        }

        next->data = handle->data;

        uv_async_init(
                siri.loop,
                next,
                (uv_async_cb) async_filter_series);
        uv_async_send(next);

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);

    }
    else if (siridb_presuf_add(&q_select->presuf, query->nodes->node) == NULL)
    {
        MEM_ERR_RET
    }
    else
    {
        q_select->nselects--;

        if (!siridb_presuf_is_unique(q_select->presuf))
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "When using multiple select methods, add a prefix "
                    "and/or suffix to the selection to make them unique.");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            if (q_select->merge_as != NULL)
            {
                vec_t * plist = vec_new(VEC_DEFAULT_SIZE);

                if (plist == NULL || ct_add(
                        q_select->result,
                        siridb_presuf_name(
                                q_select->presuf,
                                q_select->merge_as,
                                strlen(q_select->merge_as)),
                        plist))
                {
                    sprintf(query->err_msg,
                            "Error while merging points. Make sure the "
                            "destination series name is valid.");
                    vec_free(plist);
                    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                    return;
                }
            }

            if (q_select->series_map->len)
            {
                q_select->alist = siridb_aggregate_list(
                        cleri_gn(query->nodes->node->children)->children,
                        query->err_msg);
                if (q_select->alist == NULL)
                {
                    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                    return;
                }
                q_select->vec = imap_2vec_ref(q_select->series_map);

                if (q_select->vec == NULL)
                {
                    MEM_ERR_RET
                }

                uv_async_t * next = malloc(sizeof(uv_async_t));

                if (next == NULL)
                {
                    MEM_ERR_RET
                }

                next->data = handle->data;

                uv_async_init(
                        siri.loop,
                        next,
                        (uv_async_cb) (
                                (q_select->flags & QUERIES_SKIP_GET_POINTS) ?
                                        async_no_points_aggregate :
                                        async_select_aggregate));
                uv_async_send(next);

                uv_close((uv_handle_t *) handle, (uv_close_cb) free);
            }
            else
            {
                SIRIPARSER_ASYNC_NEXT_NODE
            }
        }
    }
}

static void exit_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;

    if (IS_MASTER)
    {
        if (q_select->pmap == NULL || q_select->pmap->len)
        {
            /* we have not reached the limit, send the query to other pools */
            siridb_query_forward(
                    handle,
                    (q_select->pmap == NULL) ?
                            SIRIDB_QUERY_FWD_POOLS :
                            SIRIDB_QUERY_FWD_SOME_POOLS,
                    (sirinet_promises_cb) on_select_response,
                    0);
        }
        else
        {
            uv_work_t * work = malloc(sizeof(uv_work_t));
            if (work == NULL)
            {
                MEM_ERR_RET
            }

            uv_async_t * next = malloc(sizeof(uv_async_t));
            if (next == NULL)
            {
                free(work);
                MEM_ERR_RET
            }

            uv_close((uv_handle_t *) handle, (uv_close_cb) free);

            handle = next;
            handle->data = query;
            siridb_nodes_next(&query->nodes);

            uv_async_init(
                    siri.loop,
                    handle,
                    (query->nodes == NULL) ?
                            (uv_async_cb) siridb_send_query_result :
                            (uv_async_cb) query->nodes->cb);

            siri_async_incref(handle);
            work->data = handle;
            uv_queue_work(
                        siri.loop,
                        work,
                        &master_select_work,
                        &master_select_work_finish);
        }
    }
    else
    {
        if (qp_add_raw(query->packer, (const unsigned char *) "select", 6) ||
            qp_add_type(query->packer, QP_MAP_OPEN) ||
            ct_items(
                    q_select->result,
                    (q_select->merge_as == NULL) ?
                            (ct_item_cb) &items_select_other
                            :
                            (ct_item_cb) &items_select_other_merge,
                    handle) ||
            qp_add_type(query->packer, QP_MAP_CLOSE))
        {
            MEM_ERR_RET
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_set_address(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_server_t * server = ((query_alter_t *) query->data)->via.server;
    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    siridb_t * siridb = query->siridb;

    if (siridb->server == server || server->client != NULL)
    {
        sprintf(query->err_msg, MSG_ERR_SERVER_ADDRESS);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    char address[node->len - 1];
    xstr_extract_string(address, node->str, node->len);

    int rc;
    rc = siridb_server_update_address(siridb, server, address, server->port);
    switch (rc)
    {
    case -1:
        sprintf(query->err_msg, "Error while updating server address");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        break;

    case 0:
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Server address is already set to '%s'",
                server->address);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        break;

    case 1:
        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                MSG_SUCCESS_SET_ADDR_PORT,
                server->name);
        SIRIPARSER_ASYNC_NEXT_NODE
        break;
    }
}

static void exit_set_backup_mode(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    assert (query->data != NULL);
    assert (IS_MASTER);

    siridb_server_t * server = ((query_alter_t *) query->data)->via.server;

    int backup_mode = cleri_gn(cleri_gn(
            query->nodes->node->children->next->next)->
            children)->cl_obj->gid == CLERI_GID_K_TRUE;

    if (backup_mode ^ ((server->flags & SERVER_FLAG_BACKUP_MODE) != 0))
    {
        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                    MSG_SUCCES_SET_BACKUP_MODE,
                    (backup_mode) ? "enabled" : "disabled",
                    server->name);

        if (server == siridb->server)
        {
            if (backup_mode)
            {
                if (siri_backup_enable(&siri, siridb))
                {
                    MEM_ERR_RET
                }
            }
            else
            {
                if (siri_backup_disable(&siri, siridb))
                {
                    MEM_ERR_RET
                }
            }

            SIRIPARSER_ASYNC_NEXT_NODE
        }
        else
        {
            if (siridb_server_is_online(server))
            {
                sirinet_pkg_t * pkg = sirinet_pkg_new(
                        0,
                        0,
                        (backup_mode) ?
                                BPROTO_ENABLE_BACKUP_MODE :
                                BPROTO_DISABLE_BACKUP_MODE,
                        NULL);

                if (pkg == NULL)
                {
                    MEM_ERR_RET
                }

                /* handle will be bound to a timer so we should increment */
                siri_async_incref(handle);

                if (siridb_server_send_pkg(
                        server,
                        pkg,
                        0,
                        (sirinet_promise_cb) on_ack_response,
                        handle,
                        0))
                {
                    /*
                     * signal is raised and 'on_ack_response' will not be
                     * called
                     */
                    free(pkg);
                    siri_async_decref(&handle);
                    MEM_ERR_RET
                }
            }
            else
            {
                snprintf(query->err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Cannot %s backup mode, '%s' is currently unavailable",
                        (backup_mode) ? "enable" : "disable",
                        server->name);
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            }
        }
    }
    else
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Backup mode is already %s on '%s'.",
                (backup_mode) ? "enabled" : "disabled",
                server->name);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
}

static void exit_set_drop_threshold(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);

    double drop_threshold = xstr_to_double(node->str);

    if (drop_threshold < 0.0 || drop_threshold > 1.0)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Drop threshold should be a value between or "
                 "equal to 0 and 1.0 but got %0.3f",
                 drop_threshold);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        double old = siridb->drop_threshold;
        siridb->drop_threshold = drop_threshold;
        if (siridb_save(siridb))
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error while saving database changes!");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            QP_ADD_SUCCESS

            log_info(
                    MSG_SUCCESS_SET_DROP_THRESHOLD,
                    old,
                    siridb->drop_threshold);

            qp_add_fmt_safe(query->packer,
                    MSG_SUCCESS_SET_DROP_THRESHOLD,
                    old,
                    siridb->drop_threshold);

            if (IS_MASTER)
            {
                siridb_query_forward(
                        handle,
                        SIRIDB_QUERY_FWD_UPDATE,
                        (sirinet_promises_cb) on_update_xxx_response,
                        0);
            }
            else
            {
                SIRIPARSER_ASYNC_NEXT_NODE
            }
        }
    }
}

static void exit_set_expiration_xxx(
        uv_async_t * handle,
        uint64_t * expirep,
        uint8_t tp)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)
    MASTER_CHECK_VERSION(siridb, "2.0.35")

    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    uint64_t expiration = (uint64_t) CLERI_NODE_DATA(node);

    if (IS_MASTER && expiration)
    {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec -= (3600*24);  /* remove one dat to be save */
        uint64_t now_ts = siridb_time_now(siridb, now);

        if (expiration >= now_ts)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Shard expiration time should be a value greater "
                    "than or equal to zero (0) and smaller "
                    "than %"PRIu64" but got %" PRId64,
                    now_ts, (int64_t) CLERI_NODE_DATA(node));
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;
        double percent = siridb_shards_count_percent(
                siridb,
                now_ts - expiration,
                tp);

        if ((~q_wrapper->flags & QUERIES_IGNORE_DROP_THRESHOLD) &&
            percent >= siridb->drop_threshold)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "This query would drop %0.2f%% of the shards in pool %u. "
                    "Add \'set ignore_threshold true\' to the query "
                    "statement if you really want to do this.",
                    percent * 100,
                    siridb->server->pool);
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }
    }

    uint64_t old = *expirep;
    *expirep = expiration;

    siridb_update_shard_expiration(siridb);

    if (siridb_save(siridb))
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Error while saving database changes!");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS

        log_info(
                MSG_SUCCESS_SET_EXPIRATION,
                old,
                *expirep);

        qp_add_fmt_safe(query->packer,
                MSG_SUCCESS_SET_EXPIRATION,
                old,
                *expirep);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}
static void exit_set_expiration_log(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    exit_set_expiration_xxx(
            handle,
            &siridb->expiration_log,
            SIRIDB_SHARD_TP_LOG);
}


static void exit_set_expiration_num(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    exit_set_expiration_xxx(
            handle,
            &siridb->expiration_num,
            SIRIDB_SHARD_TP_NUMBER);
}

static void exit_set_list_limit(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)
    MASTER_CHECK_VERSION(siridb, "2.0.17")

    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);

    uint64_t limit = xstr_to_uint64(node->str, node->len);

    if (limit < 1000 || limit >= 4294967296)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "List limit should be a value greater than or equal to 1000 "
                "and smaller than 4294967296 but got %" PRIu64,
                limit);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uint32_t old = siridb->list_limit;
        siridb->list_limit = (uint32_t) limit;

        if (siridb_save(siridb))
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error while saving database changes!");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            QP_ADD_SUCCESS

            log_info(
                    MSG_SUCCESS_SET_LIST_LIMIT,
                    old,
                    siridb->list_limit);

            qp_add_fmt_safe(query->packer,
                    MSG_SUCCESS_SET_LIST_LIMIT,
                    old,
                    siridb->list_limit);

            if (IS_MASTER)
            {
                siridb_query_forward(
                        handle,
                        SIRIDB_QUERY_FWD_UPDATE,
                        (sirinet_promises_cb) on_update_xxx_response,
                        0);
            }
            else
            {
                SIRIPARSER_ASYNC_NEXT_NODE
            }
        }
    }
}

static void exit_set_log_level(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_alter_t * q_alter = (query_alter_t *) query->data;
    siridb_t * siridb = query->siridb;

    assert (query->data != NULL);

    cleri_node_t * node = cleri_gn(cleri_gn(
            query->nodes->node->children->next->next)->children);

    int log_level;

    switch (node->cl_obj->gid)
    {
    case CLERI_GID_K_DEBUG:
        log_level = LOGGER_DEBUG;
        break;
    case CLERI_GID_K_INFO:
        log_level = LOGGER_INFO;
        break;
    case CLERI_GID_K_WARNING:
        log_level = LOGGER_WARNING;
        break;
    case CLERI_GID_K_ERROR:
        log_level = LOGGER_ERROR;
        break;
    case CLERI_GID_K_CRITICAL:
        log_level = LOGGER_CRITICAL;
        break;
    default:
        assert (0);
        log_level = LOGGER_DEBUG;
        break;
    }

    if (q_alter->alter_tp == QUERY_ALTER_SERVERS)
    {
        /*
         * alter_servers
         */
        cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
        siridb_server_walker_t wserver = {
            .server=siridb->server,
            .siridb=siridb
        };

        if (where_expr == NULL || cexpr_run(
                where_expr,
                (cexpr_cb_t) siridb_server_cexpr_cb,
                &wserver))
        {
            logger_set_level(log_level);
            q_alter->n++;
        }

        if (IS_MASTER)
        {
            /*
             * Hide log level information so we can later create an appropriate
             * message.
             */
            q_alter->n += log_level << 16;
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_SERVERS,
                    (sirinet_promises_cb) on_alter_xxx_response,
                    0);
        }
        else
        {
            qp_add_raw(query->packer, (const unsigned char *) "servers", 7);
            qp_add_int64(query->packer, q_alter->n);
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
    else
    {
        /*
         * alter_server
         *
         * we can set the success message, we just ignore the message in case
         * an error occurs.
         */
        siridb_server_t * server = q_alter->via.server;

        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                    MSG_SUCCES_SET_LOG_LEVEL,
                    logger_level_name(log_level),
                    server->name);

        if (server == siridb->server)
        {
            logger_set_level(log_level);

            SIRIPARSER_ASYNC_NEXT_NODE
        }
        else
        {
            QP_PACK_INT16(buffer, log_level)

            if (siridb_server_is_online(server))
            {
                sirinet_pkg_t * pkg = sirinet_pkg_new(
                        0,
                        3,
                        BPROTO_LOG_LEVEL_UPDATE,
                        buffer);
                if (pkg != NULL)
                {
                    /* handle will be bound to a timer so we should increment */
                    siri_async_incref(handle);
                    if (siridb_server_send_pkg(
                            server,
                            pkg,
                            0,
                            (sirinet_promise_cb) on_ack_response,
                            handle,
                            0))
                    {
                        /*
                         * signal is raised and 'on_ack_response' will not be
                         * called
                         */
                        free(pkg);
                        siri_async_decref(&handle);
                    }
                }
            }
            else
            {
                snprintf(query->err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Cannot set log level, '%s' is currently unavailable",
                        server->name);
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            }
        }
    }
}

static void exit_set_port(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_server_t * server = ((query_alter_t *) query->data)->via.server;
    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    siridb_t * siridb = query->siridb;

    if (siridb->server == server || server->client != NULL)
    {
        sprintf(query->err_msg, MSG_ERR_SERVER_ADDRESS);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }


    uint64_t port = xstr_to_uint64(node->str, node->len);

    if (port > 65535)
    {
        sprintf(query->err_msg,
                "Server port must be a value between 0 and 65535");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        int rc;
        rc = siridb_server_update_address(siridb, server, server->address, port);
        switch (rc)
        {
        case -1:
            sprintf(query->err_msg, "Error while updating server address");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            break;

        case 0:
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Server port is already set to '%u'",
                    server->port);
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            break;

        case 1:
            QP_ADD_SUCCESS
            qp_add_fmt_safe(query->packer,
                    MSG_SUCCESS_SET_ADDR_PORT,
                    server->name);
            SIRIPARSER_ASYNC_NEXT_NODE
            break;
        }
    }
}

static void exit_set_select_points_limit(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)
    MASTER_CHECK_VERSION(siridb, "2.0.17")

    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);

    uint64_t limit = xstr_to_uint64(node->str, node->len);

    if (limit < 1 || limit >= 4294967296)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Select points limit should be a value greater than 0 "
                "and smaller than 4294967296 but got %" PRIu64,
                limit);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uint32_t old = siridb->select_points_limit;
        siridb->select_points_limit = (uint32_t) limit;

        if (siridb_save(siridb))
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error while saving database changes!");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            QP_ADD_SUCCESS

            log_info(
                    MSG_SUCCESS_SET_SELECT_POINTS_LIMIT,
                    old,
                    siridb->select_points_limit);

            qp_add_fmt_safe(query->packer,
                    MSG_SUCCESS_SET_SELECT_POINTS_LIMIT,
                    old,
                    siridb->select_points_limit);

            if (IS_MASTER)
            {
                siridb_query_forward(
                        handle,
                        SIRIDB_QUERY_FWD_UPDATE,
                        (sirinet_promises_cb) on_update_xxx_response,
                        0);
            }
            else
            {
                SIRIPARSER_ASYNC_NEXT_NODE
            }
        }
    }
}

static void exit_set_tee(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    cleri_node_t * node = cleri_gn(cleri_gn(
            query->nodes->node->children->next->next)->children);

    char tee_addr_port[node->len - 1];
    char tee_address[SIRI_CFG_MAX_LEN_ADDRESS];
    uint16_t tee_port = SIRIDB_TEE_DEFAULT_TCP_PORT;


    if (node->cl_obj->gid == CLERI_GID_STRING)
    {
        xstr_extract_string(tee_addr_port, node->str, node->len);

        if (tee_addr_port[0] == 0)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Tee address must not be empty");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        if (sirinet_extract_addr_port(tee_addr_port, tee_address, &tee_port))
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Invalid tee address; expecting ADDRESS[:PORT]");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
            return;
        }

        if (siridb_tee_set_address_port(siridb->tee, tee_address, tee_port))
        {
            log_error("Failed to set tee address");  /* continue on error..*/
        }
    }
    else
    {
        /* disable the tee */
        (void) siridb_tee_set_address_port(siridb->tee, NULL, 0);
    }

    if (siridb_save(siridb))
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Error while saving database changes!");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS

        if (siridb->tee->address)
        {
            log_info(
                    MSG_SUCCES_SET_TEE_ENABLED,
                    siridb->tee->address,
                    siridb->tee->port);
            qp_add_fmt_safe(query->packer,
                    MSG_SUCCES_SET_TEE_ENABLED,
                    siridb->tee->address,
                    siridb->tee->port);
        }
        else
        {
            log_info(MSG_SUCCES_SET_TEE_DISABLED);
            qp_add_string(query->packer, MSG_SUCCES_SET_TEE_DISABLED);
        }

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_set_timezone(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cleri_node_t * node = cleri_gn(query->nodes->node->children->next->next);
    siridb_t * siridb = query->siridb;

    MASTER_CHECK_ACCESSIBLE(siridb)

    char timezone[node->len - 1];
    xstr_extract_string(timezone, node->str, node->len);

    iso8601_tz_t new_tz = iso8601_tz(timezone);

    if (new_tz < 0)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Unknown time zone: '%s'. (see 'help timezones' "
                "for a list of valid time zones)",
                timezone);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);

    }
    else if (siridb->tz == new_tz)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Database '%s' is already set to time-zone '%s'.",
                siridb->dbname,
                timezone);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS

        qp_add_fmt_safe(
                query->packer,
                MSG_SUCCES_SET_TIMEZONE,
                iso8601_tzname(siridb->tz),
                iso8601_tzname(new_tz));

        siridb->tz = new_tz;

        if (siridb_save(siridb))
        {
            log_critical("Could not save database changes (database: '%s')",
                    siridb->dbname);
        }

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response,
                    0);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }
}

static void exit_show_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_user_t * db_user = query->client->origin;
    SIRIPARSER_MASTER_CHECK_ACCESS(db_user, SIRIDB_ACCESS_SHOW)

    cleri_children_t * children = cleri_gn(
            query->nodes->node->children->next)->children;
    siridb_props_cb prop_cb;

    assert (query->packer == NULL);

    query->packer = sirinet_packer_new(4096);

    if (query->packer == NULL)
    {
        MEM_ERR_RET
    }

    qp_add_type(query->packer, QP_MAP_OPEN);
    qp_add_raw(query->packer, (const unsigned char *) "data", 4);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    /* set props.h (who_am_i) to current db_name */
    props_set_who_am_i(db_user->name);

    if (children == NULL || cleri_gn(children) == NULL)
    {
        /* show all properties */
        int i;

        for (i = 0; i < KW_COUNT; i++)
        {
            if ((prop_cb = props_get_cb(i)) == NULL)
            {
                continue;
            }
            prop_cb(siridb, query->packer, 1);
        }
    }
    else
    {
        /* show selected properties chosen by query */
        while (1)
        {
            /* get the callback */
            prop_cb = props_get_cb(cleri_gn(cleri_gn(children)->children)->
                                   cl_obj->gid - KW_OFFSET);
            assert (prop_cb != NULL);  /* all props are implemented */
            prop_cb(siridb, query->packer, 1);

            if (children->next == NULL)
            {
                break;
            }

            /* skip one which is the delimiter */
            children = children->next->next;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_tag_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (IS_MASTER)
    {
        qp_add_fmt_safe(
                query->packer,
                MSG_SUCCESS_TAG,
                ((query_alter_t *) query->data)->n);
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;

    struct timespec end;
    char * name = siridb->server->name;
    clock_gettime(CLOCK_REALTIME, &end);

    qp_add_type(query->timeit, QP_MAP2);
    qp_add_raw(query->timeit, (const unsigned char *) "server", 6);
    qp_add_string(query->timeit, name);
    qp_add_raw(query->timeit, (const unsigned char *) "time", 4);
    qp_add_double(query->timeit,
            (double) (end.tv_sec - query->start.tv_sec) +
            (double) (end.tv_nsec - query->start.tv_nsec) / 1000000000.0f);

    if (query->packer == NULL)
    {
        /* lets give the new packer the exact size so we do not
         * need a realloc */
        query->packer = sirinet_packer_new(
                query->timeit->len +
                1 +
                sizeof(sirinet_pkg_t));

        if (query->packer == NULL)
        {
            MEM_ERR_RET
        }

        qp_add_type(query->packer, QP_MAP_OPEN);
    }

    /* extend packer with timeit information */
    qp_packer_extend(query->packer, query->timeit);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void exit_untag_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (IS_MASTER)
    {
        qp_add_fmt_safe(
                query->packer,
                MSG_SUCCESS_UNTAG,
                ((query_alter_t *) query->data)->n);
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

/******************************************************************************
 * Async loop functions.
 *****************************************************************************/

static void async_count_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_series_t * series;
    query_count_t * q_count = (query_count_t *) query->data;
    uint8_t async_more = 0;

    size_t index_end = q_count->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_count->vec->len)
    {
        index_end = q_count->vec->len;
    }
    else
    {
        async_more = 1;
    }

    for (; q_count->vec_index < index_end; q_count->vec_index++)
    {
        series = (siridb_series_t *) q_count->vec->data[q_count->vec_index];
        q_count->n += cexpr_run(
                    q_count->where_expr,
                    (cexpr_cb_t) siridb_series_cexpr_cb,
                    series);
        siridb_series_decref(series);
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_count_series_length(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    siridb_series_t * series;
    query_count_t * q_count = (query_count_t *) query->data;
    uint8_t async_more = 0;

    size_t index_end = q_count->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_count->vec->len)
    {
        index_end = q_count->vec->len;
    }
    else
    {
        async_more = 1;
    }

    for (; q_count->vec_index < index_end; q_count->vec_index++)
    {
        series = (siridb_series_t *) q_count->vec->data[q_count->vec_index];

        if (cexpr_run(
                q_count->where_expr,
                (cexpr_cb_t) siridb_series_cexpr_cb,
                series))
        {
            q_count->n += series->length;
        }

        siridb_series_decref(series);
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_count_xxx_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_drop_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_drop_t * q_drop = (query_drop_t *) query->data;
    siridb_t * siridb = query->siridb;

    siridb_series_t * series;
    uint8_t async_more = 0;

    size_t index_end = q_drop->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_drop->vec->len)
    {
        index_end = q_drop->vec->len;
    }
    else
    {
        async_more = 1;
    }

    uv_mutex_lock(&siridb->series_mutex);

    for (; q_drop->vec_index < index_end; q_drop->vec_index++)
    {
        series = (siridb_series_t *) q_drop->vec->data[q_drop->vec_index];
        siridb_series_drop(siridb, series);
        siridb_series_decref(series);
    }

    uv_mutex_unlock(&siridb->series_mutex);

    /* flush dropped file change to disk */
    if (q_drop->vec->len)
    {
        siridb_series_flush_dropped(siridb);
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_drop_series_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_drop->n);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_drop_shards(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_drop_t * q_drop = (query_drop_t *) query->data;
    siridb_t * siridb = query->siridb;

    if (q_drop->shards_list->len)
    {
        siridb_shard_t * shard = (siridb_shard_t *) vec_pop(
                q_drop->shards_list);

        siridb_shard_drop(
                shard,
                siridb);
        siridb_shard_decref(shard);
    }

    if (q_drop->shards_list->len)
    {
        uv_async_send(handle);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_drop_shards_response,
                0);
    }
    else
    {
        qp_add_int64(query->packer, q_drop->n);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_filter_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;
    cexpr_t * where_expr = q_wrapper->where_expr;
    uint8_t async_more = 0;
    siridb_series_t * series;
    size_t index_end = q_wrapper->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_wrapper->vec->len)
    {
        index_end = q_wrapper->vec->len;
    }
    else
    {
        async_more = 1;
    }

    for (; q_wrapper->vec_index < index_end; q_wrapper->vec_index++)
    {
        series = (siridb_series_t *)
                q_wrapper->vec->data[q_wrapper->vec_index];

        if (cexpr_run(
                where_expr,
                (cexpr_cb_t) siridb_series_cexpr_cb,
                series))
        {
            if (imap_add(q_wrapper->series_map, series->id, series))
            {
                log_critical("Cannot add filtered series to internal map.");
                siridb_series_decref(series);
            }
        }
        else
        {
            siridb_series_decref(series);
        }
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else
    {
        /* free the s-list object and reset index */
        vec_free(q_wrapper->vec);

        q_wrapper->vec = NULL;
        q_wrapper->vec_index = 0;

        /* cleanup where statement since we do not need it anymore */
        cexpr_free(q_wrapper->where_expr);
        q_wrapper->where_expr = NULL;

        /* we now processed the where statement, continue... */
        switch (q_wrapper->tp)
        {
        case QUERIES_DROP:
            exit_drop_series(handle);
            break;
        case QUERIES_SELECT:
            exit_select_aggregate(handle);
            break;
        default:
            assert (0);
            break;
        }
    }
}

static void async_list_series(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    vec_t * props = q_list->props;
    cexpr_t * where_expr = q_list->where_expr;
    uint8_t async_more = 0;
    siridb_series_t * series;
    size_t i;
    size_t index_end = q_list->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_list->vec->len)
    {
        index_end = q_list->vec->len;
    }
    else
    {
        async_more = 1;
    }

    for (;  q_list->limit && q_list->vec_index < index_end;
            q_list->vec_index++)
    {
        series = (siridb_series_t *) q_list->vec->data[q_list->vec_index];

        if (where_expr == NULL || cexpr_run(
                    where_expr,
                    (cexpr_cb_t) siridb_series_cexpr_cb,
                    series))
        {
            if (!--q_list->limit)
            {
                async_more = 0;
            }

            qp_add_type(query->packer, QP_ARRAY_OPEN);

            for (i = 0; i < props->len; i++)
            {
                switch(*((uint32_t *) props->data[i]))
                {
                case CLERI_GID_K_NAME:
                    qp_add_raw(
                            query->packer,
                            (const unsigned char *) series->name,
                            series->name_len);
                    break;
                case CLERI_GID_K_LENGTH:
                    qp_add_int64(query->packer, (int64_t) series->length);
                    break;
                case CLERI_GID_K_TYPE:
                    qp_add_string(query->packer, series_type_map[series->tp]);
                    break;
                case CLERI_GID_K_POOL:
                    qp_add_int64(query->packer, (int64_t) series->pool);
                    break;
                case CLERI_GID_K_SHARD_DURATION:
                    qp_add_int64(query->packer, (int64_t) (series->idx
                            ? series->idx->shard->duration : 0));
                    break;
                case CLERI_GID_K_START:
                    qp_add_int64(query->packer, (int64_t) series->start);
                    break;
                case CLERI_GID_K_END:
                    qp_add_int64(query->packer, (int64_t) series->end);
                    break;
                }
            }

            qp_add_type(query->packer, QP_ARRAY_CLOSE);
        }

        siridb_series_decref(series);
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else if (IS_MASTER && q_list->limit)
    {
        /* we have not reached the limit, send the query to other pools */
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_list_xxx_response,
                0);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_no_points_aggregate(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;
    siridb_t * siridb = query->siridb;
    uint8_t async_more = 0;
    siridb_series_t * series;
    siridb_points_t * points;
    siridb_points_t * aggr_points;
    int required_shard = 0;

    for (;  q_select->vec_index < q_select->vec->len;
            ++q_select->vec_index)
    {
        const char * name;
        size_t i;

        if (required_shard > MAX_BATCH_REQUIRE_SHARD)
        {
            async_more = 1;
            break;
        }

        series = (siridb_series_t *)
                q_select->vec->data[q_select->vec_index];
        /*
         * We must decrement the ref count immediately since the index is
         * incremented by one. The series will not be freed since at least
         * 'series_map' still has a reference.
         */
        siridb_series_decref(series);

        assert (q_select->alist->len >= 1);

        siridb_aggr_t * aggr = q_select->alist->data[0];

        uv_mutex_lock(&siridb->series_mutex);

        switch (aggr->gid)
        {
        case CLERI_GID_F_COUNT:
            points = siridb_series_get_count(series);
            break;
        case CLERI_GID_F_FIRST:
            points = siridb_series_get_first(series, &required_shard);
            break;
        case CLERI_GID_F_LAST:
            points = siridb_series_get_last(series, &required_shard);
            break;
        default:
            assert (0);
            points = NULL;
        }

        uv_mutex_unlock(&siridb->series_mutex);

        if (points == NULL)
        {
            MEM_ERR_RET
        }

        for (i = 1; points->len && i < q_select->alist->len; i++)
        {
            aggr_points = siridb_aggregate_run(
                    points,
                    (siridb_aggr_t *) q_select->alist->data[i],
                    query->err_msg);

            if (aggr_points != points)
            {
                siridb_points_free(points);
            }

            if (aggr_points == NULL)
            {
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }

            points = aggr_points;
        }

        q_select->n += points->len;

        if (q_select->merge_as == NULL)
        {
            name = siridb_presuf_name(
                    q_select->presuf,
                    series->name,
                    series->name_len);

            if (name == NULL || ct_add(q_select->result, name, points))
            {
                sprintf(query->err_msg, "Error adding points to map.");
                siridb_points_free(points);
                log_critical("Critical error adding points");
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
        else
        {
            vec_t ** plist;

            name = siridb_presuf_name(
                    q_select->presuf,
                    q_select->merge_as,
                    strlen(q_select->merge_as));

            plist = (vec_t **) ct_getaddr(q_select->result, name);

            if (    name == NULL ||
                    plist == NULL ||
                    vec_append_safe(plist, points))
            {
                sprintf(query->err_msg, "Error adding points to map.");
                siridb_points_free(points);
                log_critical("Critical error adding points");
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else
    {
        siridb_aggregate_list_free(q_select->alist);
        q_select->alist = NULL;

        vec_free(q_select->vec);
        q_select->vec = NULL;
        q_select->vec_index = 0;

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_select_aggregate(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;
    siridb_t * siridb = query->siridb;
    uint8_t async_more = 0;
    siridb_series_t * series;
    siridb_points_t * points;
    siridb_points_t * aggr_points;

    if (q_select->n > siridb->select_points_limit)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Query has reached the maximum number of selected points "
                "(%u). Please use another time window, an aggregation "
                "function or select less series to reduce the number of "
                "points.",
                siridb->select_points_limit);

        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    series = (siridb_series_t *)
            q_select->vec->data[q_select->vec_index];

    /*
     * We must decrement the ref count immediately since we now update the
     * index by one. The series will not be freed since at least 'series_map'
     * still has a reference.
     */
    siridb_series_decref(series);

    if ((++q_select->vec_index) < q_select->vec->len)
    {
        async_more = 1;
    }

    /* We try to read the points from the cache in case a cache is created.
     * If there are more select functions left we create a copy of the cache.
     * When this is the last select function we pop from the cache since the
     * points are no longer required.
     */
    points = (q_select->points_map == NULL) ?
            NULL :
            q_select->nselects ?
                siridb_points_copy(imap_get(q_select->points_map, series->id)):
                imap_pop(q_select->points_map, series->id);

    if (points == NULL)
    {
        uv_mutex_lock(&siridb->series_mutex);

        points = (series->flags & SIRIDB_SERIES_IS_DROPPED)
               ? NULL
               : q_select->headtail == 0
               ? siridb_series_get_points(
                        series,
                        q_select->start_ts,
                        q_select->end_ts)
               : q_select->headtail < 0
               ? siridb_series_get_points_tail(
                       series,
                       -q_select->headtail)
               : siridb_series_get_points_head(
                      series,
                      q_select->headtail);

        uv_mutex_unlock(&siridb->series_mutex);

        /* when having a cache and points, add a copy of points to the cache */
        if (q_select->points_map != NULL && points != NULL)
        {
            siridb_points_t * cpoints = siridb_points_copy(points);
            if (cpoints != NULL &&
                imap_add(q_select->points_map, series->id, cpoints))
            {
                siridb_points_free(cpoints);
            }
        }
    }

    if (points != NULL)
    {
        const char * name;
        size_t i;

        for (i = 0; points->len && i < q_select->alist->len; i++)
        {
            aggr_points = siridb_aggregate_run(
                    points,
                    (siridb_aggr_t *) q_select->alist->data[i],
                    query->err_msg);

            if (aggr_points != points)
            {
                siridb_points_free(points);
            }

            if (aggr_points == NULL)
            {
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }

            points = aggr_points;
        }

        q_select->n += points->len;

        if (q_select->merge_as == NULL)
        {
            name = siridb_presuf_name(
                    q_select->presuf,
                    series->name,
                    series->name_len);

            if (name == NULL || ct_add(q_select->result, name, points))
            {
                sprintf(query->err_msg, "Error adding points to map.");
                siridb_points_free(points);
                log_critical("Critical error adding points");
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
        else
        {
            vec_t ** plist;

            name = siridb_presuf_name(
                    q_select->presuf,
                    q_select->merge_as,
                    strlen(q_select->merge_as));

            plist = (vec_t **) ct_getaddr(q_select->result, name);

            if (    name == NULL ||
                    plist == NULL ||
                    vec_append_safe(plist, points))
            {
                sprintf(query->err_msg, "Error adding points to map.");
                siridb_points_free(points);
                log_critical("Critical error adding points");
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else
    {
        siridb_aggregate_list_free(q_select->alist);
        q_select->alist = NULL;

        vec_free(q_select->vec);
        q_select->vec = NULL;
        q_select->vec_index = 0;

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

static void async_series_re(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_wrapper_t * q_wrapper = query->data;
    uint8_t async_more = 0;
    siridb_series_t * series;
    size_t index_end = q_wrapper->vec_index + MAX_ITERATE_COUNT;

    if (index_end >= q_wrapper->vec->len)
    {
        index_end = q_wrapper->vec->len;
    }
    else
    {
        async_more = 1;
    }

    int pcre_exec_ret;

    for (; q_wrapper->vec_index < index_end; q_wrapper->vec_index++)
    {
        series = (siridb_series_t *)
                q_wrapper->vec->data[q_wrapper->vec_index];

        pcre_exec_ret = pcre2_match(
                q_wrapper->regex,
                (PCRE2_SPTR8) series->name,
                series->name_len,
                0,                     /* start looking at this point   */
                0,                     /* OPTIONS                       */
                q_wrapper->match_data,
                0);                    /* length of sub_str_vec         */
        if (    pcre_exec_ret < 0 ||
                imap_add(q_wrapper->series_tmp, series->id, series))
        {
            siridb_series_decref(series);
        }
    }

    if (async_more)
    {
        uv_async_send(handle);
    }
    else
    {
        /* free the s-list object and reset index */
        vec_free(q_wrapper->vec);

        pcre2_code_free(q_wrapper->regex);
        pcre2_match_data_free(q_wrapper->match_data);

        q_wrapper->regex = NULL;
        q_wrapper->match_data = NULL;

        q_wrapper->vec = NULL;
        q_wrapper->vec_index = 0;

        if (q_wrapper->update_cb != NULL)
        {
            (*q_wrapper->update_cb)(
                    q_wrapper->series_map,
                    q_wrapper->series_tmp,
                    (imap_free_cb) &siridb__series_decref);
        }
        q_wrapper->series_tmp = NULL;

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

/******************************************************************************
 * On Response functions
 *****************************************************************************/

/*
 * Call-back function: sirinet_promise_cb
 */
static void on_ack_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status)
{
    uv_async_t * handle = (uv_async_t *) promise->data;

    /* decrement the handle reference counter */
    siri_async_decref(&handle);

    if (handle != NULL)
    {
        siridb_query_t * query = handle->data;

        if (status == PROMISE_SUCCESS)
        {
            switch (pkg->tp)
            {
            case BPROTO_ACK_LOG_LEVEL:
                /* success message is already set */
                break;
            case BPROTO_ACK_ENABLE_BACKUP_MODE:
                /* success message is already set */
                break;
            case BPROTO_ACK_DISABLE_BACKUP_MODE:
                /* success message is already set */
                break;
            case BPROTO_ACK_TEE_PIPE_NAME:
                /* success message is already set */
                break;
            default:
                status = PROMISE_PKG_TYPE_ERROR;
                break;
            }
        }

        if (status)
        {
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error occurred while sending the request to '%s' (%s)",
                    promise->server->name,
                    sirinet_promise_strstatus(status));
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            SIRIPARSER_ASYNC_NEXT_NODE
        }
    }

    /* we must free the promise */
    sirinet_promise_decref(promise);
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_alter_xxx_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    qp_obj_t qp_count;
    query_alter_t * q_alter = (query_alter_t *) query->data;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) &&  /* servers etc.*/
                    qp_is_int(qp_next(&unpacker, &qp_count))) /* one result*/
            {
                q_alter->n += qp_count.via.int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }
    /*
     * Note: since this function has the sole purpose for alter servers
     *       and setting log levels or pipe name, we now simply add the
     *       message here.
     */
    QP_ADD_SUCCESS

    log_info(MSG_SUCCES_SET_LOG_LEVEL_MULTI,
            logger_level_name(q_alter->n >> 16),
            q_alter->n & 0xffff);

    qp_add_fmt_safe(
            query->packer,
            MSG_SUCCES_SET_LOG_LEVEL_MULTI,
            logger_level_name(q_alter->n >> 16),
            q_alter->n & 0xffff);

    SIRIPARSER_ASYNC_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_count_xxx_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    uint8_t error_tp = 0;
    siridb_query_t * query = handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    qp_obj_t qp_count;
    query_count_t * q_count = (query_count_t *) query->data;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) &&  /* servers etc.*/
                    qp_is_int(qp_next(&unpacker, &qp_count))) /* one result*/
            {
                q_count->n += qp_count.via.int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }
        else if (pkg != NULL &&
                !error_tp &&
                sirinet_protocol_is_error_msg(pkg->tp) &&
                siridb_query_err_from_pkg(query, pkg) == 0)
        {
            error_tp = pkg->tp;
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    if (error_tp)
    {
        siridb_query_send_error(handle, error_tp);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);

        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_drop_series_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    qp_obj_t qp_drop;
    query_drop_t * q_drop = (query_drop_t *) query->data;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) &&  /* servers etc.*/
                    qp_is_int(qp_next(&unpacker, &qp_drop))) /* one result */
            {
                q_drop->n += qp_drop.via.int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    qp_add_fmt(query->packer, MSG_SUCCES_DROP_SERIES, q_drop->n);

    SIRIPARSER_ASYNC_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_drop_shards_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    qp_obj_t qp_drop;
    query_drop_t * q_drop = (query_drop_t *) query->data;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) &&  /* shards   */
                    qp_is_int(qp_next(&unpacker, &qp_drop))) /* one result */
            {
                q_drop->n += qp_drop.via.int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    qp_add_fmt(query->packer, MSG_SUCCES_DROP_SHARDS, q_drop->n);

    SIRIPARSER_ASYNC_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_groups_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_group_t * group;
    qp_obj_t qp_name;
    qp_obj_t qp_series;
    size_t i;

    siridb_groups_init_nseries(siridb->groups);

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_GROUPS)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_array(qp_next(&unpacker, NULL)))
            {
                while ( qp_is_array(qp_next(&unpacker, NULL)) &&
                        qp_is_raw(qp_next(&unpacker, &qp_name)) &&
                        qp_is_raw_term(&qp_name) &&
                        qp_is_int(qp_next(&unpacker, &qp_series)))
                {
                    group = (siridb_group_t *) ct_get(
                            siridb->groups->groups,
                            (const char *) qp_name.via.raw);
                    if (group != NULL)
                    {
                        group->n += qp_series.via.int64;
                    }
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    query->nodes->cb(handle);
}

static void on_tags_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    siridb_tag_t * tag;
    qp_obj_t qp_name;
    qp_obj_t qp_series;
    size_t i;

    siridb_tags_init_nseries(siridb->tags);

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_TAGS)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_array(qp_next(&unpacker, NULL)))
            {
                while ( qp_is_array(qp_next(&unpacker, NULL)) &&
                        qp_is_raw(qp_next(&unpacker, &qp_name)) &&
                        qp_is_raw_term(&qp_name) &&
                        qp_is_int(qp_next(&unpacker, &qp_series)))
                {
                    tag = (siridb_tag_t *) ct_get(
                            siridb->tags->tags,
                            (const char *) qp_name.via.raw);
                    if (tag != NULL)
                    {
                        tag->n += qp_series.via.int64;
                    }
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    query->nodes->cb(handle);
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_list_xxx_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    int error_tp = 0;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    siridb_query_t * query = handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) && /* columns     */
                    qp_is_array(qp_skip_next(&unpacker)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) && /* series/...  */
                    qp_is_array(qp_next(&unpacker, NULL)))  /* results    */
            {
                while (qp_is_array(qp_current(&unpacker)))
                {
                    if (q_list->limit)
                    {
                        qp_packer_extend_fu(query->packer, &unpacker);
                        q_list->limit--;
                    }
                    else
                    {
                        qp_skip_next(&unpacker);
                    }
                }

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }
        else if (pkg != NULL &&
                !error_tp &&
                sirinet_protocol_is_error_msg(pkg->tp) &&
                siridb_query_err_from_pkg(query, pkg) == 0)
        {
            error_tp = pkg->tp;
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    if (error_tp)
    {
        siridb_query_send_error(handle, error_tp);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_select_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    siridb_query_t * query = handle->data;
    siridb_t * siridb = query->siridb;
    size_t err_count = 0;
    query_select_t * q_select = query->data;
    qp_obj_t qp_name;
    qp_obj_t qp_tp;
    qp_obj_t qp_len;
    qp_obj_t qp_points;
    qp_obj_t qp_err_msg;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            err_count++;
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error occurred while sending the select query to at "
                    "least one required server");
        }
        else
        {
            pkg = (sirinet_pkg_t *) promise->data;

            if (pkg == NULL || pkg->tp != BPROTO_RES_QUERY)
            {
                err_count++;

                if (pkg != NULL && pkg->tp == BPROTO_ERR_QUERY)
                {
                    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

                    if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                            qp_is_raw(qp_next(&unpacker, NULL)) &&
                            qp_is_raw(qp_next(&unpacker, &qp_err_msg)))
                    {
                        snprintf(query->err_msg,
                                SIRIDB_MAX_SIZE_ERR_MSG,
                                "%.*s",
                                (int) qp_err_msg.len,
                                qp_err_msg.via.raw);
                    }
                    else
                    {
                        snprintf(query->err_msg,
                                SIRIDB_MAX_SIZE_ERR_MSG,
                                "Error occurred while sending request to "
                                "at least '%s'",
                                promise->server->name);
                    }
                }
                else
                {
                    snprintf(query->err_msg,
                            SIRIDB_MAX_SIZE_ERR_MSG,
                            "Error occurred while sending request to "
                            "at least '%s'",
                            promise->server->name);
                }
            }
            else
            {
                qp_unpacker_init(&unpacker, pkg->data, pkg->len);

                if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                        qp_is_raw(qp_next(&unpacker, NULL)) && /* select */
                        qp_is_map(qp_next(&unpacker, NULL)))
                {
                    if (q_select->merge_as == NULL)
                    {
                        on_select_unpack_points(
                                &unpacker,
                                q_select,
                                &qp_name,
                                &qp_tp,
                                &qp_len,
                                &qp_points,
                                siridb->select_points_limit);
                    }
                    else
                    {
                        on_select_unpack_merged_points(
                                &unpacker,
                                q_select,
                                &qp_name,
                                &qp_tp,
                                &qp_len,
                                &qp_points,
                                siridb->select_points_limit);
                    }


                    /* extract time-it info if needed */
                    if (query->timeit != NULL)
                    {
                        siridb_query_timeit_from_unpacker(query, &unpacker);
                    }
                }
            }

            /* make sure we free the promise and data */
            free(promise->data);
            sirinet_promise_decref(promise);
        }
    }

    if (q_select->n > siridb->select_points_limit)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Query has reached the maximum number of selected points "
                "(%u). Please use another time window, an aggregation "
                "function or select less series to reduce the number of "
                "points.",
                siridb->select_points_limit);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else if (err_count)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uv_async_t * next;
        uv_work_t * work = malloc(sizeof(uv_work_t));
        if (work == NULL)
        {
            MEM_ERR_RET
        }

        next = malloc(sizeof(uv_async_t));
        if (next == NULL)
        {
            free(work);
            MEM_ERR_RET
        }

        uv_close((uv_handle_t *) handle, (uv_close_cb) free);

        handle = next;
        handle->data = query;
        siridb_nodes_next(&query->nodes);

        uv_async_init(
                siri.loop,
                handle,
                (query->nodes == NULL) ?
                        (uv_async_cb) siridb_send_query_result :
                        (uv_async_cb) query->nodes->cb);

        siri_async_incref(handle);
        work->data = handle;
        uv_queue_work(
                    siri.loop,
                    work,
                    &master_select_work,
                    &master_select_work_finish);

    }
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_update_xxx_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    siridb_query_t * query = handle->data;
    size_t err_count = 0;
    size_t i;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            err_count++;
            snprintf(query->err_msg,
                    SIRIDB_MAX_SIZE_ERR_MSG,
                    "Error occurred while sending the database change to at "
                    "least one required server");
        }
        else
        {
            pkg = (sirinet_pkg_t *) promise->data;

            if (pkg == NULL || pkg->tp != BPROTO_RES_QUERY)
            {
                err_count++;
                snprintf(query->err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Error occurred while sending the database change to "
                        "at least '%s'", promise->server->name);
            }

            /* make sure we free the promise and data */
            free(promise->data);
            sirinet_promise_decref(promise);
        }
    }

    if (err_count)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        SIRIPARSER_ASYNC_NEXT_NODE
    }
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle.
 *
 * Note: used both for tag and untag response.
 */
static void on_tag_response(vec_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = (siridb_query_t *) handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t unpacker;
    qp_obj_t qp_tag;
    size_t i;
    query_alter_t * q_tag = (query_alter_t *) query->data;

    for (i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = (sirinet_pkg_t *) promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            qp_unpacker_init(&unpacker, pkg->data, pkg->len);

            if (    qp_is_map(qp_next(&unpacker, NULL)) &&
                    qp_is_raw(qp_next(&unpacker, NULL)) &&  // success_msg
                    qp_is_int(qp_next(&unpacker, &qp_tag))) // one result
            {
                q_tag->n += qp_tag.via.int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, &unpacker);
                }
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        sirinet_promise_decref(promise);
    }

    SIRIPARSER_ASYNC_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */



/******************************************************************************
 * Helper functions
 *****************************************************************************/


static void master_select_work(uv_work_t * work)
{
    uv_async_t * handle = (uv_async_t *) work->data;
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;
    siridb_t * siridb = query->siridb;
    siridb->selected_points += q_select->n;
    int rc = ct_items(
            q_select->result,
            (q_select->merge_as == NULL) ?
                    (ct_item_cb) &items_select_master
                    :
                    (ct_item_cb) &items_select_master_merge,
            handle);

    /* Do not set an error message when rc==1 since in that case the message
     * is already set.
     */
    switch (rc)
    {
    case -1:
        sprintf(query->err_msg, "Memory allocation error.");
        /* FALLTHRU */
        /* fall through */
    case 1:
        query->flags |= SIRIDB_QUERY_FLAG_ERR;
    }
}

static void master_select_work_finish(uv_work_t * work, int status)
{
    if (status)
    {
        log_error("Select work failed (error: %s)", uv_strerror(status));
    }
    else if (!siri_err)
    {
        /*
         * We need to check for SiriDB errors because this task is running in
         * another thread. In case a siri_err is set, this means we are in forced
         * closing state and we should not use the handle but let siri close it.
         */

        uv_async_t * handle = (uv_async_t *) work->data;
        siridb_query_t * query = handle->data;

        if (query->flags & SIRIDB_QUERY_FLAG_ERR)
        {
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            uv_async_send(handle);
        }
    }

    siri_async_decref((uv_async_t **) &work->data);

    free(work);
}

static int items_select_master(
        const char * name,
        size_t len,
        siridb_points_t * points,
        uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    if (query->factor)
    {
        siridb_points_ts_correction(points, (double) query->factor);
    }

    if (    qp_add_raw(query->packer, (const unsigned char *) name, len) ||
            siridb_points_pack(points, query->packer))
    {
        sprintf(query->err_msg, "Memory allocation error.");
        return -1;
    }

    return 0;
}

static int items_select_master_merge(
        const char * name,
        size_t len,
        vec_t * plist,
        uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_select_t * q_select = query->data;
    siridb_points_t * points;

    if (qp_add_raw(query->packer, (const unsigned char *) name, len))
    {
        sprintf(query->err_msg, "Memory allocation error.");
        return -1;
    }

    switch (plist->len)
    {
    case 0:
        points = siridb_points_new(0, TP_INT);
        if (points == NULL)
        {
            sprintf(query->err_msg, "Memory allocation error.");
        }
        break;
    case 1:
        points = vec_pop(plist);
        break;
    default:
        points = siridb_points_merge(plist, query->err_msg);
        break;
    }

    if (q_select->mlist != NULL && points != NULL)
    {
        siridb_points_t * aggr_points;
        size_t i;

        for (i = 0; points->len && i < q_select->mlist->len; i++)
        {
            aggr_points = siridb_aggregate_run(
                    points,
                    (siridb_aggr_t *) q_select->mlist->data[i],
                    query->err_msg);

            if (aggr_points != points)
            {
                siridb_points_free(points);
            }

            if (aggr_points == NULL)
            {
                return -1;  /* (error message is set)  */
            }

            points = aggr_points;
        }
    }

    if (points == NULL)
    {
        /*
         * The list will be cleared including the points since 'merge_as'
         * is still not NULL. (error message is set)
         */
        return -1;
    }

    if (query->factor)
    {
        siridb_points_ts_correction(points, (double) query->factor);
    }

    if (siridb_points_pack(points, query->packer))
    {
        sprintf(query->err_msg, "Memory allocation error.");
        siridb_points_free(points);
        return -1;
    }

    siridb_points_free(points);

    return 0;
}

/*
 * Returns 0 when successful and -1 in case of an error.
 * (a SIGNAL is raised in case of an error)
 */
static int items_select_other(
        const char * name,
        size_t len,
        siridb_points_t * points,
        uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    return -(qp_add_raw_term(
                query->packer, (const unsigned char *) name, len) ||
            siridb_points_raw_pack(points, query->packer));
}

/*
 * Returns 0 when successful and -1 in case of an error.
 * (a SIGNAL is raised in case of an error)
 */
static int items_select_other_merge(
        const char * name,
        size_t len,
        vec_t * plist,
        uv_async_t * handle)
{
    size_t i;
    siridb_query_t * query = handle->data;
    int rc = qp_add_raw_term(
                query->packer, (const unsigned char *) name, len) ||
            qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (i = 0; !rc && i < plist->len; i++)
    {
        rc = siridb_points_raw_pack(
                (siridb_points_t * ) plist->data[i],
                query->packer);
    }

    return -(rc || qp_add_type(query->packer, QP_ARRAY_CLOSE));
}

static void on_select_unpack_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points,
        uint32_t select_points_limit)
{
    siridb_points_t * points;

    while ( q_select->n <= select_points_limit &&
            qp_is_raw(qp_next(unpacker, qp_name)) &&
            qp_is_raw_term(qp_name) &&
            qp_is_array(qp_next(unpacker, NULL)) &&
            qp_is_int(qp_next(unpacker, qp_tp)) &&
            qp_is_int(qp_next(unpacker, qp_len)) &&
            qp_is_raw(qp_next(unpacker, qp_points)))
    {
        points = siridb_points_new(qp_len->via.int64, qp_tp->via.int64);
        if (points != NULL)
        {
            if (points->tp == TP_STRING)
            {
                if (qp_len->via.int64 < POINTS_ZIP_THRESHOLD)
                {
                    siridb_points_unzip_string_raw(
                            points,
                            qp_points->via.raw,
                            qp_len->via.int64);
                }
                else
                {
                    siridb_points_unzip_string(
                            points,
                            qp_points->via.raw,
                            qp_len->via.int64,
                            NULL, NULL, 0);
                }
            }
            else
            {
                points->len = qp_len->via.int64;
                memcpy(points->data, qp_points->via.raw, qp_points->len);
            }

            if (ct_add(q_select->result, (char *) qp_name->via.raw, points))
            {
                siridb_points_free(points);
            }
            else
            {
                q_select->n += points->len;
            }
        }

        qp_next(unpacker, NULL);  /* QP_ARRAY_CLOSE     */
    }
}

static void on_select_unpack_merged_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points,
        uint32_t select_points_limit)
{
    siridb_points_t * points;

    while ( qp_is_raw(qp_next(unpacker, qp_name)) &&
            qp_is_raw_term(qp_name) &&
            qp_is_array(qp_next(unpacker, NULL)))
    {
        vec_t ** plist = (vec_t **) ct_getaddr(
                q_select->result,
                (const char *) qp_name->via.raw);

        while ( q_select->n <= select_points_limit &&
                qp_is_array(qp_next(unpacker, NULL)) &&
                qp_is_int(qp_next(unpacker, qp_tp)) &&
                qp_is_int(qp_next(unpacker, qp_len)) &&
                qp_is_raw(qp_next(unpacker, qp_points)))
        {

            points = siridb_points_new(qp_len->via.int64, qp_tp->via.int64);

            if (points != NULL)
            {
                if (points->tp == TP_STRING)
                {
                    if (qp_len->via.int64 < POINTS_ZIP_THRESHOLD)
                    {
                        siridb_points_unzip_string_raw(
                                points,
                                qp_points->via.raw,
                                qp_len->via.int64);
                    }
                    else
                    {
                        siridb_points_unzip_string(
                                points,
                                qp_points->via.raw,
                                qp_len->via.int64,
                                NULL, NULL, 0);
                    }
                }
                else
                {
                    points->len = qp_len->via.int64;
                    memcpy(points->data, qp_points->via.raw, qp_points->len);
                }

                if (vec_append_safe(plist, points))
                {
                    siridb_points_free(points);
                }
                else
                {
                    q_select->n += points->len;
                }
            }

            qp_next(unpacker, NULL);  /* QP_ARRAY_CLOSE     */
        }
    }
}

static int values_list_groups(siridb_group_t * group, uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_group_cexpr_cb;
    vec_t * props = ((query_list_t *) query->data)->props;

    if (where_expr == NULL || cexpr_run(where_expr, cb, group))
    {
        size_t i;
        qp_add_type(query->packer, QP_ARRAY_OPEN);

        for (i = 0; i < props->len; i++)
        {
            siridb_group_prop(
                    group,
                    query->packer,
                    *((uint32_t *) props->data[i]));
        }

        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        return 1;
    }

    return 0;
}

static int values_count_groups(siridb_group_t * group, uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    return cexpr_run(
            ((query_list_t *) query->data)->where_expr,
            (cexpr_cb_t) siridb_group_cexpr_cb,
            group);
}

static void finish_list_groups(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_t * siridb = query->siridb;

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(1);
        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }
        vec_append(q_list->props, &GID_K_NAME);
        qp_add_raw(query->packer, (const unsigned char *) "name", 4);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "groups", 6);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    ct_valuesn(
            siridb->groups->groups,
            &q_list->limit,
            (ct_val_cb) values_list_groups,
            handle);

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void finish_count_groups(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_count_t * q_count = (query_count_t *) query->data;
    siridb_t * siridb = query->siridb;

    /* Note: ct_values(..values_count_groups..) can only result in a positive
     *       value.
     */
    size_t n = (q_count->where_expr == NULL) ?
            siridb->groups->groups->len :
            (size_t) ct_values(
                        siridb->groups->groups,
                        (ct_val_cb) values_count_groups,
                        handle);

    qp_add_raw(query->packer, (const unsigned char *) "groups", 6);

    qp_add_int64(query->packer, n);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static int values_list_tags(siridb_tag_t * tag, uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_tag_cexpr_cb;
    vec_t * props = ((query_list_t *) query->data)->props;

    if (where_expr == NULL || cexpr_run(where_expr, cb, tag))
    {
        size_t i;
        qp_add_type(query->packer, QP_ARRAY_OPEN);

        for (i = 0; i < props->len; i++)
        {
            siridb_tag_prop(
                    tag,
                    query->packer,
                    *((uint32_t *) props->data[i]));
        }

        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        return 1;
    }

    return 0;
}

static int values_count_tags(siridb_tag_t * tag, uv_async_t * handle)
{
    siridb_query_t * query = handle->data;

    return cexpr_run(
            ((query_list_t *) query->data)->where_expr,
            (cexpr_cb_t) siridb_tag_cexpr_cb,
            tag);
}

static void finish_list_tags(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_t * siridb = query->siridb;

    if (q_list->props == NULL)
    {
        q_list->props = vec_new(1);
        if (q_list->props == NULL)
        {
            MEM_ERR_RET
        }
        vec_append(q_list->props, &GID_K_NAME);
        qp_add_raw(query->packer, (const unsigned char *) "name", 4);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, (const unsigned char *) "tags", 4);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    ct_valuesn(
            siridb->tags->tags,
            &q_list->limit,
            (ct_val_cb) values_list_tags,
            handle);

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_ASYNC_NEXT_NODE
}

static void finish_count_tags(uv_async_t * handle)
{
    siridb_query_t * query = handle->data;
    query_count_t * q_count = (query_count_t *) query->data;
    siridb_t * siridb = query->siridb;

    /* Note: ct_values(..values_count_tags..) can only result in a positive
     *       value.
     */
    size_t n = (q_count->where_expr == NULL) ?
            siridb->tags->tags->len :
            (size_t) ct_values(
                        siridb->tags->tags,
                        (ct_val_cb) values_count_tags,
                        handle);

    qp_add_raw(query->packer, (const unsigned char *) "tags", 4);

    qp_add_int64(query->packer, n);

    SIRIPARSER_ASYNC_NEXT_NODE
}
