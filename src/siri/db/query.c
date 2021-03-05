/*
 * query.c - Responsible for parsing queries.
 */
#include <assert.h>
#include <cleri/cleri.h>
#include <expr/expr.h>
#include <iso8601/iso8601.h>
#include <logger/logger.h>
#include <siri/async.h>
#include <siri/db/nodes.h>
#include <siri/db/query.h>
#include <siri/db/replicate.h>
#include <siri/db/servers.h>
#include <siri/db/time.h>
#include <siri/db/walker.h>
#include <siri/db/listener.h>
#include <siri/db/queries.h>
#include <siri/net/clserver.h>
#include <siri/net/pkg.h>
#include <siri/net/clserver.h>
#include <siri/siri.h>
#include <siri/grammar/gramp.h>
#include <xstr/xstr.h>
#include <string.h>
#include <sys/time.h>
#include <siri/err.h>

#if SIRIDB_EXPR_ALLOC
#include <llist/llist.h>
#endif


#define QUERY_TOO_LONG -1
#define QUERY_MAX_LENGTH 8192
#define QUERY_EXTRA_ALLOC_SIZE 200
#define SIRIDB_FWD_SERVERS_TIMEOUT 5000  /* 5 seconds  */

static void QUERY_send_invalid_error(uv_async_t * handle);
static void QUERY_parse(uv_async_t * handle);
static int QUERY_walk(
#if SIRIDB_EXPR_ALLOC
        siridb_query_t * query,
#endif
        cleri_node_t * node,
        siridb_walker_t * walker);
static int QUERY_to_packer(qp_packer_t * packer, siridb_query_t * query);
static int QUERY_time_expr(
        cleri_node_t * node,
        siridb_walker_t * walker,
        char * buf,
        size_t * size);
static int QUERY_int_expr(cleri_node_t * node, char * buf, size_t * size);
static int QUERY_rebuild(
        siridb_t * siridb,
        cleri_node_t * node,
        char * buf,
        size_t * size,
        const size_t max_size);
static void QUERY_send_no_query(uv_async_t * handle);

/*
 * This function can raise a SIGNAL.
 */
void siridb_query_run(
        uint16_t pid,
        sirinet_stream_t * client,
        const char * q,
        size_t q_len,
        float factor,
        int flags)
{
    uv_async_t * handle = malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        ERR_ALLOC
        return;
    }
    siridb_query_t * query = malloc(sizeof(siridb_query_t));
    if (query == NULL)
    {
        ERR_ALLOC
        free(handle);
        return;
    }

    /* set query */
    if ((query->q = strndup(q, q_len)) == NULL)
    {
        ERR_ALLOC
        free(query);
        free(handle);
        return;
    }

    #if SIRIDB_EXPR_ALLOC
    if ((query->expr_cache = llist_new()) == NULL)
    {
        ERR_ALLOC
        free(query->q);
        free(query);
        free(handle);
        return;
    }
    #endif

    /*
     * Set start time.
     * (must be real time since we translate now with this value)
     */
    clock_gettime(CLOCK_REALTIME, &query->start);

    /* bind pid, client and flags so we can send back the result */
    query->pid = pid;

    /* increment client reference counter */
    sirinet_stream_incref(client);

    query->client = client;
    query->flags = flags;

    /* bind time precision factor */
    query->factor = factor;

    /* set the default callback, this might change when custom
     * data is linked to the query handle
     */
    query->free_cb = siridb_query_free;
    query->ref = 1;

    /* We should initialize the packer based on query type */
    query->packer = NULL;
    query->timeit = NULL;

    /* make sure all *other* pointers are set to NULL */
    query->data = NULL;
    query->pr = NULL;
    query->nodes = NULL;

    if (Logger.level == LOGGER_DEBUG && strstr(query->q, "password") == NULL)
    {
        log_debug("Parsing query (%d): %s", query->flags, query->q);
    }

    /* increment active tasks */
    siridb_tasks_inc(client->siridb->tasks);

    /* send next call */
    uv_async_init(siri.loop, handle, (uv_async_cb) QUERY_parse);
    handle->data = query;
    uv_async_send(handle);
}

void siridb_query_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = query->client->siridb;

    /* decrement active tasks */
    siridb_tasks_dec(siridb->tasks);

    /* free query */
    free(query->q);

    /* free qpack buffers */
    if (query->packer != NULL)
    {
        qp_packer_free(query->packer);
    }

    if (query->timeit != NULL)
    {
        qp_packer_free(query->timeit);
    }

    /* free node list */
    siridb_nodes_free(query->nodes);

    /* free query result */
    if (query->pr != NULL)
    {
        cleri_parse_free(query->pr);
    }

    #if SIRIDB_EXPR_ALLOC
    if (query->expr_cache != NULL)
    {
        llist_destroy(query->expr_cache, (llist_destroy_cb) free);
    }
    #endif

    /* decrement client reference counter */
    sirinet_stream_decref(query->client);

    /* free query */
    free(query);

    /* free handle */
    free(handle);
}

void siridb_send_query_result(uv_async_t * handle)
{
    /*
     * Its important to have another callback to free stuff so we can
     * clean everything without sending things in case of a client failure
     */
    siridb_query_t * query = (siridb_query_t *) handle->data;

    assert (query->packer != NULL);

    sirinet_pkg_t * pkg = sirinet_packer2pkg(
            query->packer,
            query->pid,
            CPROTO_RES_QUERY);

    sirinet_pkg_send(query->client, pkg);

    query->packer = NULL;

    uv_close((uv_handle_t *) handle, siri_async_close);
}

/*
 * Signal can be raised by this function.
 */
void siridb_query_send_error(
        uv_async_t * handle,
        cproto_server_t err)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* make sure the error message is null terminated in case the length has
     * exceeded the max length.
     */
    query->err_msg[SIRIDB_MAX_SIZE_ERR_MSG-1] = '\0';

    sirinet_pkg_t * package = sirinet_pkg_err(
                query->pid,
                strlen(query->err_msg),
                err,  /* usually this is CPROTO_ERR_QUERY, CPROTO_ERR_POOL...*/
                query->err_msg);

    log_warning("(%s) %s", sirinet_cproto_server_str(err), query->err_msg);

    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send(query->client, package);
    }
    uv_close((uv_handle_t *) handle, siri_async_close);
}

/*
 * This function can raise a SIGNAL.
 *
 * Reference counter for handle will be incremented since we count the handle
 * to a timer object. The cb function should perform siri_async_decref(&handle).
 *
 * (see siridb_query_fwd_t definition for info about fwd_t and flags)
 */
void siridb_query_forward(
        uv_async_t * handle,
        siridb_query_fwd_t fwd,
        sirinet_promises_cb cb,
        int flags)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = query->client->siridb;

    /*
     * the size is important here, we will use the alloc_size to guess the
     * maximum query size in QUERY_to_packer.
     */
    qp_packer_t * packer =
            qp_packer_new(query->pr->tree->len + QUERY_EXTRA_ALLOC_SIZE);

    if (packer == NULL)
    {
        ERR_ALLOC
        return;
    }

    /*
     * For backwards compatibility with SiriDB version < 2.0.24 we send an
     * extra value SIRIDB_TIME_DEFAULT.
     */
    qp_add_type(packer, QP_ARRAY2);

    /* add the query to the packer */
    QUERY_to_packer(packer, query);
    qp_add_int64(packer, SIRIDB_TIME_DEFAULT);  /* Only for version < 2.0.24 */


    sirinet_pkg_t * pkg = sirinet_pkg_new(0, packer->len, 0, packer->buffer);

    /* increment reference since handle will be bound to a timer */
    siri_async_incref(handle);

    if (pkg != NULL)
    {
        switch (fwd)
        {
        case SIRIDB_QUERY_FWD_SERVERS:
            pkg->tp = BPROTO_QUERY_SERVER;
            {
                vec_t * servers = siridb_servers_other2vec(siridb);
                if (servers != NULL)
                {
                    siridb_servers_send_pkg(
                            servers,
                            pkg,
                            SIRIDB_FWD_SERVERS_TIMEOUT,
                            cb,
                            handle);
                    vec_free(servers);
                }
                else
                {
                    free(pkg);
                    ERR_ALLOC
                }
            }
            break;

        case SIRIDB_QUERY_FWD_POOLS:
            pkg->tp = BPROTO_QUERY_SERVER;
            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    cb,
                    handle,
                    flags);
            break;

        case SIRIDB_QUERY_FWD_SOME_POOLS:
            assert (((query_select_t *) ((siridb_query_t *)
                    handle->data)->data)->tp == QUERIES_SELECT);
            assert (((query_select_t *) ((siridb_query_t *)
                    handle->data)->data)->pmap != NULL);
            pkg->tp = BPROTO_QUERY_SERVER;
            {
                vec_t * borrow_list = imap_vec(((query_select_t *) (
                        (siridb_query_t *) handle->data)->data)->pmap);
                if (borrow_list != NULL)
                {
                    /* if vec is NULL, a signal is raised */
                    siridb_pools_send_pkg_2some(
                            borrow_list,
                            pkg,
                            0,
                            cb,
                            handle,
                            flags);
                }
                else
                {
                    free(pkg);
                }
            }
            break;

        case SIRIDB_QUERY_FWD_UPDATE:
            if (siridb->replica != NULL)
            {
                pkg->tp = BPROTO_QUERY_SERVER;
                siridb_replicate_pkg(siridb, pkg);
            }
            pkg->tp = BPROTO_QUERY_UPDATE;
            siridb_pools_send_pkg(
                    siridb,
                    pkg,
                    0,
                    cb,
                    handle,
                    flags);
            break;

        default:
            assert (0);
            free(pkg);
            break;
        }
    }
    qp_packer_free(packer);
}

/*
 * Unpack an error message from a package and copy the message
 * to query->err_msg.
 *
 * Returns 0 if successful or -1 in case the package data is not valid.
 */
int siridb_query_err_from_pkg(siridb_query_t * query, sirinet_pkg_t * pkg)
{
    qp_unpacker_t unpacker;
    qp_obj_t qp_err;

    /* initialize unpacker */
    qp_unpacker_init(&unpacker, pkg->data, pkg->len);

    if (qp_is_map(qp_next(&unpacker, NULL)) &&
        qp_is_raw(qp_next(&unpacker, NULL)) && /* error_msg  */
        qp_is_raw(qp_next(&unpacker, &qp_err)) &&
        qp_err.len < SIRIDB_MAX_SIZE_ERR_MSG)
    {
        memcpy(query->err_msg, qp_err.via.raw, qp_err.len);
        query->err_msg[qp_err.len] = '\0';
        return 0;
    }

    /* package data did not have a valid error message */
    return -1;
}

void siridb_query_timeit_from_unpacker(
        siridb_query_t * query,
        qp_unpacker_t * unpacker)
{
    assert (query->timeit != NULL);

    qp_types_t tp = qp_next(unpacker, NULL);

    while (qp_is_close(tp))
    {
        tp = qp_next(unpacker, NULL);
    }

    if (    qp_is_raw(tp) &&
            qp_is_array(qp_next(unpacker, NULL)) &&
            qp_is_map(qp_current(unpacker)))
    {
        qp_packer_extend_fu(query->timeit, unpacker);
    }
}

static void QUERY_unique(cleri_olist_t * olist)
{
    while (olist != NULL && olist->next != NULL)
    {
        cleri_olist_t * test = olist;
        while (test->next != NULL)
        {
            if (olist->cl_obj == test->next->cl_obj)
            {
                cleri_olist_t * tmp = test->next->next;
                free(test->next);
                test->next = tmp;
                continue;
            }
            test = test->next;
        }
        olist = olist->next;
    }
}

static void QUERY_send_invalid_error(uv_async_t * handle)
{
    size_t len;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    int count = 0;
    const char * expect;
    cleri_t * cl_obj;

    /* start building the error message */
    len = snprintf(query->err_msg,
            SIRIDB_MAX_SIZE_ERR_MSG,
            "Query error at position %zd. Expecting ",
            query->pr->pos);

    /* required for libcleri versions prior to 0.10.1 */
    QUERY_unique(query->pr->expecting->required);

    /* expand the error message with suggestions. we try to add nice names
     * for regular expressions etc.
     */
    while (query->pr->expect != NULL)
    {
        cl_obj = query->pr->expect->cl_obj;
        if (cl_obj->tp == CLERI_TP_END_OF_STATEMENT)
        {
            expect = "end_of_statement";
        }
        else if (cl_obj->tp == CLERI_TP_KEYWORD)
        {
            expect = cl_obj->via.keyword->keyword;
        }
        else if (cl_obj->tp == CLERI_TP_TOKENS)
        {
            expect = cl_obj->via.tokens->spaced;
        }
        else if (cl_obj->tp == CLERI_TP_TOKEN)
        {
            expect = cl_obj->via.token->token;
        }
        else switch (cl_obj->gid)
        {
        case CLERI_GID_R_SINGLEQ_STR:
            expect = "single_quote_str"; break;
        case CLERI_GID_R_DOUBLEQ_STR:
            expect = "double_quote_str"; break;
        case CLERI_GID_R_GRAVE_STR:
            expect = "grave_str"; break;
        case CLERI_GID_R_INTEGER:
            expect = "integer"; break;
        case CLERI_GID_R_FLOAT:
            expect = "float"; break;
        case CLERI_GID_R_UUID_STR:
            expect = "uuid"; break;
        case CLERI_GID_R_TIME_STR:
            expect = "date/time_string"; break;
        case CLERI_GID_R_REGEX:
            expect = "regular_expression"; break;
        default:
            /* the best result we get is to handle all, but it will not break
             * in case we did not specify some elements.
             */
            query->pr->expect = query->pr->expect->next;
            continue;
        }

        /* make sure len is not greater than the maximum size */
        if (len > SIRIDB_MAX_SIZE_ERR_MSG)
        {
            len = SIRIDB_MAX_SIZE_ERR_MSG;
        }

        /* we use count = 0 to print the first one, then for the others
         * a comma prefix and the last with -or-
         */
        if (!count++)
        {
            len += snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    "%s",
                    expect);
        }
        else if (query->pr->expect->next == NULL)
        {
            len += snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    " or %s",
                    expect);
        }
        else
        {
            len += snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    ", %s",
                    expect);
        }

        query->pr->expect = query->pr->expect->next;
    }

    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
}

static void QUERY_send_no_query(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->packer = sirinet_packer_new(512);
    qp_add_type(query->packer, QP_MAP1);

    siridb_t * siridb = query->client->siridb;

    qp_add_raw(query->packer, (const unsigned char *) "calc", 4);
    uint64_t ts = siridb_time_now(siridb, query->start);

    if (!query->factor)
    {
        qp_add_int64(query->packer, (int64_t) ts);
    }
    else
    {
        double factor = (double) query->factor;
        qp_add_int64(query->packer, (int64_t) (ts * factor));
    }

    siridb_send_query_result(handle);
}

static void QUERY_parse(uv_async_t * handle)
{
    int rc;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = query->client->siridb;

    siridb_walker_t * walker = siridb_walker_new(
            siridb,
            siridb_time_now(siridb, query->start),
            &query->flags);

    if (    walker == NULL ||
            (query->pr = cleri_parse(siri.grammar, query->q)) == NULL)
    {
        if (walker != NULL)
        {
            siridb_nodes_free(siridb_walker_free(walker));
        }
        else
        {
            ERR_ALLOC
        }
        sprintf(query->err_msg,
                "Memory allocation error or maximum recursion depth reached.");
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    if (!query->pr->is_valid)
    {
        siridb_nodes_free(siridb_walker_free(walker));
        QUERY_send_invalid_error(handle);
        return;
    }

    if ((rc = QUERY_walk(
#if SIRIDB_EXPR_ALLOC
            query,
#endif
            query->pr->tree->children->node,
            walker)))
    {
        switch (rc)
        {
        case EXPR_DIVISION_BY_ZERO:
            sprintf(query->err_msg, "Division by zero error.");
            break;
        case EXPR_MODULO_BY_ZERO:
            sprintf(query->err_msg, "Modulo by zero error.");
            break;
        case EXPR_TOO_LONG:
            sprintf(query->err_msg,
                    "Expression too long. (max %d characters)",
                    EXPR_MAX_SIZE);
            break;
        case EXPR_TIME_OUT_OF_RANGE:
            sprintf(query->err_msg, "Time expression out-of-range.");
            break;
        case EXPR_INVALID_DATE_STRING:
            sprintf(query->err_msg, "Invalid date string.");
            break;
        case EXPR_MEM_ALLOC_ERR:
            sprintf(query->err_msg, "Memory allocation error.");
            break;
        default:
            log_critical("Unknown Return Code received: %d", rc);
            assert(0);
        }
        siridb_nodes_free(siridb_walker_free(walker));
        siridb_query_send_error(handle, CPROTO_ERR_QUERY);
        return;
    }

    /* free the walker but keep the nodes list */
    query->nodes = siridb_walker_free(walker);

    if (query->nodes == NULL)
    {
        QUERY_send_no_query(handle);
        return;
    }

    uv_async_t * forward = malloc(sizeof(uv_async_t));
    uv_async_init(siri.loop, forward, (uv_async_cb) query->nodes->cb);
    forward->data = handle->data;
    uv_async_send(forward);
    uv_close((uv_handle_t *) handle, (uv_close_cb) free);
}

static int QUERY_to_packer(qp_packer_t * packer, siridb_query_t * query)
{
    int rc;
    if (query->flags & SIRIDB_QUERY_FLAG_REBUILD)
    {
        /* reserve 200 extra chars */
        char buffer[packer->alloc_size];
        size_t size = packer->alloc_size;
        siridb_t * siridb = query->client->siridb;

        rc = QUERY_rebuild(
                siridb,
                query->pr->tree->children->node,
                buffer,
                &size,
                packer->alloc_size);

        if (rc)
        {
            log_error(
                    "Error '%d' occurred while re-building the query, "
                    "continue using the original query", rc);
            qp_add_string(packer, query->q);
        }
        else
        {
            qp_add_raw(
                    packer,
                    (const unsigned char *) buffer,
                    packer->alloc_size - size);
        }
    }
    else
    {
        qp_add_string(packer, query->q);
    }
    return 0;
}

static int QUERY_walk(
#if SIRIDB_EXPR_ALLOC
        siridb_query_t * query,
#endif
        cleri_node_t * node,
        siridb_walker_t * walker)
{
    int rc;
    uint32_t gid;
    cleri_children_t * current;
    uv_async_cb func;

    gid = node->cl_obj->gid;

    /*
     * When GID is 0 this means CLERI_NONE
     */
    if (gid != CLERI_NONE)
    {
        if (    (func = siridb_node_get_enter(gid)) != NULL &&
                siridb_walker_append(walker, node, func))
        {
            return EXPR_MEM_ALLOC_ERR;
        }

        if (    (func = siridb_node_get_exit(gid)) != NULL &&
                siridb_walker_insert(walker, node, func))
        {
            return EXPR_MEM_ALLOC_ERR;
        }
    }

    if (gid == CLERI_GID_TIME_EXPR || gid == CLERI_GID_CALC_STMT)
    {
        char buffer[EXPR_MAX_SIZE];
        size_t size = EXPR_MAX_SIZE;

        /* we can have nested integer and time expressions */
        if ((rc = QUERY_time_expr(
                node->children->node,
                walker,
                buffer,
                &size)))
        {
            return rc;
        }

        /* terminate buffer */
        buffer[EXPR_MAX_SIZE - size] = 0;

        #if SIRIDB_EXPR_ALLOC
        {
            int64_t * itmp = malloc(sizeof(int64_t));
            if (itmp == NULL || llist_append(query->expr_cache, itmp))
            {
                free(itmp);
                return EXPR_MEM_ALLOC_ERR;
            }
            node->data = itmp;
        }
        #endif

        /* evaluate the expression */
        if ((rc = expr_parse(CLERI_NODE_DATA_ADDR(node), buffer)))
        {
            return rc;
        }

        /* check if timestamp is valid */
        if (!siridb_int64_valid_ts(walker->siridb->time, CLERI_NODE_DATA(node)))
        {
            return EXPR_TIME_OUT_OF_RANGE;
        }
    }
    else if (gid == CLERI_GID_INT_EXPR)
    {
        char buffer[EXPR_MAX_SIZE];
        size_t size = EXPR_MAX_SIZE;

        if ((rc = QUERY_int_expr(
                node->children->node,
                buffer,
                &size)))
        {
            return rc;
        }

        /* terminate buffer */
        buffer[EXPR_MAX_SIZE - size] = 0;

        #if SIRIDB_EXPR_ALLOC
        {
            int64_t * itmp = malloc(sizeof(int64_t));
            if (itmp == NULL || llist_append(query->expr_cache, itmp))
            {
                free(itmp);
                return EXPR_MEM_ALLOC_ERR;
            }
            node->data = itmp;
        }
        #endif

        /* evaluate the expression */
        if ((rc = expr_parse(CLERI_NODE_DATA_ADDR(node), buffer)))
        {
            return rc;
        }
    }
    else
    {
        current = node->children;
        while (current != NULL && current->node != NULL)
        {
            /*
             * We should not simple walk because THIS has no
             * cl_obj->cl_obj and THIS is save to skip.
             */
            while (current->node->cl_obj->tp == CLERI_TP_THIS)
            {
                current = current->node->children;
            }
            if ((rc = QUERY_walk(
#if SIRIDB_EXPR_ALLOC
                    query,
#endif
                    current->node,
                    walker)))
            {
                return rc;
            }
            current = current->next;
        }
    }

    return 0;
}

static int QUERY_time_expr(
        cleri_node_t * node,
        siridb_walker_t * walker,
        char * buf,
        size_t * size)
{
    int n;

    switch (node->cl_obj->tp)
    {
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
        /* tokens like + - ( etc. */
        *(buf + EXPR_MAX_SIZE - *size) = *node->str;
        return (--(*size)) ? 0 : EXPR_TOO_LONG;

    case CLERI_TP_KEYWORD:
        /* this is now */
        n = snprintf(
                buf + EXPR_MAX_SIZE - *size,
                *size,
                "%" PRIu64,
                walker->now);
        if (n >= (ssize_t) *size)
        {
            return EXPR_TOO_LONG;
        }
        *walker->flags |= SIRIDB_QUERY_FLAG_REBUILD;
        *size -= n;
        return 0;

    case CLERI_TP_REGEX:
        /* can be an integer or time string like 2d or something */
        switch (node->cl_obj->gid)
        {
        case CLERI_GID_R_INTEGER:
            if (node->len >= *size)
            {
                return EXPR_TOO_LONG;
            }
            memcpy(buf + EXPR_MAX_SIZE - *size, node->str, node->len);
            *size -= node->len;
            return 0;

        case CLERI_GID_R_TIME_STR:
            n = snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%" PRIu64,
                        siridb_time_parse(node->str, node->len) *
                        (uint64_t)walker->siridb->time->factor);
            if (n >= (ssize_t) *size)
            {
                return EXPR_TOO_LONG;
            }
            *size -= n;
            return 0;
        }
        log_critical(
                "Unexpected object in time expression received: %d",
                node->cl_obj->tp);
        assert (0);
        break;

    case CLERI_TP_CHOICE:
        /* this is a string (single or double quoted) */
        {
            char datestr[node->len - 1];

            /* extract date string */
            xstr_extract_string(datestr, node->str, node->len);

            /* get timestamp from date string */
            int64_t ts = iso8601_parse_date(datestr, walker->siridb->tz);

            if (ts < 0)
            {
                return EXPR_INVALID_DATE_STRING;
            }

            n = snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%" PRId64,
                    ts * (uint64_t)walker->siridb->time->factor);

            if (n >= (ssize_t) *size)
            {
                return EXPR_TOO_LONG;
            }
            *walker->flags |= SIRIDB_QUERY_FLAG_REBUILD;
            *size -= n;
        }
        return 0;

    default:
        /* anything else, probably THIS or a sequence */
        {
            int rc;
            cleri_children_t * current;

            current = node->children;
            while (current != NULL && current->node != NULL)
            {
                if ((rc = QUERY_time_expr(
                        current->node,
                        walker,
                        buf,
                        size)))
                {
                    return rc;
                }
                current = current->next;
            }
        }
    }
    return 0;
}

static int QUERY_int_expr(cleri_node_t * node, char * buf, size_t * size)
{
    switch (node->cl_obj->tp)
    {
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
        /* tokens like + - ( etc. */
        *(buf + EXPR_MAX_SIZE - *size) = *node->str;
        return (--(*size)) ? 0 : EXPR_TOO_LONG;

    case CLERI_TP_REGEX:
        /* this is an integer */
        if (node->len >= *size)
        {
            return EXPR_TOO_LONG;
        }
        memcpy(buf + EXPR_MAX_SIZE - *size, node->str, node->len);
        *size -= node->len;
        return 0;

    default:
        /* anything else, probably THIS or a sequence */
        {
            int rc;
            cleri_children_t * current;

            current = node->children;
            while (current != NULL && current->node != NULL)
            {
                if ((rc = QUERY_int_expr(
                        current->node,
                        buf,
                        size)))
                {
                    return rc;
                }
                current = current->next;
            }
        }
    }
    return 0;
}

/*
 * Returns 0 if successful or QUERY_TOO_LONG
 */
static int QUERY_rebuild(
        siridb_t * siridb,
        cleri_node_t * node,
        char * buf,
        size_t * size,
        const size_t max_size)
{
    switch (node->cl_obj->tp)
    {
    case CLERI_TP_REGEX:
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
    case CLERI_TP_KEYWORD:
        if (node->len >= *size)
        {
            return QUERY_TOO_LONG;
        }
        memcpy(buf + max_size - *size, node->str, node->len);
        *size -= node->len;

        *(buf + max_size - *size) = ' ';
        return (--(*size)) ? 0 : QUERY_TOO_LONG;

    case CLERI_TP_CHOICE:
    case CLERI_TP_RULE:
        switch (node->cl_obj->gid)
        {
        case CLERI_GID_UUID:
            {
                siridb_server_t * server = siridb_server_from_node(
                        siridb,
                        node->children->node,
                        NULL);
                if (server != NULL)
                {
                    char uuid[37];
                    uuid_unparse_lower(server->uuid, uuid);
                    int n;
                    n = snprintf(
                            buf + max_size - *size,
                            *size,
                            "%s ",
                            uuid);
                    if (n >= (ssize_t) *size)
                    {
                        return QUERY_TOO_LONG;
                    }
                    *size -= n;
                    return 0;
                }
            }
            break;
        case CLERI_GID_INT_EXPR:
        case CLERI_GID_TIME_EXPR:
            {
                int n;
                n = snprintf(
                        buf + max_size - *size,
                        *size,
                        "%" PRId64 " ",
                        CLERI_NODE_DATA(node));
                if (n >= (ssize_t) *size)
                {
                    return QUERY_TOO_LONG;
                }
                *size -= n;
            }
            return 0;
        }
    /* FALLTHRU */
    /* fall through */
    default:
        {
            int rc;
            cleri_children_t * current;

            current = node->children;
            while (current != NULL && current->node != NULL)
            {
                if ((rc = QUERY_rebuild(
                        siridb,
                        current->node,
                        buf,
                        size,
                        max_size)))
                {
                    return rc;
                }
                current = current->next;
            }
        }
    }
    return 0;
}
