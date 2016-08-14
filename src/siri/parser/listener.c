/*
 * listener.c - contains functions for processing queries.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#include <assert.h>
#include <cexpr/cexpr.h>
#include <inttypes.h>
#include <logger/logger.h>
#include <math.h>
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/db/nodes.h>
#include <siri/db/props.h>
#include <siri/db/query.h>
#include <siri/db/series.h>
#include <siri/db/servers.h>
#include <siri/db/shard.h>
#include <siri/db/user.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <siri/net/promises.h>
#include <siri/net/protocol.h>
#include <siri/net/socket.h>
#include <siri/parser/listener.h>
#include <siri/parser/queries.h>
#include <siri/parser/walkers.h>
#include <siri/siri.h>
#include <strextra/strextra.h>
#include <sys/time.h>
#include <siri/db/re.h>
#include <siri/db/presuf.h>
#include <siri/db/aggregate.h>

#define MAX_ITERATE_COUNT 1000      // thousand
#define MAX_SELECT_POINTS 100000   // one hundred thousand
#define MAX_LIST_LIMIT 10000        // ten thousand

#define QP_ADD_SUCCESS qp_add_raw(query->packer, "success_msg", 11);
#define DEFAULT_ALLOC_COLUMNS 6
#define IS_MASTER (query->flags & SIRIDB_QUERY_FLAG_MASTER)
#define MASTER_CHECK_POOLS_ONLINE(siridb)                                   \
if (IS_MASTER && !siridb_pools_online(siridb))                              \
{                                                                           \
    snprintf(query->err_msg,                                                \
            SIRIDB_MAX_SIZE_ERR_MSG,                                        \
            "At least one pool is lacking an available server to process "  \
            "this request");                                                \
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
static void decref_server_object(uv_handle_t * handle);
static void decref_user_object(uv_handle_t * handle);

static void enter_access_expr(uv_async_t * handle);
static void enter_alter_server(uv_async_t * handle);
static void enter_alter_user(uv_async_t * handle);
static void enter_count_stmt(uv_async_t * handle);
static void enter_create_user(uv_async_t * handle);
static void enter_drop_stmt(uv_async_t * handle);
static void enter_grant_stmt(uv_async_t * handle);
static void enter_grant_user(uv_async_t * handle);
static void enter_limit_expr(uv_async_t * handle);
static void enter_list_stmt(uv_async_t * handle);
static void enter_merge_as(uv_async_t * handle);
static void enter_revoke_stmt(uv_async_t * handle);
static void enter_revoke_user(uv_async_t * handle);
static void enter_select_stmt(uv_async_t * handle);
static void enter_set_ignore_threshold(uv_async_t * handle);
static void enter_set_password(uv_async_t * handle);
static void enter_series_name(uv_async_t * handle);
static void enter_series_match(uv_async_t * handle);
static void enter_series_re(uv_async_t * handle);
static void enter_series_sep(uv_async_t * handle);
static void enter_timeit_stmt(uv_async_t * handle);
static void enter_where_xxx(uv_async_t * handle);
static void enter_xxx_columns(uv_async_t * handle);

static void exit_after_expr(uv_async_t * handle);
static void exit_alter_user(uv_async_t * handle);
static void exit_before_expr(uv_async_t * handle);
static void exit_between_expr(uv_async_t * handle);
static void exit_calc_stmt(uv_async_t * handle);
static void exit_count_pools(uv_async_t * handle);
static void exit_count_series(uv_async_t * handle);
static void exit_count_servers(uv_async_t * handle);
static void exit_count_users(uv_async_t * handle);
static void exit_create_user(uv_async_t * handle);
static void exit_drop_series(uv_async_t * handle);
static void exit_drop_shard(uv_async_t * handle);
static void exit_drop_user(uv_async_t * handle);
static void exit_grant_user(uv_async_t * handle);
static void exit_list_pools(uv_async_t * handle);
static void exit_list_series(uv_async_t * handle);
static void exit_list_servers(uv_async_t * handle);
static void exit_list_users(uv_async_t * handle);
static void exit_revoke_user(uv_async_t * handle);
static void exit_select_aggregate(uv_async_t * handle);
static void exit_select_stmt(uv_async_t * handle);
static void exit_set_drop_threshold(uv_async_t * handle);
static void exit_set_log_level(uv_async_t * handle);
static void exit_show_stmt(uv_async_t * handle);
static void exit_timeit_stmt(uv_async_t * handle);

/* async loop functions */
static void async_count_series(uv_async_t * handle);
static void async_drop_series(uv_async_t * handle);
static void async_filter_series(uv_async_t * handle);
static void async_list_series(uv_async_t * handle);
static void async_select_aggregate(uv_async_t * handle);
static void async_series_re(uv_async_t * handle);

/* on response functions */
static void on_ack_response(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);
static void on_count_xxx_response(slist_t * promises, uv_async_t * handle);
static void on_drop_xxx_response(slist_t * promises, uv_async_t * handle);
static void on_list_xxx_response(slist_t * promises, uv_async_t * handle);
static void on_select_response(slist_t * promises, uv_async_t * handle);
static void on_update_xxx_response(slist_t * promises, uv_async_t * handle);

/* helper functions */
static void master_select_work(uv_work_t * handle);
static void master_select_work_finish(uv_work_t * work, int status);
static int items_select_master(
        const char * name,
        siridb_points_t * points,
        uv_async_t * handle);
static int items_select_master_merge(
        const char * name,
        slist_t * plist,
        uv_async_t * handle);
int items_select_other(
        const char * name,
        siridb_points_t * points,
        uv_async_t * handle);
int items_select_other_merge(
        const char * name,
        slist_t * plist,
        uv_async_t * handle);
static void on_select_unpack_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points);
static void on_select_unpack_merged_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points);

/* address bindings for default list properties */
static uint32_t GID_K_NAME = CLERI_GID_K_NAME;
static uint32_t GID_K_POOL = CLERI_GID_K_POOL;
static uint32_t GID_K_VERSION = CLERI_GID_K_VERSION;
static uint32_t GID_K_ONLINE = CLERI_GID_K_ONLINE;
static uint32_t GID_K_STATUS = CLERI_GID_K_STATUS;
static uint32_t GID_K_SERVERS = CLERI_GID_K_SERVERS;
static uint32_t GID_K_SERIES = CLERI_GID_K_SERIES;

/*
 * Start SIRIPARSER_NEXT_NODE
 */
#define SIRIPARSER_NEXT_NODE                                                \
siridb_nodes_next(&query->nodes);                                           \
if (query->nodes == NULL)                                                   \
{                                                                           \
    siridb_send_query_result(handle);                                       \
}                                                                           \
else                                                                        \
{                                                                           \
    uv_async_t * forward = (uv_async_t *) malloc(sizeof(uv_async_t));       \
    if (forward == NULL)                                                    \
    {                                                                       \
        ERR_ALLOC                                                           \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        forward->data = (void *) handle->data;                              \
        uv_async_init(siri.loop, forward, (uv_async_cb) query->nodes->cb);  \
        uv_async_send(forward);                                             \
    }                                                                       \
    uv_close((uv_handle_t *) handle, (uv_close_cb) free);                   \
}

/*
 * Start SIRIPARSER_MASTER_CHECK_ACCESS
 */
#define SIRIPARSER_MASTER_CHECK_ACCESS(ACCESS_BIT)                          \
if (    IS_MASTER &&                                                        \
        !siridb_user_check_access(                                          \
            ((sirinet_socket_t *) query->client->data)->origin,             \
            ACCESS_BIT,                                                     \
            query->err_msg))                                                \
{                                                                           \
    siridb_query_send_error(handle, CPROTO_ERR_USER_ACCESS);                \
    return;                                                                 \
}

void siriparser_init_listener(void)
{
    for (uint_fast16_t i = 0; i < CLERI_END; i++)
    {
        siriparser_listen_enter[i] = NULL;
        siriparser_listen_exit[i] = NULL;
    }

    siriparser_listen_enter[CLERI_GID_ACCESS_EXPR] = enter_access_expr;
    siriparser_listen_enter[CLERI_GID_ALTER_SERVER] = enter_alter_server;
    siriparser_listen_enter[CLERI_GID_ALTER_USER] = enter_alter_user;
    siriparser_listen_enter[CLERI_GID_COUNT_STMT] = enter_count_stmt;
    siriparser_listen_enter[CLERI_GID_CREATE_USER] = enter_create_user;
    siriparser_listen_enter[CLERI_GID_DROP_STMT] = enter_drop_stmt;
    siriparser_listen_enter[CLERI_GID_GRANT_STMT] = enter_grant_stmt;
    siriparser_listen_enter[CLERI_GID_GRANT_USER] = enter_grant_user;
    siriparser_listen_enter[CLERI_GID_LIMIT_EXPR] = enter_limit_expr;
    siriparser_listen_enter[CLERI_GID_LIST_STMT] = enter_list_stmt;
    siriparser_listen_enter[CLERI_GID_MERGE_AS] = enter_merge_as;
    siriparser_listen_enter[CLERI_GID_POOL_COLUMNS] = enter_xxx_columns;
    siriparser_listen_enter[CLERI_GID_REVOKE_STMT] = enter_revoke_stmt;
    siriparser_listen_enter[CLERI_GID_REVOKE_USER] = enter_revoke_user;
    siriparser_listen_enter[CLERI_GID_SELECT_STMT] = enter_select_stmt;
    siriparser_listen_enter[CLERI_GID_SET_IGNORE_THRESHOLD] = enter_set_ignore_threshold;
    siriparser_listen_enter[CLERI_GID_SET_PASSWORD] = enter_set_password;
    siriparser_listen_enter[CLERI_GID_SERIES_COLUMNS] = enter_xxx_columns;
    siriparser_listen_enter[CLERI_GID_SERVER_COLUMNS] = enter_xxx_columns;
    siriparser_listen_enter[CLERI_GID_SERIES_NAME] = enter_series_name;
    siriparser_listen_enter[CLERI_GID_SERIES_MATCH] = enter_series_match;
    siriparser_listen_enter[CLERI_GID_SERIES_RE] = enter_series_re;
    siriparser_listen_enter[CLERI_GID_SERIES_SEP] = enter_series_sep;
    siriparser_listen_enter[CLERI_GID_TIMEIT_STMT] = enter_timeit_stmt;
    siriparser_listen_enter[CLERI_GID_USER_COLUMNS] = enter_xxx_columns;
    siriparser_listen_enter[CLERI_GID_WHERE_POOL] = enter_where_xxx;
    siriparser_listen_enter[CLERI_GID_WHERE_SERIES] = enter_where_xxx;
    siriparser_listen_enter[CLERI_GID_WHERE_SERVER] = enter_where_xxx;
    siriparser_listen_enter[CLERI_GID_WHERE_USER] = enter_where_xxx;


    siriparser_listen_exit[CLERI_GID_AFTER_EXPR] = exit_after_expr;
    siriparser_listen_exit[CLERI_GID_ALTER_USER] = exit_alter_user;
    siriparser_listen_exit[CLERI_GID_BEFORE_EXPR] = exit_before_expr;
    siriparser_listen_exit[CLERI_GID_BETWEEN_EXPR] = exit_between_expr;
    siriparser_listen_exit[CLERI_GID_CALC_STMT] = exit_calc_stmt;
    siriparser_listen_exit[CLERI_GID_COUNT_POOLS] = exit_count_pools;
    siriparser_listen_exit[CLERI_GID_COUNT_SERIES] = exit_count_series;
    siriparser_listen_exit[CLERI_GID_COUNT_SERVERS] = exit_count_servers;
    siriparser_listen_exit[CLERI_GID_COUNT_USERS] = exit_count_users;
    siriparser_listen_exit[CLERI_GID_CREATE_USER] = exit_create_user;
    siriparser_listen_exit[CLERI_GID_DROP_SERIES] = exit_drop_series;
    siriparser_listen_exit[CLERI_GID_DROP_SHARD] = exit_drop_shard;
    siriparser_listen_exit[CLERI_GID_DROP_USER] = exit_drop_user;
    siriparser_listen_exit[CLERI_GID_GRANT_USER] = exit_grant_user;
    siriparser_listen_exit[CLERI_GID_LIST_POOLS] = exit_list_pools;
    siriparser_listen_exit[CLERI_GID_LIST_SERIES] = exit_list_series;
    siriparser_listen_exit[CLERI_GID_LIST_SERVERS] = exit_list_servers;
    siriparser_listen_exit[CLERI_GID_LIST_USERS] = exit_list_users;
    siriparser_listen_exit[CLERI_GID_REVOKE_USER] = exit_revoke_user;
    siriparser_listen_exit[CLERI_GID_SELECT_AGGREGATE] = exit_select_aggregate;
    siriparser_listen_exit[CLERI_GID_SELECT_STMT] = exit_select_stmt;
    siriparser_listen_exit[CLERI_GID_SET_DROP_THRESHOLD] = exit_set_drop_threshold;
    siriparser_listen_exit[CLERI_GID_SET_LOG_LEVEL] = exit_set_log_level;
    siriparser_listen_exit[CLERI_GID_SHOW_STMT] = exit_show_stmt;
    siriparser_listen_exit[CLERI_GID_TIMEIT_STMT] = exit_timeit_stmt;
}

/******************************************************************************
 * Free functions
 *****************************************************************************/

static void decref_server_object(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_server_decref((siridb_server_t *) query->data);

    /* normal free call */
    siridb_query_free(handle);
}

static void decref_user_object(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_user_decref((siridb_user_t *) query->data);

    /* normal free call */
    siridb_query_free(handle);
}

/******************************************************************************
 * Enter functions
 *****************************************************************************/

static void enter_access_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* bind ACCESS_EXPR children to query */
    query->data = query->nodes->node->children;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_server(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_ALTER)

    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cleri_node_t * server_node =
                    query->nodes->node->children->next->node->children->node;

    siridb_server_t * server;

    switch (server_node->cl_obj->tp)
    {
    case CLERI_TP_CHOICE:  // server name
        {
            char name[server_node->len - 1];
            strx_extract_string(name, server_node->str, server_node->len);
            server = siridb_servers_by_name(siridb->servers, name);
        }
        break;

    case CLERI_TP_REGEX:  // uuid
        {
            uuid_t uuid;
            char * str_uuid = strndup(server_node->str, server_node->len);
            server = (uuid_parse(str_uuid, uuid) == 0) ?
                    siridb_servers_by_uuid(siridb->servers, uuid) : NULL;
            free(str_uuid);
        }
        break;

    default:
        assert (0);
        break;
    }

    if (server == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find server: %.*s",
                (int) server_node->len,
                server_node->str);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        query->data = server;
        siridb_server_incref(server);
        query->free_cb = (uv_close_cb) decref_server_object;

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_alter_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_ALTER)

    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    cleri_node_t * user_node =
                query->nodes->node->children->next->node;
    siridb_user_t * user;

    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb->users, username, NULL)) == NULL)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'",
                username);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        query->data = user;
        siridb_user_incref(user);
        query->free_cb = (uv_close_cb) decref_user_object;

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_count_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_COUNT)

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(256);
    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_count_new();
    query->free_cb = (uv_close_cb) query_count_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_create_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_CREATE)

    /* bind user object to data and set correct free call */
    query->data = (siridb_user_t *) siridb_user_new();
    siridb_user_incref((siridb_user_t *) query->data);

    query->free_cb = (uv_close_cb) decref_user_object;

    SIRIPARSER_NEXT_NODE
}

static void enter_drop_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_drop_new();
    query->free_cb = (uv_close_cb) query_drop_free;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_DROP)
    SIRIPARSER_NEXT_NODE
}

static void enter_grant_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_GRANT)
    SIRIPARSER_NEXT_NODE
}

static void enter_grant_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    cleri_node_t * user_node =
                query->nodes->node->children->next->node;
    siridb_user_t * user;
    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb->users, username, NULL)) == NULL)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'", username);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        user->access_bit |=
                siridb_access_from_children((cleri_children_t *) query->data);

        query->data = user;
        siridb_user_incref(user);
        query->free_cb = (uv_close_cb) decref_user_object;

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_limit_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_list_t * qlist = (query_list_t *) query->data;
    int64_t limit = query->nodes->node->children->next->node->result;

    if (limit <= 0 || limit > MAX_LIST_LIMIT)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Limit must be a value between 0 and %d but received: %ld",
                MAX_LIST_LIMIT,
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
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_LIST)

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(QP_SUGGESTED_SIZE);
    qp_add_type(query->packer, QP_MAP_OPEN);

    qp_add_raw(query->packer, "columns", 7);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    query->data = query_list_new();
    query->free_cb = (uv_close_cb) query_list_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_merge_as(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;
    cleri_node_t * node = query->nodes->node->children->next->next->node;
    q_select->merge_as = (char *) malloc(node->len - 1);

    if (q_select->merge_as == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        strx_extract_string(q_select->merge_as, node->str, node->len);
    }

    if (IS_MASTER && query->nodes->node->children->next->next->next != NULL)
    {
        q_select->mlist = siridb_aggregate_list(
                query->nodes->node->children->next->next->next->node->
                    children->node->children->next->node->children,
                query->err_msg);
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_revoke_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_REVOKE)
    SIRIPARSER_NEXT_NODE
}

static void enter_revoke_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    cleri_node_t * user_node =
                query->nodes->node->children->next->node;
    siridb_user_t * user;
    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb->users, username, NULL)) == NULL)
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

        query->data = user;
        siridb_user_incref(user);
        query->free_cb = (uv_close_cb) decref_user_object;

        SIRIPARSER_NEXT_NODE
    }
}

static void enter_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_SELECT)

#ifdef DEBUG
    assert (query->packer == NULL && query->data == NULL);
#endif

    query->data = query_select_new();
    if (query->data != NULL)
    {
        /* this is not critical since pmap is allowed to be NULL */
        ((query_select_t *) query->data)->pmap =
                (!IS_MASTER || siridb_is_reindexing(siridb)) ?
                        NULL : imap_new();

        query->free_cb = (uv_close_cb) query_select_free;
    }

    query->packer = sirinet_packer_new(QP_SUGGESTED_SIZE);
    if (query->packer != NULL)
    {
        qp_add_type(query->packer, QP_MAP_OPEN);
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_set_ignore_threshold(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    if (    query->nodes->node->children->next->next->node->children->node->
            cl_obj->via.dummy->gid == CLERI_GID_K_TRUE)
    {
        q_drop->flags |= QUERIES_IGNORE_DROP_THRESHOLD;
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_set_password(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_user_t * user = (siridb_user_t *) query->data;
    cleri_node_t * pw_node =
            query->nodes->node->children->next->next->node;

    char password[pw_node->len - 1];
    strx_extract_string(password, pw_node->str, pw_node->len);

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
    siridb_query_t * query = (siridb_query_t *) handle->data;
    cleri_node_t * node = query->nodes->node;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;
    siridb_series_t * series;
    uint16_t pool;
    char series_name[node->len - 1];

    /* extract series name */
    strx_extract_string(series_name, node->str, node->len);

    /* get pool for series name */
    pool = siridb_lookup_sn(siridb->pools->lookup, series_name);

    /* check if this series belongs to 'this' pool and if so get the series */
    if (pool == siridb->server->pool)
    {
        if ((series = ct_get(siridb->series, series_name)) == NULL)
        {
            if (!siridb_is_reindexing(siridb))
            {
                /* the series does not exist */
                snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                        "Cannot find series: '%s'", series_name);

                /* free series_name and return with send_errror.. */
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
        else if (imap_add(q_wrapper->series_map, series->id, series) == 1)
        {
            /*
             * Bind the series to the query,
             * increment ref count if successful
             */
            siridb_series_incref(series);
        }
    }
    else if (q_wrapper->pmap != NULL && imap_add(
            q_wrapper->pmap,
            pool,
            (siridb_pool_t *) (siridb->pools->pool + pool)) < 0)
    {
        log_critical("Cannot pool to pool map.");
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_series_match(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_wrapper_t *) query->data)->series_map = imap_new();

    SIRIPARSER_NEXT_NODE
}

static void enter_series_re(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cleri_node_t * node = query->nodes->node;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;

    /* we must send this query to all pools */
    if (q_wrapper->pmap != NULL)
    {
        imap_free(q_wrapper->pmap, NULL);
        q_wrapper->pmap = NULL;
    }

    /* extract and compile regular expression */
    if (siridb_re_compile(
            &q_wrapper->regex,
            &q_wrapper->regex_extra,
            node->str,
            node->len,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        q_wrapper->slist = imap_2slist_ref(
                (   q_wrapper->update_cb == NULL ||
                    q_wrapper->update_cb == &imap_union_ref ||
                    q_wrapper->update_cb == &imap_symmetric_difference_ref) ?
                        siridb->series_map : q_wrapper->series_map);

        q_wrapper->series_tmp = (q_wrapper->update_cb == NULL) ?
                q_wrapper->series_map : imap_new();

        if (q_wrapper->slist != NULL && q_wrapper->series_tmp != NULL)
        {
            async_series_re(handle);
        }
    }

    /* handle is handled or a signal is raised */
}

static void enter_series_sep(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;

    switch (query->nodes->node->children->node->cl_obj->via.dummy->gid)
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

static void enter_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query->timeit = qp_packer_new(512);

    qp_add_raw(query->timeit, "__timeit__", 10);
    qp_add_type(query->timeit, QP_ARRAY_OPEN);

    SIRIPARSER_NEXT_NODE
}

static void enter_where_xxx(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    cexpr_t * cexpr =
            cexpr_from_node(query->nodes->node->children->next->node);

    if (cexpr == NULL)
    {
        sprintf(query->err_msg, "Max depth reached in 'where' expression!");
        log_critical(query->err_msg);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        ((query_wrapper_t *) query->data)->where_expr = cexpr;
        SIRIPARSER_NEXT_NODE
    }
}

static void enter_xxx_columns(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    cleri_children_t * columns = query->nodes->node->children;
    query_list_t * qlist = (query_list_t *) query->data;

    qlist->props = slist_new(DEFAULT_ALLOC_COLUMNS);

    if (qlist->props != NULL)
    {
        while (1)
        {
            qp_add_raw(query->packer, columns->node->str, columns->node->len);

            if (slist_append_safe(
                    &qlist->props,
                    &columns->node->children->node->cl_obj->via.dummy->gid))
            {
                ERR_ALLOC
            }

            if (columns->next == NULL)
            {
                break;
            }

            columns = columns->next->next;
        }
    }

    SIRIPARSER_NEXT_NODE
}

/******************************************************************************
 * Exit functions
 *****************************************************************************/

static void exit_after_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    ((query_select_t *) query->data)->start_ts =
            (uint64_t *) &query->nodes->node->children->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_alter_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_socket_t *) query->client->data)->siridb))
    {
        return;  /* signal is set */
    }

    query->packer = sirinet_packer_new(1024);

    if (query->packer != NULL)
    {
        qp_add_type(query->packer, QP_MAP_OPEN);

        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                "Successful changed password for user '%s'.",
                ((siridb_user_t *) query->data)->username);
    }

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_before_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_select_t *) query->data)->end_ts =
            (uint64_t *) &query->nodes->node->children->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_between_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_select_t *) query->data)->start_ts = (uint64_t *)
            &query->nodes->node->children->next->node->result;

    ((query_select_t *) query->data)->end_ts = (uint64_t *)
            &query->nodes->node->children->next->next->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_calc_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cleri_node_t * calc_node = query->nodes->node->children->node;

    query->packer = sirinet_packer_new(64);
    qp_add_type(query->packer, QP_MAP_OPEN);
    qp_add_raw(query->packer, "calc", 4);

    if (query->time_precision == SIRIDB_TIME_DEFAULT)
        qp_add_int64(query->packer, calc_node->result);
    else
    {
        double factor =
                pow(1000.0, query->time_precision - siridb->time->precision);
        qp_add_int64(query->packer, (int64_t) (calc_node->result * factor));
    }

    SIRIPARSER_NEXT_NODE
}

static void exit_count_pools(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_count_t * q_count = (query_count_t *) query->data;
    siridb_pool_t * pool = siridb->pools->pool + siridb->server->pool;

    siridb_pool_walker_t wpool = {
            .servers=pool->len,
            .series=siridb->series->len,
            .pid=siridb->server->pool
    };

    qp_add_raw(query->packer, "pools", 5);

    if (q_count->where_expr == NULL)
    {
        qp_add_int64(query->packer, siridb->pools->len);
        SIRIPARSER_NEXT_NODE
    }
    else
    {
        MASTER_CHECK_POOLS_ONLINE(siridb)

        q_count->n = cexpr_run(
                q_count->where_expr,
                (cexpr_cb_t) siridb_pool_cexpr_cb,
                &wpool);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_POOLS,
                    (sirinet_promises_cb) on_count_xxx_response);
        }
        else
        {
            qp_add_int64(query->packer, q_count->n);
            SIRIPARSER_NEXT_NODE
        }
    }
}

static void exit_count_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_count_t * q_count = (query_count_t *) query->data;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    qp_add_raw(query->packer, "series", 6);

    if (q_count->where_expr == NULL)
    {
        q_count->n = (q_count->series_map == NULL) ?
                siridb->series_map->len : q_count->series_map->len;

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_POOLS,
                    (sirinet_promises_cb) on_count_xxx_response);
        }
        else
        {
            qp_add_int64(query->packer, q_count->n);

            SIRIPARSER_NEXT_NODE
        }
    }
    else
    {
        q_count->slist = imap_2slist_ref(
                (q_count->series_map == NULL) ?
                        siridb->series_map : q_count->series_map);

        if (q_count->slist != NULL)
        {
            async_count_series(handle);
        }
    }
}

static void exit_count_servers(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    query_count_t * q_count = (query_count_t *) query->data;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_server_cexpr_cb;

    qp_add_raw(query->packer, "servers", 7);

    int is_local = IS_MASTER;

    /* if is_local, check if we use 'remote' props in where expression */
    if (is_local && where_expr != NULL)
    {
        is_local = !cexpr_contains(where_expr, siridb_server_is_remote_prop);
    }

    if (is_local)
    {
        for (   llist_node_t * node = siridb->servers->first;
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
                (sirinet_promises_cb) on_count_xxx_response);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_count_users(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    llist_node_t * node = siridb->users->first;
    cexpr_t * where_expr = ((query_count_t *) query->data)->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_user_cexpr_cb;
    int n = 0;

    qp_add_raw(query->packer, "users", 5);

    while (node != NULL)
    {
        if (where_expr == NULL || cexpr_run(where_expr, cb, node->data))
        {
            n++;
        }
        node = node->next;
    }

    qp_add_int64(query->packer, n);

    SIRIPARSER_NEXT_NODE
}

static void exit_create_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    siridb_user_t * user = (siridb_user_t *) query->data;
    cleri_node_t * user_node =
            query->nodes->node->children->next->node;

#ifdef DEBUG
    /* both username and packer should be NULL at this point */
    assert(user->username == NULL);
    assert(query->packer == NULL);
#endif

    MASTER_CHECK_POOLS_ONLINE(siridb)

    user->username = (char *) malloc(user_node->len - 1);
    if (user->username == NULL)
    {
        ERR_ALLOC
        return;
    }
    strx_extract_string(user->username, user_node->str, user_node->len);

    if (siridb_users_add_user(
            siridb,
            user,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        /* success, we do not need to free the user anymore */
        query->free_cb = (uv_close_cb) siridb_query_free;

        query->packer = sirinet_packer_new(1024);
        qp_add_type(query->packer, QP_MAP_OPEN);

        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                "User '%s' is created successfully.", user->username);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response);
        }
        else
        {
            SIRIPARSER_NEXT_NODE
        }
    }
}

static void exit_drop_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    /*
     * This function will be called twice when using a where statement.
     * The second time the where_expr is NULL and the reason we do this is
     * so that we can honor a correct drop threshold.
     */
    if (q_drop->where_expr != NULL)
    {
        /* we transform the references from imap to slist */
        q_drop->slist = imap_slist_pop(q_drop->series_map);

        if (q_drop->slist != NULL)
        {
            /* now we can simply destroy the imap */
            imap_free(q_drop->series_map, NULL);

            /* create a new one */
            q_drop->series_map = imap_new();

            if (q_drop->series_map != NULL)
            {
                async_filter_series(handle);
            }
        }
    }
    else
    {
        double percent = (double)
                q_drop->series_map->len / siridb->series_map->len;

        if (IS_MASTER &&
            q_drop->series_map->len &&
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

            q_drop->n = q_drop->series_map->len;

            q_drop->slist = imap_2slist_ref(q_drop->series_map);

            if (q_drop->slist != NULL)
            {
                async_drop_series(handle);
            }
        }
    }
}

static void exit_drop_shard(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    cleri_node_t * shard_id_node =
                query->nodes->node->children->next->node;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    int64_t shard_id = atoll(shard_id_node->str);

    uv_mutex_lock(&siridb->shards_mutex);

    siridb_shard_t * shard = imap_pop(siridb->shards, shard_id);

    uv_mutex_unlock(&siridb->shards_mutex);

    if (shard == NULL)
    {
        log_debug(
                "Cannot find shard '%ld' on server '%s'",
                shard_id,
                siridb->server->name);
    }
    else
    {
        siridb_series_t * series;

        /*
         * We need a series mutex here since we depend on the series index
         * and we create a copy since series might be removed when the length
         * of series is zero after removing the shard
         */
        uv_mutex_lock(&siridb->series_mutex);

        /* create a copy since series might be removed */
        slist_t * slist = imap_2slist(siridb->series_map);

        if (slist != NULL)
        {
            for (size_t i = 0; i < slist->len; i++)
            {
                series = (siridb_series_t *) slist->data[i];
                if (shard->id % siridb->duration_num == series->mask)
                {
                    /* series might be destroyed after this call */
                    siridb_series_remove_shard_num32(siridb, series, shard);
                }
            }
            slist_free(slist);
        }

        uv_mutex_unlock(&siridb->series_mutex);

        shard->flags |= SIRIDB_SHARD_WILL_BE_REMOVED;

        siridb_shard_decref(shard);

    }

    /* we send back a successful message even when the shard was not found
     * because it might be dropped on another server so at least the shard
     * is gone.
     */
    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "Shard '%ld' is dropped successfully.", shard_id);

    SIRIPARSER_NEXT_NODE
}

static void exit_drop_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    cleri_node_t * user_node =
            query->nodes->node->children->next->node;
    char username[user_node->len - 1];

    /* we need to free user-name */
    strx_extract_string(username, user_node->str, user_node->len);

    if (siridb_users_drop_user(
            ((sirinet_socket_t *) query->client->data)->siridb,
            username,
            query->err_msg))
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        QP_ADD_SUCCESS
        qp_add_fmt_safe(query->packer,
                "User '%s' is dropped successfully.", username);

        if (IS_MASTER)
        {
            siridb_query_forward(
                    handle,
                    SIRIDB_QUERY_FWD_UPDATE,
                    (sirinet_promises_cb) on_update_xxx_response);
        }
        else
        {
            SIRIPARSER_NEXT_NODE
        }
    }
}

static void exit_grant_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_socket_t *) query->client->data)->siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt_safe(query->packer,
            "Successfully granted permissions to user '%s'.",
            ((siridb_user_t *) query->data)->username);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_list_pools(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_pool_t * pool = siridb->pools->pool + siridb->server->pool;
    siridb_pool_walker_t wpool = {
            .servers=pool->len,
            .series=siridb->series->len,
            .pid=siridb->server->pool
    };
    uint_fast16_t prop;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;

    MASTER_CHECK_POOLS_ONLINE(siridb)

    if (q_list->props == NULL)
    {
        q_list->props = slist_new(3);
        slist_append(q_list->props, &GID_K_POOL);
        slist_append(q_list->props, &GID_K_SERVERS);
        slist_append(q_list->props, &GID_K_SERIES);
        qp_add_raw(query->packer, "pool", 4);
        qp_add_raw(query->packer, "servers", 7);
        qp_add_raw(query->packer, "series", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, "pools", 5);
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
                qp_add_int16(query->packer, wpool.pid);
                break;
            case CLERI_GID_K_SERVERS:
                qp_add_int16(query->packer, wpool.servers);
                break;
            case CLERI_GID_K_SERIES:
                qp_add_int64(query->packer, wpool.series);
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
                (sirinet_promises_cb) on_list_xxx_response);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        SIRIPARSER_NEXT_NODE
    }

}

static void exit_list_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    if (q_list->props == NULL)
    {
        q_list->props = slist_new(1);
        slist_append(q_list->props, &GID_K_NAME);
        qp_add_raw(query->packer, "name", 4);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, "series", 6);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    q_list->slist = imap_2slist_ref((q_list->series_map == NULL) ?
                    siridb->series_map : q_list->series_map);

    if (q_list->slist != NULL)
    {
        async_list_series(handle);
    }
}

static void exit_list_servers(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    query_list_t * q_list = (query_list_t *) query->data;
    int is_local = IS_MASTER;

    /* if is_local, check if we need ask for 'remote' columns */
    if (is_local && q_list->props != NULL)
    {
        for (int i = 0; i < q_list->props->len; i++)
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
        q_list->props = slist_new(5);
        slist_append(q_list->props, &GID_K_NAME);
        slist_append(q_list->props, &GID_K_POOL);
        slist_append(q_list->props, &GID_K_VERSION);
        slist_append(q_list->props, &GID_K_ONLINE);
        slist_append(q_list->props, &GID_K_STATUS);
        qp_add_raw(query->packer, "name", 4);
        qp_add_raw(query->packer, "pool", 4);
        qp_add_raw(query->packer, "version", 7);
        qp_add_raw(query->packer, "online", 6);
        qp_add_raw(query->packer, "status", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, "servers", 7);
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
                (sirinet_promises_cb) on_list_xxx_response);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_list_users(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    llist_node_t * node =
            ((sirinet_socket_t *) query->client->data)->siridb->users->first;
    slist_t * props = ((query_list_t *) query->data)->props;
    cexpr_t * where_expr = ((query_list_t *) query->data)->where_expr;
    cexpr_cb_t cb = (cexpr_cb_t) siridb_user_cexpr_cb;

    size_t i;
    siridb_user_t * user;

    if (props == NULL)
    {
        qp_add_raw(query->packer, "user", 4);
        qp_add_raw(query->packer, "access", 6);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, "users", 5);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    while (node != NULL)
    {
        user = node->data;

        if (where_expr == NULL || cexpr_run(where_expr, cb, user))
        {
            qp_add_type(query->packer, QP_ARRAY_OPEN);

            if (props == NULL)
            {
                siridb_user_prop(user, query->packer, CLERI_GID_K_USER);
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

    SIRIPARSER_NEXT_NODE
}

static void exit_revoke_user(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_socket_t *) query->client->data)->siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt_safe(query->packer,
            "Successfully revoked permissions from user '%s'.",
            ((siridb_user_t *) query->data)->username);

    if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_update_xxx_response);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

static void exit_select_aggregate(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;

    if (q_select->where_expr != NULL)
    {
        /* we transform the references from imap to slist */
        q_select->slist = imap_slist_pop(q_select->series_map);

        if (q_select->slist != NULL)
        {
            /* now we can simply destroy the imap */
            imap_free(q_select->series_map, NULL);

            /* create a new one */
            q_select->series_map = imap_new();

            if (q_select->series_map != NULL)
            {
                async_filter_series(handle);
            }
        }
    }
    else if (siridb_presuf_add(&q_select->presuf, query->nodes->node) != NULL)
    {
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
                slist_t * plist = slist_new(SLIST_DEFAULT_SIZE);

                if (plist == NULL || ct_add(
                        q_select->result,
                        siridb_presuf_name(
                                q_select->presuf,
                                q_select->merge_as,
                                strlen(q_select->merge_as)),
                        plist))
                {
                    sprintf(query->err_msg,
                            "Critical error while adding points to map.");
                    slist_free(plist);
                    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                    return;
                }
            }

            if (q_select->series_map->len)
            {
                q_select->alist = siridb_aggregate_list(
                        query->nodes->node->children->node->children,
                        query->err_msg);
                if (q_select->alist == NULL)
                {
                    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                }
                else
                {
                    q_select->slist = imap_2slist_ref(q_select->series_map);
                    if (q_select->slist != NULL)
                    {
                        async_select_aggregate(handle);
                    }
                }
            }
            else
            {
                SIRIPARSER_NEXT_NODE
            }
        }
    }
}

static void exit_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;

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
                    (sirinet_promises_cb) on_select_response);
        }
        else
        {
            uv_work_t * work = (uv_work_t *) malloc(sizeof(uv_work_t));
            if (work == NULL)
            {
                ERR_ALLOC
            }
            else
            {
                siri_async_incref(handle);
                work->data = handle;
                uv_queue_work(
                            siri.loop,
                            work,
                            &master_select_work,
                            &master_select_work_finish);
            }
        }
    }
    else
    {
        if (    qp_add_raw(query->packer, "select", 6) ||
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
            sprintf(query->err_msg, "Memory allocation error.");
            siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        }
        else
        {
            SIRIPARSER_NEXT_NODE
        }
    }
}

static void exit_set_drop_threshold(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_ALTER)

    MASTER_CHECK_POOLS_ONLINE(siridb)

    cleri_node_t * node = query->nodes->node->children->next->next->node;

    double drop_threshold = strx_to_double(node->str, node->len);

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
            query->packer = sirinet_packer_new(1024);
            if (query->packer != NULL)
            {
                qp_add_type(query->packer, QP_MAP_OPEN);

                QP_ADD_SUCCESS
                qp_add_fmt_safe(query->packer,
                        "Successful changed drop_threshold from "
                        "%g to %g.",
                        old,
                        siridb->drop_threshold);

                if (IS_MASTER)
                {
                    siridb_query_forward(
                            handle,
                            SIRIDB_QUERY_FWD_UPDATE,
                            (sirinet_promises_cb) on_update_xxx_response);
                }
                else
                {
                    SIRIPARSER_NEXT_NODE
                }
            }
        }
    }
}

static void exit_set_log_level(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

#ifdef DEBUG
    assert (query->data != NULL);
    assert (IS_MASTER);
#endif

    siridb_server_t * server = query->data;
    cleri_node_t * node =
            query->nodes->node->children->next->next->node->children->node;

    int log_level;

    switch (node->cl_obj->via.keyword->gid)
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
        break;
    }

    /*
     * we can set the success message, we just ignore the message in case an
     * error occurs.
     */
    if (
        /* create a new packer with success message */
        (query->packer = sirinet_packer_new(1024)) == NULL ||

        qp_add_type(query->packer, QP_MAP_OPEN) ||

        qp_add_raw(query->packer, "success_msg", 11) ||

        qp_add_fmt_safe(query->packer,
                    "Successful set log level to '%s' on '%s'.",
                    logger_level_name(log_level),
                    server->name))
    {
        return;  /* signal is raised */
    }

    if (server == siridb->server)
    {
        logger_set_level(log_level);

        SIRIPARSER_NEXT_NODE
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
                        on_ack_response,
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

static void exit_show_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_SHOW)

    cleri_children_t * children =
            query->nodes->node->children->next->node->children;
    siridb_props_cb prop_cb;

#ifdef DEBUG
    assert (query->packer == NULL);
#endif

    query->packer = sirinet_packer_new(4096);
    qp_add_type(query->packer, QP_MAP_OPEN);
    qp_add_raw(query->packer, "data", 4);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    siridb_user_t * user = ((sirinet_socket_t *) query->client->data)->origin;
    who_am_i = user->username;

    if (children->node == NULL)
    {
        /* show all properties */
        int i;

        for (i = 0; i < KW_COUNT; i++)
        {
            if ((prop_cb = siridb_props[i]) == NULL)
            {
                continue;
            }
            prop_cb(((sirinet_socket_t *) query->client->data)->siridb,
                    query->packer, 1);
        }
    }
    else
    {
        /* show selected properties chosen by query */
        while (1)
        {
            /* get the callback */
            prop_cb = siridb_props[children->node->children->node->
                                   cl_obj->via.keyword->gid - KW_OFFSET];
#ifdef DEBUG
            /* TODO: can be removed as soon as all props are implemented */
            if (prop_cb == NULL)
            {
                LOGC("not implemented");
            }
            else
#endif
            prop_cb(((sirinet_socket_t *) query->client->data)->siridb,
                    query->packer, 1);

            if (children->next == NULL)
            {
                break;
            }

            /* skip one which is the delimiter */
            children = children->next->next;
        }
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_NEXT_NODE
}

static void exit_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    struct timespec end;

    clock_gettime(CLOCK_REALTIME, &end);

    qp_add_type(query->timeit, QP_MAP2);
    qp_add_raw(query->timeit, "server", 6);
    qp_add_string(
            query->timeit,
            ((sirinet_socket_t *) query->client->data)->siridb->server->name);
    qp_add_raw(query->timeit, "time", 4);
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
                PKG_HEADER_SIZE);
        qp_add_type(query->packer, QP_MAP_OPEN);
    }

    /* extend packer with timeit information */
    qp_packer_extend(query->packer, query->timeit);

    SIRIPARSER_NEXT_NODE
}

/******************************************************************************
 * Async loop functions.
 *****************************************************************************/

static void async_count_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_series_t * series;
    query_count_t * q_count = (query_count_t *) query->data;
    uv_async_t * async_more = NULL;

    size_t index_end = q_count->slist_index + MAX_ITERATE_COUNT;

    if (index_end >= q_count->slist->len)
    {
        index_end = q_count->slist->len;
    }
    else
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    for (; q_count->slist_index < index_end; q_count->slist_index++)
    {
        series = (siridb_series_t *) q_count->slist->data[q_count->slist_index];
        q_count->n += cexpr_run(
                    q_count->where_expr,
                    (cexpr_cb_t) siridb_series_cexpr_cb,
                    series);
        siridb_series_decref(series);
    }

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_count_series);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_count_xxx_response);
    }
    else
    {
        qp_add_int64(query->packer, q_count->n);

        SIRIPARSER_NEXT_NODE
    }
}

static void async_drop_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    query_drop_t * q_drop = (query_drop_t *) query->data;

    siridb_series_t * series;
    uv_async_t * async_more = NULL;

    size_t index_end = q_drop->slist_index + MAX_ITERATE_COUNT;

    if (index_end >= q_drop->slist->len)
    {
        index_end = q_drop->slist->len;
    }
    else
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    uv_mutex_lock(&siridb->series_mutex);

    for (; q_drop->slist_index < index_end; q_drop->slist_index++)
    {
        series = (siridb_series_t *) q_drop->slist->data[q_drop->slist_index];

        siridb_series_drop(siridb, series);

        siridb_series_decref(series);
    }

    uv_mutex_unlock(&siridb->series_mutex);

    /* flush dropped file change to disk */
    siridb_series_flush_dropped(siridb);

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_drop_series);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else if (IS_MASTER)
    {
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_UPDATE,
                (sirinet_promises_cb) on_drop_xxx_response);
    }
    else
    {
        qp_add_int64(query->packer, q_drop->n);

        SIRIPARSER_NEXT_NODE
    }
}

static void async_filter_series(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;
    cexpr_t * where_expr = q_wrapper->where_expr;
    uv_async_t * async_more = NULL;
    siridb_series_t * series;
    size_t index_end = q_wrapper->slist_index + MAX_ITERATE_COUNT;

    if (index_end >= q_wrapper->slist->len)
    {
        index_end = q_wrapper->slist->len;
    }
    else
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    for (; q_wrapper->slist_index < index_end; q_wrapper->slist_index++)
    {
        series = (siridb_series_t *)
                q_wrapper->slist->data[q_wrapper->slist_index];

        if (cexpr_run(
                where_expr,
                (cexpr_cb_t) siridb_series_cexpr_cb,
                series))
        {
            imap_add(q_wrapper->series_map, series->id, series);
        }
        else
        {
            siridb_series_decref(series);
        }
    }

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_filter_series);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else
    {
        /* free the s-list object and reset index */
        slist_free(q_wrapper->slist);

        q_wrapper->slist = NULL;
        q_wrapper->slist_index = 0;

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
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_list_t * q_list = (query_list_t *) query->data;
    slist_t * props = q_list->props;
    cexpr_t * where_expr = q_list->where_expr;
    uv_async_t * async_more = NULL;
    siridb_series_t * series;
    size_t i;
    size_t index_end = q_list->slist_index + MAX_ITERATE_COUNT;

    if (index_end >= q_list->slist->len)
    {
        index_end = q_list->slist->len;
    }
    else
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    for (;  q_list->limit && q_list->slist_index < index_end;
            q_list->slist_index++)
    {
        series = (siridb_series_t *) q_list->slist->data[q_list->slist_index];

        if (where_expr == NULL || cexpr_run(
                    where_expr,
                    (cexpr_cb_t) siridb_series_cexpr_cb,
                    series))
        {
            if (!--q_list->limit)
            {
                free(async_more);
                async_more = NULL;
            }

            qp_add_type(query->packer, QP_ARRAY_OPEN);

            for (i = 0; i < props->len; i++)
            {
                switch(*((uint32_t *) props->data[i]))
                {
                case CLERI_GID_K_NAME:
                    qp_add_raw(query->packer, series->name, series->name_len);
                    break;
                case CLERI_GID_K_LENGTH:
                    qp_add_int32(query->packer, series->length);
                    break;
                case CLERI_GID_K_TYPE:
                    qp_add_string(query->packer, series_type_map[series->tp]);
                    break;
                case CLERI_GID_K_POOL:
                    qp_add_int16(query->packer, series->pool);
                    break;
                case CLERI_GID_K_START:
                    qp_add_int64(query->packer, series->start);
                    break;
                case CLERI_GID_K_END:
                    qp_add_int64(query->packer, series->end);
                    break;
                }
            }

            qp_add_type(query->packer, QP_ARRAY_CLOSE);
        }

        siridb_series_decref(series);
    }

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_list_series);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else if (IS_MASTER && q_list->limit)
    {
        /* we have not reached the limit, send the query to other pools */
        siridb_query_forward(
                handle,
                SIRIDB_QUERY_FWD_POOLS,
                (sirinet_promises_cb) on_list_xxx_response);
    }
    else
    {
        qp_add_type(query->packer, QP_ARRAY_CLOSE);

        SIRIPARSER_NEXT_NODE
    }
}

static void async_select_aggregate(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    uv_async_t * async_more = NULL;
    siridb_series_t * series;
    siridb_points_t * points;
    siridb_points_t * aggr_points;

    if (q_select->n > MAX_SELECT_POINTS)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Query has reached the maximum number of selected points "
                "(%u). Please use another time window, an aggregation "
                "function or select less series to reduce the number of "
                "points.",
                MAX_SELECT_POINTS);

        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    series = (siridb_series_t *)
            q_select->slist->data[q_select->slist_index];

    /*
     * We must decrement the ref count immediately since we now update the
     * index by one. The series will not be freed since at least 'series_map'
     * still has a reference.
     */
    siridb_series_decref(series);

    if ((++q_select->slist_index) < q_select->slist->len)
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    uv_mutex_lock(&siridb->series_mutex);

    points = (series->flags & SIRIDB_SERIES_IS_DROPPED) ?
            NULL : siridb_series_get_points_num32(
                    series,
                    q_select->start_ts,
                    q_select->end_ts);

    uv_mutex_unlock(&siridb->series_mutex);

    if (points != NULL)
    {
        const char * name;

        for (size_t i = 0; points->len && i < q_select->alist->len; i++)
        {
            aggr_points = siridb_aggregate_run(
                    points,
                    (siridb_aggr_t *) q_select->alist->data[i],
                    query->err_msg);

            siridb_points_free(points);

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
            slist_t ** plist;

            name = siridb_presuf_name(
                    q_select->presuf,
                    q_select->merge_as,
                    strlen(q_select->merge_as));
            plist = (slist_t **) ct_getaddr(q_select->result, name);

            if (    name == NULL ||
                    *plist == NULL ||
                    slist_append_safe(plist, points))
            {
                sprintf(query->err_msg, "Error adding points to map.");
                siridb_points_free(points);
                log_critical("Critical error adding points");
                siridb_query_send_error(handle, CPROTO_ERR_QUERY);
                return;
            }
        }
    }

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_select_aggregate);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else
    {
        siridb_aggregate_list_free(q_select->alist);
        q_select->alist = NULL;

        slist_free(q_select->slist);
        q_select->slist = NULL;
        q_select->slist_index = 0;

        SIRIPARSER_NEXT_NODE
    }
}

static void async_series_re(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_wrapper_t * q_wrapper = (query_wrapper_t *) query->data;
    uv_async_t * async_more = NULL;
    siridb_series_t * series;
    size_t index_end = q_wrapper->slist_index + MAX_ITERATE_COUNT;

    if (index_end >= q_wrapper->slist->len)
    {
        index_end = q_wrapper->slist->len;
    }
    else
    {
        async_more = (uv_async_t *) malloc(sizeof(uv_async_t));
        if (async_more == NULL)
        {
            ERR_ALLOC
        }
    }

    int pcre_exec_ret;

    for (; q_wrapper->slist_index < index_end; q_wrapper->slist_index++)
    {
        series = (siridb_series_t *)
                q_wrapper->slist->data[q_wrapper->slist_index];

        pcre_exec_ret = pcre_exec(
                q_wrapper->regex,
                q_wrapper->regex_extra,
                series->name,
                series->name_len,
                0,                     // start looking at this point
                0,                     // OPTIONS
                NULL,
                0);                    // length of sub_str_vec

        if (!pcre_exec_ret)
        {
            imap_add(q_wrapper->series_tmp, series->id, series);
        }
        else
        {
            siridb_series_decref(series);
        }
    }

    if (async_more != NULL)
    {
        async_more->data = (void *) handle->data;
        uv_async_init(
                siri.loop,
                async_more,
                (uv_async_cb) &async_series_re);
        uv_async_send(async_more);
        uv_close((uv_handle_t *) handle, (uv_close_cb) free);
    }
    else
    {
        /* free the s-list object and reset index */
        slist_free(q_wrapper->slist);

        q_wrapper->slist = NULL;
        q_wrapper->slist_index = 0;

        if (q_wrapper->update_cb != NULL)
        {
            (*q_wrapper->update_cb)(
                    q_wrapper->series_map,
                    q_wrapper->series_tmp,
                    (imap_free_cb) &siridb_series_decref);
        }

        q_wrapper->series_tmp = NULL;

        SIRIPARSER_NEXT_NODE
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
        siridb_query_t * query = (siridb_query_t *) handle->data;

        if (status == PROMISE_SUCCESS)
        {
            switch (pkg->tp)
            {
            case BPROTO_ACK_LOG_LEVEL:
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
            SIRIPARSER_NEXT_NODE
        }
    }


    /* we must free the promise */
    free(promise);
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_count_xxx_response(slist_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = (siridb_query_t *) handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_count = qp_object_new();

    if (qp_count == NULL)
    {
        return;  /* critical error, signal is set */
    }

    query_count_t * q_count = query->data;

    for (size_t i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            unpacker = qp_unpacker_new(pkg->data, pkg->len);

            if (unpacker == NULL)
            {
                return;  /* critical error, signal is set */
            }

            if (    qp_is_map(qp_next(unpacker, NULL)) &&
                    qp_is_raw(qp_next(unpacker, NULL)) &&  // servers etc.
                    qp_is_int(qp_next(unpacker, qp_count))) // one result
            {
                q_count->n += qp_count->via->int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, unpacker);
                }
            }

            /* free the unpacker */
            qp_unpacker_free(unpacker);
        }

        /* make sure we free the promise and data */
        free(promise->data);
        free(promise);
    }

    qp_object_free(qp_count);
    qp_add_int64(query->packer, q_count->n);

    SIRIPARSER_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_drop_xxx_response(slist_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    siridb_query_t * query = (siridb_query_t *) handle->data;
    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t * unpacker;
    qp_obj_t * qp_drop = qp_object_new();

    if (qp_drop == NULL)
    {
        return;  /* critical error, signal is set */
    }

    query_drop_t * q_drop = query->data;

    for (size_t i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            unpacker = qp_unpacker_new(pkg->data, pkg->len);

            if (unpacker == NULL)
            {
                return;  /* critical error, signal is set */
            }

            if (    qp_is_map(qp_next(unpacker, NULL)) &&
                    qp_is_raw(qp_next(unpacker, NULL)) &&  // servers etc.
                    qp_is_int(qp_next(unpacker, qp_drop))) // one result
            {
                q_drop->n += qp_drop->via->int64;

                /* extract time-it info if needed */
                if (query->timeit != NULL)
                {
                    siridb_query_timeit_from_unpacker(query, unpacker);
                }
            }

            /* free the unpacker */
            qp_unpacker_free(unpacker);
        }

        /* make sure we free the promise and data */
        free(promise->data);
        free(promise);
    }

    qp_object_free(qp_drop);

    qp_add_fmt(query->packer,
            "Successfully dropped %lu series.", q_drop->n);

    SIRIPARSER_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_list_xxx_response(slist_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t * unpacker;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_list_t * q_list = (query_list_t *) query->data;

    for (size_t i = 0; i < promises->len; i++)
    {
        promise = promises->data[i];

        if (promise == NULL)
        {
            continue;
        }

        pkg = promise->data;

        if (pkg != NULL && pkg->tp == BPROTO_RES_QUERY)
        {
            unpacker = qp_unpacker_new(pkg->data, pkg->len);

            if (unpacker != NULL)
            {

                if (    qp_is_map(qp_next(unpacker, NULL)) &&
                        qp_is_raw(qp_next(unpacker, NULL)) && // columns
                        qp_is_array(qp_skip_next(unpacker)) &&
                        qp_is_raw(qp_next(unpacker, NULL)) && // series/...
                        qp_is_array(qp_next(unpacker, NULL)))  // results
                {
                    while (qp_is_array(qp_current(unpacker)))
                    {
                        if (q_list->limit)
                        {
                            qp_packer_extend_fu(query->packer, unpacker);
                            q_list->limit--;
                        }
                        else
                        {
                            qp_skip_next(unpacker);
                        }
                    }

                    /* extract time-it info if needed */
                    if (query->timeit != NULL)
                    {
                        siridb_query_timeit_from_unpacker(query, unpacker);
                    }

                }
                /* free the unpacker */
                qp_unpacker_free(unpacker);
            }
        }

        /* make sure we free the promise and data */
        free(promise->data);
        free(promise);
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_NEXT_NODE
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_select_response(slist_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    qp_unpacker_t * unpacker;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    size_t err_count = 0;
    query_select_t * q_select = (query_select_t *) query->data;
    qp_obj_t * qp_name = qp_object_new();
    qp_obj_t * qp_tp = qp_object_new();
    qp_obj_t * qp_len = qp_object_new();
    qp_obj_t * qp_points = qp_object_new();

    if (    qp_name == NULL ||
            qp_tp == NULL ||
            qp_len == NULL ||
            qp_points == NULL)
    {
        qp_object_free_safe(qp_name);
        qp_object_free_safe(qp_tp);
        qp_object_free_safe(qp_len);
        qp_object_free_safe(qp_points);
        return;  /* critical error, signal is set */
    }

    for (size_t i = 0; i < promises->len; i++)
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
            pkg = promise->data;

            if (pkg == NULL || pkg->tp != BPROTO_RES_QUERY)
            {
                err_count++;
                snprintf(query->err_msg,
                        SIRIDB_MAX_SIZE_ERR_MSG,
                        "Error occurred while sending the database change to "
                        "at least '%s'", promise->server->name);
            }
            else
            {
                unpacker = qp_unpacker_new(pkg->data, pkg->len);

                if (unpacker != NULL)
                {

                    if (    qp_is_map(qp_next(unpacker, NULL)) &&
                            qp_is_raw(qp_next(unpacker, NULL)) && // select
                            qp_is_map(qp_next(unpacker, NULL)))
                    {
                        if (q_select->merge_as == NULL)
                        {
                            on_select_unpack_points(
                                    unpacker,
                                    q_select,
                                    qp_name,
                                    qp_tp,
                                    qp_len,
                                    qp_points);
                        }
                        else
                        {
                            on_select_unpack_merged_points(
                                    unpacker,
                                    q_select,
                                    qp_name,
                                    qp_tp,
                                    qp_len,
                                    qp_points);
                        }


                        /* extract time-it info if needed */
                        if (query->timeit != NULL)
                        {
                            siridb_query_timeit_from_unpacker(query, unpacker);
                        }

                    }
                    /* free the unpacker */
                    qp_unpacker_free(unpacker);
                }
            }

            /* make sure we free the promise and data */
            free(promise->data);
            free(promise);
        }
    }

    qp_object_free(qp_name);
    qp_object_free(qp_tp);
    qp_object_free(qp_len);
    qp_object_free(qp_points);

    if (q_select->n > MAX_SELECT_POINTS)
    {
        snprintf(query->err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Query has reached the maximum number of selected points "
                "(%u). Please use another time window, an aggregation "
                "function or select less series to reduce the number of "
                "points.",
                MAX_SELECT_POINTS);
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else if (err_count)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        uv_work_t * work = (uv_work_t *) malloc(sizeof(uv_work_t));
        if (work == NULL)
        {
            ERR_ALLOC
        }
        else
        {
            siri_async_incref(handle);
            work->data = handle;
            uv_queue_work(
                        siri.loop,
                        work,
                        &master_select_work,
                        &master_select_work_finish);
        }
    }
}

/*
 * Call-back function: sirinet_promises_cb
 *
 * Make sure to run siri_async_incref() on the handle
 */
static void on_update_xxx_response(slist_t * promises, uv_async_t * handle)
{
    ON_PROMISES

    sirinet_pkg_t * pkg;
    sirinet_promise_t * promise;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    size_t err_count = 0;

    for (size_t i = 0; i < promises->len; i++)
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
            pkg = promise->data;

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
            free(promise);
        }
    }

    if (err_count)
    {
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
    else
    {
        SIRIPARSER_NEXT_NODE
    }
}

/******************************************************************************
 * Helper functions
 *****************************************************************************/

static void master_select_work(uv_work_t * work)
{
    uv_async_t * handle = (uv_async_t *) work->data;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;
    int rc = ct_items(
            q_select->result,
            (q_select->merge_as == NULL) ?
                    (ct_item_cb) &items_select_master
                    :
                    (ct_item_cb) &items_select_master_merge,
            handle);

    /*
     * We need to check for SiriDB errors because this task is running in
     * another thread. In case a siri_err is set, this means we are in forced
     * closing state and we should not use the handle but let siri close it.
     */
    if (!siri_err)
    {
        switch (rc)
        {
        case -1:
            sprintf(query->err_msg, "Memory allocation error.");
            /* no break */
        case 1:
            siridb_query_send_error((uv_async_t *) handle, CPROTO_ERR_QUERY);
            return;
        }

        SIRIPARSER_NEXT_NODE
    }
}

static void master_select_work_finish(uv_work_t * work, int status)
{
    if (status)
    {
        log_error("Select work failed (error: %s)",
                uv_strerror(status));
    }

    siri_async_decref((uv_async_t **) &work->data);

    free(work);
}

static int items_select_master(
        const char * name,
        siridb_points_t * points,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (    qp_add_string(query->packer, name) ||
            siridb_points_pack(points, query->packer))
    {
        sprintf(query->err_msg, "Memory allocation error.");
        return -1;
    }

    return 0;
}

static int items_select_master_merge(
        const char * name,
        slist_t * plist,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query_select_t * q_select = (query_select_t *) query->data;
    siridb_points_t * points;

    if (qp_add_string(query->packer, name))
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
        points = slist_pop(plist);
        break;
    default:
        points = siridb_points_merge(
                                plist,
                                query->err_msg);
        break;
    }

    if (q_select->mlist != NULL && points != NULL)
    {
        siridb_points_t * aggr_points;
        for (size_t i = 0; points->len && i < q_select->mlist->len; i++)
        {
            aggr_points = siridb_aggregate_run(
                    points,
                    (siridb_aggr_t *) q_select->mlist->data[i],
                    query->err_msg);

            siridb_points_free(points);

            if (aggr_points == NULL)
            {
                return -1;  // (error message is set)
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

    if (siridb_points_pack(points, query->packer))
    {
        sprintf(query->err_msg, "Memory allocation error.");

        siridb_points_free(points);
        return -1;
    }

    siridb_points_free(points);

    return 0;
}

int items_select_other(
        const char * name,
        siridb_points_t * points,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    return (
            qp_add_string_term(query->packer, name) ||
            siridb_points_raw_pack(points, query->packer));
}

int items_select_other_merge(
        const char * name,
        slist_t * plist,
        uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    int rc;

    rc = qp_add_string_term(query->packer, name);

    rc += qp_add_type(query->packer, QP_ARRAY_OPEN);

    for (size_t i = 0; i < plist->len; i++)
    {
        rc += siridb_points_raw_pack(
                (siridb_points_t * ) plist->data[i],
                query->packer);
    }
    rc += qp_add_type(query->packer, QP_ARRAY_CLOSE);
    return rc;
}

static void on_select_unpack_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points)
{
    siridb_points_t * points;

    while ( q_select->n <= MAX_SELECT_POINTS &&
            qp_is_raw(qp_next(unpacker, qp_name)) &&
#ifdef DEBUG
            qp_is_raw_term(qp_name) &&
#endif
            qp_is_array(qp_next(unpacker, NULL)) &&
            qp_is_int(qp_next(unpacker, qp_tp)) &&
            qp_is_int(qp_next(unpacker, qp_len)) &&
            qp_is_raw(qp_next(unpacker, qp_points)))
    {
        points = siridb_points_new(qp_len->via->int64, qp_tp->via->int64);

        if (points != NULL)
        {
#ifdef DEBUG
            assert (qp_len->via->int64 * sizeof(siridb_point_t) ==
                    qp_points->len);
#endif
            points->len = qp_len->via->int64;

            memcpy(points->data, qp_points->via->raw, qp_points->len);

            if (ct_add(q_select->result, qp_name->via->raw, points))
            {
                siridb_points_free(points);
            }
            else
            {
                q_select->n += points->len;
            }
        }

        qp_next(unpacker, NULL);  // QP_ARRAY_CLOSE
    }
}

static void on_select_unpack_merged_points(
        qp_unpacker_t * unpacker,
        query_select_t * q_select,
        qp_obj_t * qp_name,
        qp_obj_t * qp_tp,
        qp_obj_t * qp_len,
        qp_obj_t * qp_points)
{
    siridb_points_t * points;

    while ( qp_is_raw(qp_next(unpacker, qp_name)) &&
#ifdef DEBUG
            qp_is_raw_term(qp_name) &&
#endif
            qp_is_array(qp_next(unpacker, NULL)))

    {
        slist_t ** plist = (slist_t **) ct_getaddr(
                q_select->result,
                qp_name->via->raw);

        while ( q_select->n <= MAX_SELECT_POINTS &&
                qp_is_array(qp_next(unpacker, NULL)) &&
                qp_is_int(qp_next(unpacker, qp_tp)) &&
                qp_is_int(qp_next(unpacker, qp_len)) &&
                qp_is_raw(qp_next(unpacker, qp_points)))
        {

            points = siridb_points_new(qp_len->via->int64, qp_tp->via->int64);

            if (points != NULL)
            {
    #ifdef DEBUG
                assert (qp_len->via->int64 * sizeof(siridb_point_t) ==
                        qp_points->len);
    #endif
                points->len = qp_len->via->int64;

                memcpy(points->data, qp_points->via->raw, qp_points->len);


                if (slist_append_safe(plist, points))
                {
                    siridb_points_free(points);
                }
                else
                {
                    q_select->n += points->len;
                }
            }

            qp_next(unpacker, NULL);  // QP_ARRAY_CLOSE
        }
    }
}
