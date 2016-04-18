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
#include <siri/db/listener.h>
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
#include <strextra/strextra.h>
#include <assert.h>

#define QP_ADD_SUCCESS qp_add_raw(query->packer, "success_msg", 11);

static void free_select_query(uv_handle_t * handle);
static void free_user_object(uv_handle_t * handle);

static void enter_create_user_stmt(uv_async_t * handle);
static void enter_list_users_stmt(uv_async_t * handle);
static void enter_select_stmt(uv_async_t * handle);
static void enter_set_password_expr(uv_async_t * handle);
static void enter_series_name(uv_async_t * handle);
static void enter_timeit_stmt(uv_async_t * handle);
static void enter_user_columns(uv_async_t * handle);

static void exit_create_user_stmt(uv_async_t * handle);
static void exit_drop_user_stmt(uv_async_t * handle);
static void exit_list_users_stmt(uv_async_t * handle);
static void exit_select_stmt(uv_async_t * handle);
static void exit_show_stmt(uv_async_t * handle);
static void exit_timeit_stmt(uv_async_t * handle);

#define SIRIDB_NEXT_NODE                                                    \
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

void siridb_init_listener(void)
{
    uint_fast16_t i;
    for (i = 0; i < CLERI_END; i++)
    {
        siridb_listen_enter[i] = NULL;
        siridb_listen_exit[i] = NULL;
    }

    siridb_listen_enter[CLERI_GID_CREATE_USER_STMT] = enter_create_user_stmt;
    siridb_listen_enter[CLERI_GID_LIST_USERS_STMT] = enter_list_users_stmt;
    siridb_listen_enter[CLERI_GID_SELECT_STMT] = enter_select_stmt;
    siridb_listen_enter[CLERI_GID_SET_PASSWORD_EXPR] = enter_set_password_expr;
    siridb_listen_enter[CLERI_GID_SERIES_NAME] = enter_series_name;
    siridb_listen_enter[CLERI_GID_TIMEIT_STMT] = enter_timeit_stmt;
    siridb_listen_enter[CLERI_GID_USER_COLUMNS] = enter_user_columns;

    siridb_listen_exit[CLERI_GID_CREATE_USER_STMT] = exit_create_user_stmt;
    siridb_listen_exit[CLERI_GID_DROP_USER_STMT] = exit_drop_user_stmt;
    siridb_listen_exit[CLERI_GID_LIST_USERS_STMT] = exit_list_users_stmt;
    siridb_listen_exit[CLERI_GID_SELECT_STMT] = exit_select_stmt;
    siridb_listen_exit[CLERI_GID_SHOW_STMT] = exit_show_stmt;
    siridb_listen_exit[CLERI_GID_TIMEIT_STMT] = exit_timeit_stmt;
}

static void free_select_query(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_free_select_query((siridb_q_select_t *) query->data);

    /* normal free call */
    siridb_free_query(handle);
}

static void free_user_object(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_free_user((siridb_user_t *) query->data);

    /* normal free call */
    siridb_free_query(handle);
}

static void enter_create_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* bind user object to data */
    query->data = (siridb_user_t *) siridb_new_user();

    /* when binding data, we must set a specific free call */
    query->free_cb = (uv_close_cb) free_user_object;

    SIRIDB_NEXT_NODE
}

static void enter_list_users_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->packer = qp_new_packer(QP_SUGGESTED_SIZE);
    qp_add_type(query->packer, QP_MAP_OPEN);

    query->data = NULL;

    SIRIDB_NEXT_NODE
}

static void enter_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->data = (siridb_q_select_t *) siridb_new_select_query();
    query->free_cb = (uv_close_cb) free_select_query;

    query->packer = qp_new_packer(QP_SUGGESTED_SIZE);
    qp_add_type(query->packer, QP_MAP_OPEN);

    SIRIDB_NEXT_NODE
}

static void enter_set_password_expr(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_user_t * user = (siridb_user_t *) query->data;
    cleri_node_t * pw_node =
            query->node_list->node->children->next->next->node;
    assert(user->password == NULL);
    extract_string(&user->password, pw_node->str, pw_node->len);

    SIRIDB_NEXT_NODE
}

static void enter_series_name(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    cleri_node_t * node = query->node_list->node;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    siridb_series_t * series;
    uint16_t pool;
    char * series_name;

    /* extract series name */
    extract_string(&series_name, node->str, node->len);

    /* get pool for series name */
    pool = siridb_pool_sn(siridb, series_name);

    /* check if this series belongs to 'this' pool and if so get the series */
    if (pool == siridb->server->pool &&
            (series = ct_get(siridb->series, series_name)) == NULL)
    {
        /* the series does not exist */
        snprintf(query->err_msg, SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot find series: '%s'", series_name);

        /* free series_name and return with send_errror.. */
        free(series_name);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    /* bind the series to the query and ignore CT_EXISTS */
    ct_add(((siridb_q_series_t *) query->data)->ct_series, series_name, series);

    /* free series name since we do not need the name anymore */
    free(series_name);

    SIRIDB_NEXT_NODE
}

static void enter_timeit_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    query->timeit = qp_new_packer(512);

    qp_add_raw(query->timeit, "__timeit__", 10);
    qp_add_type(query->timeit, QP_ARRAY_OPEN);

    SIRIDB_NEXT_NODE
}

static void enter_user_columns(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->data = (cleri_children_t *)
            query->node_list->node->children;

    SIRIDB_NEXT_NODE
}

static void exit_create_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_user_t * user = (siridb_user_t *) query->data;

    cleri_node_t * user_node =
            query->node_list->node->children->next->node;

    assert(user->username == NULL);
    extract_string(&user->username, user_node->str, user_node->len);

    if (siridb_add_user(
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
    SIRIDB_NEXT_NODE
}

static void exit_drop_user_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    char * username;
    cleri_node_t * user_node =
            query->node_list->node->children->next->node;

    /* we need to free user-name */
    extract_string(&username, user_node->str, user_node->len);

    if (siridb_drop_user(
            ((sirinet_handle_t *) query->client->data)->siridb,
            username,
            query->err_msg))
    {
        /* free user name */
        free(username);
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    assert(query->packer == NULL);
    query->packer = qp_new_packer(1024);
    qp_add_type(query->packer, QP_MAP_OPEN);

    QP_ADD_SUCCESS
    qp_add_fmt(query->packer,
            "User '%s' is dropped successfully.", username);

    /* free user name */
    free(username);

    SIRIDB_NEXT_NODE
}

static void exit_list_users_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    siridb_users_t * users =
            ((sirinet_handle_t *) query->client->data)->siridb->users;

    cleri_children_t * columns;

    qp_add_raw(query->packer, "columns", 7);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    if ((columns = (cleri_children_t *) query->data) == NULL)
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

        if ((columns = (cleri_children_t *) query->data) == NULL)
        {
            siridb_prop_user(users->user, query->packer, CLERI_GID_K_USER);
            siridb_prop_user(users->user, query->packer, CLERI_GID_K_ACCESS);
        }
        else
        {
            while (1)
            {
                siridb_prop_user(
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

    SIRIDB_NEXT_NODE
}

static void walk_series(
        const char * series_name,
        siridb_series_t * series,
        uv_async_t * handle)
{
    siridb_point_t * point;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    uint64_t start_ts = 1450000060;
//    uint64_t end_ts = 1450000080;

    qp_add_string(query->packer, series_name);
    qp_add_type(query->packer, QP_ARRAY_OPEN);

    siridb_points_t * points = siridb_series_get_points_num32(
            ((sirinet_handle_t *) query->client->data)->siridb,
            series,
            &start_ts,
            NULL);

    point = points->data;

    for (size_t i = 0; i < points->len; i++, point++)
    {
        qp_add_type(query->packer, QP_ARRAY2);
        qp_add_int64(query->packer, (int64_t) point->ts);
        qp_add_int64(query->packer, point->val.int64);
    }

    siridb_free_points(points);

    qp_add_type(query->packer, QP_ARRAY_CLOSE);
}

static void exit_select_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    ct_walk(((siridb_q_series_t *) query->data)->ct_series,
            (ct_cb_t) &walk_series, handle);

    SIRIDB_NEXT_NODE
}

static void exit_show_stmt(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
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

    SIRIDB_NEXT_NODE
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

    SIRIDB_NEXT_NODE
}
