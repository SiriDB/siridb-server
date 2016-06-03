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
#include <siri/parser/listener.h>
#include <siri/parser/walkers.h>
#include <siri/parser/queries.h>
#include <logger/logger.h>
#include <siri/siri.h>
#include <siri/db/node.h>
#include <siri/db/query.h>
#include <siri/db/series.h>
#include <siri/db/props.h>
#include <siri/db/shard.h>
#include <siri/net/handle.h>
#include <siri/net/protocol.h>
#include <inttypes.h>
#include <sys/time.h>
#include <qpack/qpack.h>
#include <siri/db/user.h>
#include <siri/db/users.h>
#include <strextra/strextra.h>
#include <assert.h>
#include <math.h>

#define QP_ADD_SUCCESS qp_add_raw(query->packer, "success_msg", 11);

static void free_user_object(uv_handle_t * handle);

static void enter_access_expr(uv_async_t * handle);
static void enter_alter_user_stmt(uv_async_t * handle);
static void enter_count_stmt(uv_async_t * handle);
static void enter_create_user_stmt(uv_async_t * handle);
static void enter_drop_stmt(uv_async_t * handle);
static void enter_grant_stmt(uv_async_t * handle);
static void enter_grant_user_stmt(uv_async_t * handle);
static void enter_list_stmt(uv_async_t * handle);
static void enter_revoke_stmt(uv_async_t * handle);
static void enter_revoke_user_stmt(uv_async_t * handle);
static void enter_select_stmt(uv_async_t * handle);
static void enter_set_password_expr(uv_async_t * handle);
static void enter_series_name(uv_async_t * handle);
static void enter_series_match(uv_async_t * handle);
static void enter_timeit_stmt(uv_async_t * handle);
static void enter_where_xxx_stmt(uv_async_t * handle);
static void enter_xxx_columns(uv_async_t * handle);

static void exit_after_expr(uv_async_t * handle);
static void exit_alter_user_stmt(uv_async_t * handle);
static void exit_before_expr(uv_async_t * handle);
static void exit_between_expr(uv_async_t * handle);
static void exit_calc_stmt(uv_async_t * handle);
static void exit_count_series_stmt(uv_async_t * handle);
static void exit_create_user_stmt(uv_async_t * handle);
static void exit_drop_series_stmt(uv_async_t * handle);
static void exit_drop_shard_stmt(uv_async_t * handle);
static void exit_drop_user_stmt(uv_async_t * handle);
static void exit_grant_user_stmt(uv_async_t * handle);
static void exit_list_users_stmt(uv_async_t * handle);
static void exit_revoke_user_stmt(uv_async_t * handle);
static void exit_select_stmt(uv_async_t * handle);
static void exit_show_stmt(uv_async_t * handle);
static void exit_timeit_stmt(uv_async_t * handle);

#define SIRIPARSER_NEXT_NODE                                                \
siridb_node_next(&query->node_list);                                        \
if (query->node_list == NULL)                                               \
    siridb_send_query_result(handle);                                       \
else                                                                        \
{                                                                           \
    uv_async_t * forward = (uv_async_t *) malloc(sizeof(uv_async_t));       \
    forward->data = (void *) handle->data;                                  \
    uv_async_init(siri.loop, forward, (uv_async_cb) query->node_list->cb);  \
    uv_async_send(forward);                                                 \
    uv_close((uv_handle_t *) handle, (uv_close_cb) sirinet_free_async);     \
}

#define SIRIPARSER_MASTER_CHECK_ACCESS(ACCESS_BIT)                          \
if (    (query->flags & SIRIDB_QUERY_FLAG_MASTER) &&                        \
        !siridb_user_check_access(                                          \
            ((sirinet_handle_t *) query->client->data)->user,               \
            ACCESS_BIT,                                                     \
            query->err_msg))                                                \
    return siridb_send_error(handle, SN_MSG_QUERY_ERROR);


void siriparser_init_listener(void)
{
    for (uint_fast16_t i = 0; i < CLERI_END; i++)
    {
        siriparser_listen_enter[i] = NULL;
        siriparser_listen_exit[i] = NULL;
    }

    siriparser_listen_enter[CLERI_GID_ACCESS_EXPR] = enter_access_expr;
    siriparser_listen_enter[CLERI_GID_ALTER_USER_STMT] = enter_alter_user_stmt;
    siriparser_listen_enter[CLERI_GID_COUNT_STMT] = enter_count_stmt;
    siriparser_listen_enter[CLERI_GID_CREATE_USER_STMT] = enter_create_user_stmt;
    siriparser_listen_enter[CLERI_GID_DROP_STMT] = enter_drop_stmt;
    siriparser_listen_enter[CLERI_GID_GRANT_STMT] = enter_grant_stmt;
    siriparser_listen_enter[CLERI_GID_GRANT_USER_STMT] = enter_grant_user_stmt;
    siriparser_listen_enter[CLERI_GID_LIST_STMT] = enter_list_stmt;
    siriparser_listen_enter[CLERI_GID_REVOKE_STMT] = enter_revoke_stmt;
    siriparser_listen_enter[CLERI_GID_REVOKE_USER_STMT] = enter_revoke_user_stmt;
    siriparser_listen_enter[CLERI_GID_SELECT_STMT] = enter_select_stmt;
    siriparser_listen_enter[CLERI_GID_SET_PASSWORD_EXPR] = enter_set_password_expr;
    siriparser_listen_enter[CLERI_GID_SERIES_NAME] = enter_series_name;
    siriparser_listen_enter[CLERI_GID_SERIES_MATCH] = enter_series_match;
    siriparser_listen_enter[CLERI_GID_TIMEIT_STMT] = enter_timeit_stmt;
    siriparser_listen_enter[CLERI_GID_USER_COLUMNS] = enter_xxx_columns;
    siriparser_listen_enter[CLERI_GID_WHERE_USER_STMT] = enter_where_xxx_stmt;


    siriparser_listen_exit[CLERI_GID_AFTER_EXPR] = exit_after_expr;
    siriparser_listen_exit[CLERI_GID_ALTER_USER_STMT] = exit_alter_user_stmt;
    siriparser_listen_exit[CLERI_GID_BEFORE_EXPR] = exit_before_expr;
    siriparser_listen_exit[CLERI_GID_BETWEEN_EXPR] = exit_between_expr;
    siriparser_listen_exit[CLERI_GID_CALC_STMT] = exit_calc_stmt;
    siriparser_listen_exit[CLERI_GID_COUNT_SERIES_STMT] = exit_count_series_stmt;
    siriparser_listen_exit[CLERI_GID_CREATE_USER_STMT] = exit_create_user_stmt;
    siriparser_listen_exit[CLERI_GID_DROP_SERIES_STMT] = exit_drop_series_stmt;
    siriparser_listen_exit[CLERI_GID_DROP_SHARD_STMT] = exit_drop_shard_stmt;
    siriparser_listen_exit[CLERI_GID_DROP_USER_STMT] = exit_drop_user_stmt;
    siriparser_listen_exit[CLERI_GID_GRANT_USER_STMT] = exit_grant_user_stmt;
    siriparser_listen_exit[CLERI_GID_LIST_USERS_STMT] = exit_list_users_stmt;
    siriparser_listen_exit[CLERI_GID_REVOKE_USER_STMT] = exit_revoke_user_stmt;
    siriparser_listen_exit[CLERI_GID_SELECT_STMT] = exit_select_stmt;
    siriparser_listen_exit[CLERI_GID_SHOW_STMT] = exit_show_stmt;
    siriparser_listen_exit[CLERI_GID_TIMEIT_STMT] = exit_timeit_stmt;
}

/******************************************************************************
 * Free functions
 *****************************************************************************/

static void free_user_object(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_user_free((siridb_user_t *) query->data);

    /* normal free call */
    siridb_free_query(handle);
}

/******************************************************************************
 * Enter functions
 *****************************************************************************/

static void enter_access_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* bind ACCESS_EXPR children to query */
    query->data = query->node_list->node->children;

    SIRIPARSER_NEXT_NODE
}

static void enter_alter_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_ALTER)

    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    cleri_node_t * user_node =
                query->node_list->node->children->next->node;

    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((query->data = siridb_users_get_user(siridb, username, NULL)) == NULL)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'", username);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_count_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_COUNT)

    query->packer = qp_new_packer(256);
    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_count_new();
    query->free_cb = (uv_close_cb) query_count_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_create_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_CREATE)

    /* bind user object to data and set correct free call */
    query->data = siridb_user_new();
    query->free_cb = (uv_close_cb) free_user_object;

    SIRIPARSER_NEXT_NODE
}

static void enter_drop_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->packer = qp_new_packer(1024);
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

static void enter_grant_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    cleri_node_t * user_node =
                query->node_list->node->children->next->node;
    siridb_user_t * user;
    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb, username, NULL)) == NULL)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'", username);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    user->access_bit |=
            siridb_access_from_children((cleri_children_t *) query->data);

    query->data = user;

    SIRIPARSER_NEXT_NODE
}

static void enter_list_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_LIST)

    query->packer = qp_new_packer(QP_SUGGESTED_SIZE);
    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = query_list_new();
    query->free_cb = (uv_close_cb) query_list_free;

    SIRIPARSER_NEXT_NODE
}

static void enter_revoke_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_REVOKE)
    SIRIPARSER_NEXT_NODE
}

static void enter_revoke_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    cleri_node_t * user_node =
                query->node_list->node->children->next->node;
    siridb_user_t * user;
    char username[user_node->len - 1];
    strx_extract_string(username, user_node->str, user_node->len);

    if ((user = siridb_users_get_user(siridb, username, NULL)) == NULL)
    {
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find user: '%s'", username);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    user->access_bit ^= (
            user->access_bit &
            siridb_access_from_children((cleri_children_t *) query->data));
    log_debug("New access: %d", user->access_bit);

    query->data = user;

    SIRIPARSER_NEXT_NODE
}

static void enter_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_SELECT)

    query->data = query_select_new();
    query->free_cb = (uv_close_cb) query_select_free;

    query->packer = qp_new_packer(QP_SUGGESTED_SIZE);
    qp_add_type(query->packer, QP_MAP_OPEN);

    SIRIPARSER_NEXT_NODE
}

static void enter_set_password_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_user_t * user = (siridb_user_t *) query->data;
    cleri_node_t * pw_node =
            query->node_list->node->children->next->next->node;

    char password[pw_node->len - 1];
    strx_extract_string(password, pw_node->str, pw_node->len);

    if (siridb_user_set_password(user, password, query->err_msg))
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);

    SIRIPARSER_NEXT_NODE
}

static void enter_series_name(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    cleri_node_t * node = query->node_list->node;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    siridb_series_t * series;
    uint16_t pool;
    char series_name[node->len - 1];

    /* extract series name */
    strx_extract_string(series_name, node->str, node->len);

    /* get pool for series name */
    pool = siridb_pool_sn(siridb, series_name);

    /* check if this series belongs to 'this' pool and if so get the series */
    if (pool == siridb->server->pool)
    {
        if ((series = ct_get(siridb->series, series_name)) == NULL)
        {
            /* the series does not exist */
            snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                    "Cannot find series: '%s'", series_name);

            /* free series_name and return with send_errror.. */
            return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
        }

        /* bind the series to the query, increment ref count if successful */
        if (ct_add(((query_wrapper_ct_series_t *) query->data)->ct_series,
                series_name,
                series) == CT_OK)
        {
            siridb_series_incref(series);
        }
    }

    SIRIPARSER_NEXT_NODE
}

static void enter_series_match(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_wrapper_ct_series_t *) query->data)->ct_series = ct_new();

    SIRIPARSER_NEXT_NODE
}

static void enter_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query->timeit = qp_new_packer(512);

    qp_add_raw(query->timeit, "__timeit__", 10);
    qp_add_type(query->timeit, QP_ARRAY_OPEN);

    SIRIPARSER_NEXT_NODE
}

static void enter_where_xxx_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_wrapper_where_node_t *) query->data)->where_node =
            query->node_list->node->children->next->next->node;

    SIRIPARSER_NEXT_NODE
}

static void enter_xxx_columns(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_wrapper_columns_t *) query->data)->columms =
            query->node_list->node->children;

    SIRIPARSER_NEXT_NODE
}

/******************************************************************************
 * Exit functions
 *****************************************************************************/

static void exit_after_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    ((query_select_t *) query->data)->start_ts =
            (uint64_t *) &query->node_list->node->children->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_alter_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_handle_t *) query->client->data)->siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    query->packer = qp_new_packer(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "Successful changed password for user '%s'.",
            ((siridb_user_t *) query->data)->username);

    SIRIPARSER_NEXT_NODE
}

static void exit_before_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_select_t *) query->data)->end_ts =
            (uint64_t *) &query->node_list->node->children->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_between_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ((query_select_t *) query->data)->start_ts = (uint64_t *)
            &query->node_list->node->children->next->node->result;

    ((query_select_t *) query->data)->end_ts = (uint64_t *)
            &query->node_list->node->children->next->next->next->node->result;

    SIRIPARSER_NEXT_NODE
}

static void exit_calc_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    cleri_node_t * calc_node = query->node_list->node->children->node;

    query->packer = qp_new_packer(64);
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

static void exit_count_series_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;


//    ((query_count_t *) query->data)->
    qp_add_raw(query->packer, "series", 6);
    qp_add_int64(query->packer, siridb->series_map->len);

    SIRIPARSER_NEXT_NODE
}

static void exit_create_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_user_t * user = (siridb_user_t *) query->data;
    cleri_node_t * user_node =
            query->node_list->node->children->next->node;

    assert(user->username == NULL);
    user->username = (char *) malloc(user_node->len - 1);
    strx_extract_string(user->username, user_node->str, user_node->len);

    if (siridb_users_add_user(
            ((sirinet_handle_t *) query->client->data)->siridb,
            user,
            query->err_msg))
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);

    /* success, we do not need to free the user anymore */
    query->free_cb = (uv_close_cb) siridb_free_query;

    assert(query->packer == NULL);
    query->packer = qp_new_packer(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "User '%s' is created successfully.", user->username);
    SIRIPARSER_NEXT_NODE
}

static void exit_drop_series_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;

    query_drop_t * q_drop = (query_drop_t *) query->data;

    uv_mutex_lock(&siridb->series_mutex);

    ct_walk(q_drop->ct_series,
            (ct_cb_t) &walk_drop_series, handle);

    uv_mutex_unlock(&siridb->series_mutex);

    SIRIPARSER_NEXT_NODE
}

static void exit_drop_shard_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    cleri_node_t * shard_id_node =
                query->node_list->node->children->next->node;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;

    int64_t shard_id = atoll(shard_id_node->str);

    uv_mutex_lock(&siridb->shards_mutex);

    siridb_shard_t * shard = imap64_pop(siridb->shards, shard_id);

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
        ((query_drop_t *) query->data)->data = shard;

        /* We need a series mutex here since we depend on the series index */
        uv_mutex_lock(&siridb->series_mutex);

        imap32_walk(
                siridb->series_map,
                (imap32_cb_t) walk_drop_shard,
                (void *) handle);

        uv_mutex_unlock(&siridb->series_mutex);

        shard->status |= SIRIDB_SHARD_WILL_BE_REMOVED;

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

static void exit_drop_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    cleri_node_t * user_node =
            query->node_list->node->children->next->node;
    char username[user_node->len - 1];

    /* we need to free user-name */
    strx_extract_string(username, user_node->str, user_node->len);

    if (siridb_users_drop_user(
            ((sirinet_handle_t *) query->client->data)->siridb,
            username,
            query->err_msg))
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "User '%s' is dropped successfully.", username);

    SIRIPARSER_NEXT_NODE
}

static void exit_grant_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_handle_t *) query->client->data)->siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    assert (query->packer == NULL);

    query->packer = qp_new_packer(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "Successfully granted permissions to user '%s'.",
            ((siridb_user_t *) query->data)->username);

    SIRIPARSER_NEXT_NODE
}

static void exit_list_users_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_users_t * users =
            ((sirinet_handle_t *) query->client->data)->siridb->users;

    cleri_children_t * columns;

    qp_add_raw(query->packer, "columns", 7);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    if ((columns = ((query_list_t *) query->data)->columms) == NULL)
    {
        qp_add_raw(query->packer, "user", 4);
        qp_add_raw(query->packer, "access", 6);
    }
    else
    {
        while (1)
        {
            qp_add_raw(query->packer, columns->node->str, columns->node->len);
            if (columns->next == NULL)
                break;
            columns = columns->next->next;
        }
    }
    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    qp_add_raw(query->packer, "users", 5);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    if (users->user != NULL) while (users != NULL)
    {
        qp_add_type(query->packer, QP_ARRAY_OPEN);

        /* reset columns */
        if ((columns = ((query_list_t *) query->data)->columms) == NULL)
        {
            siridb_user_prop(users->user, query->packer, CLERI_GID_K_USER);
            siridb_user_prop(users->user, query->packer, CLERI_GID_K_ACCESS);
        }
        else
        {
            while (1)
            {
                siridb_user_prop(
                        users->user,
                        query->packer,
                        columns->node->children->node->cl_obj->cl_obj->dummy->gid);
                if (columns->next == NULL)
                    break;
                columns = columns->next->next;
            }
        }

        qp_add_type(query->packer, QP_ARRAY_CLOSE);
        users = users->next;
    }

    qp_add_type(query->packer, QP_ARRAY_CLOSE);

    SIRIPARSER_NEXT_NODE
}

static void exit_revoke_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    if (siridb_users_save(((sirinet_handle_t *) query->client->data)->siridb))
    {
        sprintf(query->err_msg, "Could not write users to file!");
        log_critical(query->err_msg);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    assert (query->packer == NULL);

    query->packer = qp_new_packer(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "Successfully revoked permissions from user '%s'.",
            ((siridb_user_t *) query->data)->username);

    SIRIPARSER_NEXT_NODE
}

static void exit_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;

    uv_mutex_lock(&siridb->series_mutex);

    ct_walk(((query_select_t *) query->data)->ct_series,
            (ct_cb_t) &walk_select, handle);

    uv_mutex_unlock(&siridb->series_mutex);

    SIRIPARSER_NEXT_NODE
}

static void exit_show_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    SIRIPARSER_MASTER_CHECK_ACCESS(SIRIDB_ACCESS_SHOW)

    cleri_children_t * children =
            query->node_list->node->children->next->node->children;
    siridb_props_cb prop_cb;

    query->packer = qp_new_packer(4096);
    qp_add_type(query->packer, QP_MAP_OPEN);
    qp_add_raw(query->packer, "data", 4);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    who_am_i = ((sirinet_handle_t *) query->client->data)->user->username;

    if (children->node == NULL)
    {
        /* show all properties */
        int i;

        for (i = 0; i < KW_COUNT; i++)
        {
            if ((prop_cb = siridb_props[i]) == NULL)
                continue;
            prop_cb(((sirinet_handle_t *) query->client->data)->siridb,
                    query->packer, 1);
        }
    }
    else
    {
        /* show selected properties chosen by query */
        while (1)
        {
            /* get the callback */
            if ((prop_cb = siridb_props[children->node->children->node->
                                        cl_obj->cl_obj->keyword->
                                        gid - KW_OFFSET]) == NULL)
                log_debug("not implemented");
            else
                prop_cb(((sirinet_handle_t *) query->client->data)->siridb,
                        query->packer, 1);

            if (children->next == NULL)
                break;

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
            ((sirinet_handle_t *) query->client->data)->siridb->server->name);
    qp_add_raw(query->timeit, "time", 4);
    qp_add_double(query->timeit,
            (double) (end.tv_sec - query->start.tv_sec) +
            (double) (end.tv_nsec - query->start.tv_nsec) / 1000000000.0f);

    if (query->packer == NULL)
    {
        /* lets give the new packer the exact size so we do not
         * need a realloc */
        query->packer = qp_new_packer(query->timeit->len + 1);
        qp_add_type(query->packer, QP_MAP_OPEN);
    }

    /* extend packer with timeit information */
    qp_extend_packer(query->packer, query->timeit);

    SIRIPARSER_NEXT_NODE
}
