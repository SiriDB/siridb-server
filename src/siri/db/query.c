/*
 * query.c - Responsible for parsing queries.
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
#include <cleri/olist.h>
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
#include <siri/net/clserver.h>
#include <siri/net/pkg.h>
#include <siri/net/socket.h>
#include <siri/parser/listener.h>
#include <siri/parser/queries.h>
#include <siri/siri.h>
#include <strextra/strextra.h>
#include <string.h>
#include <sys/time.h>
#include <siri/err.h>

#ifndef DEBUG
#include <math.h>
#else
#include <motd/motd.h>
#endif

#define QUERY_TOO_LONG -1
#define QUERY_MAX_LENGTH 8192
#define QUERY_EXTRA_ALLOC_SIZE 200

static void QUERY_send_invalid_error(uv_async_t * handle);
static void QUERY_parse(uv_async_t * handle);
static int QUERY_walk(cleri_node_t * node, siridb_walker_t * walker);
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
        uv_stream_t * client,
        const char * q,
        size_t q_len,
        siridb_timep_t time_precision,
        int flags)
{
    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    if (handle == NULL)
    {
        ERR_ALLOC
        return;
    }
    siridb_query_t * query = (siridb_query_t *) malloc(sizeof(siridb_query_t));
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

    /*
     * Set start time.
     * (must be real time since we translate now with this value)
     */
    clock_gettime(CLOCK_REALTIME, &query->start);

    /* bind pid, client and flags so we can send back the result */
    query->pid = pid;

    /* increment client reference counter */
    sirinet_socket_incref(client);

    query->client = client;
    query->flags = flags;

    /* bind time precision (this can never be equal to the SiriDB precision) */
    query->time_precision = time_precision;

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
    ((sirinet_socket_t *) query->client->data)->siridb->active_tasks++;

    /* send next call */
    uv_async_init(siri.loop, handle, (uv_async_cb) QUERY_parse);
    handle->data = query;
    uv_async_send(handle);
}

void siridb_query_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* decrement active tasks */
    ((sirinet_socket_t *) query->client->data)->siridb->active_tasks--;

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

    /* decrement client reference counter */
    sirinet_socket_decref(query->client);

    /* free query */
    free(query);

    /* free handle */
    free(handle);

    #ifdef DEBUG
    log_debug("Free query!, hooray!");
    #endif
}

void siridb_send_query_result(uv_async_t * handle)
{
    /*
     * Its important to have another callback to free stuff so we can
     * clean everything without sending things in case of a client failure
     */
    siridb_query_t * query = (siridb_query_t *) handle->data;
#ifdef DEBUG
    if (query->packer == NULL)
    {
        sprintf(query->err_msg, "CRITICAL: We have nothing to send!");
        return siridb_query_send_error(handle, CPROTO_ERR_QUERY);
    }
#endif
    sirinet_pkg_t * pkg = sirinet_packer2pkg(
            query->packer,
            query->pid,
            CPROTO_RES_QUERY);

    sirinet_pkg_send((uv_stream_t *) query->client, pkg);

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
    sirinet_pkg_t * package = sirinet_pkg_err(
                query->pid,
                strlen(query->err_msg),
                err,  // usually this is CPROTO_ERR_QUERY, CPROTO_ERR_POOL etc.
                query->err_msg);

    log_warning("(%s) %s", sirinet_cproto_server_str(err), query->err_msg);

    if (package != NULL)
    {
        /* ignore result code, signal can be raised */
        sirinet_pkg_send((uv_stream_t *) query->client, package);
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
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;

    /*
     * the size is important here, we will use the alloc_size to guess the
     * maximum query size in QUERY_to_packer.
     */
    qp_packer_t * packer =
            qp_packer_new(query->pr->tree->len + QUERY_EXTRA_ALLOC_SIZE);

    if (packer == NULL)
    {
        return;  /* signal is raised */
    }

    qp_add_type(packer, QP_ARRAY2);

    /* add the query to the packer */
    QUERY_to_packer(packer, query);

    qp_add_int8(packer, query->time_precision);

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
                slist_t * servers = siridb_servers_other2slist(siridb);
                if (servers != NULL)
                {
                    siridb_servers_send_pkg(
                            servers,
                            pkg,
                            0,
                            cb,
                            handle);
                    slist_free(servers);
                }
                else
                {
                    free(pkg);
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
#ifdef DEBUG

            assert (((query_select_t *) ((siridb_query_t *)
                    handle->data)->data)->tp == QUERIES_SELECT);
            assert (((query_select_t *) ((siridb_query_t *)
                    handle->data)->data)->pmap != NULL);
#endif
            pkg->tp = BPROTO_QUERY_SERVER;
            {
                slist_t * borrow_list = imap_slist(((query_select_t *) (
                        (siridb_query_t *) handle->data)->data)->pmap);
                if (borrow_list != NULL)
                {
                    /* if slist is NULL, a signal is raised */
                    siridb_pools_send_pkg_2some(
                            siridb,
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

void siridb_query_timeit_from_unpacker(
        siridb_query_t * query,
        qp_unpacker_t * unpacker)
{
#ifdef DEBUG
    assert (query->timeit != NULL);
#endif

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

static void QUERY_send_invalid_error(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    size_t len = 0;
    int count = 0;
    const char * expect;
    cleri_olist_t * expecting;

    /* remove comment from suggestions since this is boring info */
    cleri_expecting_remove(query->pr->expecting, CLERI_GID_R_COMMENT);

    /* we always need required since cleri uses required as its final
     * suggestions tree.
     */
    expecting = query->pr->expecting->required;

    /* start building the error message */
    len = snprintf(query->err_msg,
            SIRIDB_MAX_SIZE_ERR_MSG,
            "Query error at position %zd. Expecting ",
            query->pr->pos);

    /* expand the error message with suggestions. we try to add nice names
     * for regular expressions etc.
     */
    while (expecting != NULL && expecting->cl_obj != NULL)
    {
        if (expecting->cl_obj->tp == CLERI_TP_END_OF_STATEMENT)
        {
            expect = "end_of_statement";
        }
        else if (expecting->cl_obj->tp == CLERI_TP_KEYWORD)
        {
            expect = expecting->cl_obj->via.keyword->keyword;
        }
        else if (expecting->cl_obj->tp == CLERI_TP_TOKENS)
        {
            expect = expecting->cl_obj->via.tokens->spaced;
        }
        else if (expecting->cl_obj->tp == CLERI_TP_TOKEN)
        {
            expect = expecting->cl_obj->via.token->token;
        }
        else switch (expecting->cl_obj->via.dummy->gid)
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
            expecting = expecting->next;
            continue;
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
        else if (expecting->next == NULL || expecting->next->cl_obj == NULL)
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

        expecting = expecting->next;
    }
    siridb_query_send_error(handle, CPROTO_ERR_QUERY);
}

static void QUERY_send_no_query(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    query->packer = sirinet_packer_new(512);
    qp_add_type(query->packer, QP_MAP1);

#ifndef DEBUG
    /* production version returns timestamp now */
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
    qp_add_raw(query->packer, "calc", 4);
    uint64_t ts = siridb_time_now(siridb, query->start);

    if (query->time_precision == SIRIDB_TIME_DEFAULT)
    {
        qp_add_int64(query->packer, (int64_t) ts);
    }
    else
    {
        double factor =
                pow(1000.0, query->time_precision - siridb->time->precision);
        qp_add_int64(query->packer, (int64_t) (ts * factor));
    }

#else
    /* development release returns motd */
    const char * msg;
    msg = motd_get_random_msg();
    qp_add_raw(query->packer, "motd", 4);
    qp_add_string(query->packer, msg);


#endif
    siridb_send_query_result(handle);
}

static void QUERY_parse(uv_async_t * handle)
{
    int rc;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_socket_t *) query->client->data)->siridb;
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
    	sprintf(query->err_msg, "Memory allocation error.");
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

    uv_async_t * forward = (uv_async_t *) malloc(sizeof(uv_async_t));
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

        rc = QUERY_rebuild(
                ((sirinet_socket_t *) query->client->data)->siridb,
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
            qp_add_raw(packer, buffer, packer->alloc_size - size);
        }
    }
    else
    {
        qp_add_string(packer, query->q);
    }
    return 0;
}

static int QUERY_walk(cleri_node_t * node, siridb_walker_t * walker)
{
    int rc;
    uint32_t gid;
    cleri_children_t * current;
    uv_async_cb func;

    gid = node->cl_obj->via.dummy->gid;

    /*
     * When GID is 0 this means CLERI_NONE
     */
    if (gid != CLERI_NONE)
    {
        if (    (func = siriparser_listen_enter[gid]) != NULL &&
                siridb_walker_append(walker, node, func))
        {
            return EXPR_MEM_ALLOC_ERR;
        }
        if (    (func = siriparser_listen_exit[gid]) != NULL &&
                siridb_walker_insert(walker, node, func))
        {
            return EXPR_MEM_ALLOC_ERR;
        }
    }


    if (gid == CLERI_GID_TIME_EXPR)
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

        /* evaluate the expression */
        if ((rc = expr_parse(&node->result, buffer)))
        {
            return rc;
        }

        /* check if timestamp is valid */
        if (!siridb_int64_valid_ts(walker->siridb, node->result))
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

        /* evaluate the expression */
        if ((rc = expr_parse(&node->result, buffer)))
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
            if ((rc = QUERY_walk(current->node, walker)))
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
                "%llu",
                (unsigned long long) walker->now);
        if (n >= *size)
        {
            return EXPR_TOO_LONG;
        }
        *walker->flags |= SIRIDB_QUERY_FLAG_REBUILD;
        *size -= n;
        return 0;

    case CLERI_TP_REGEX:
        /* can be an integer or time string like 2d or something */
        switch (node->cl_obj->via.regex->gid)
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
                    "%llu",
					(unsigned long long)
						siridb_time_parse(node->str, node->len) *
                        walker->siridb->time->factor);
            if (n >= *size)
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
            strx_extract_string(datestr, node->str, node->len);

            /* get timestamp from date string */
            int64_t ts = iso8601_parse_date(datestr, walker->siridb->tz);

            if (ts < 0)
            {
                return EXPR_INVALID_DATE_STRING;
            }

            n = snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%lld",
					(long long) ts * walker->siridb->time->factor);

            if (n >= *size)
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
        switch (node->cl_obj->via.dummy->gid)
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
                    if (n >= *size)
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
                        "%lld ",
                        (long long) node->result);
                if (n >= *size)
                {
                    return QUERY_TOO_LONG;
                }
                *size -= n;
            }
            return 0;
        }
    /* no break */
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
