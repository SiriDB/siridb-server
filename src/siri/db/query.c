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
#include <siri/db/query.h>
#include <logger/logger.h>
#include <siri/siri.h>
#include <sys/time.h>
#include <siri/parser/listener.h>
#include <siri/db/time.h>
#include <string.h>
#include <siri/net/clserver.h>
#include <cleri/olist.h>
#include <motd/motd.h>
#include <assert.h>
#include <strextra/strextra.h>
#include <iso8601/iso8601.h>
#include <expr/expr.h>
#include <siri/db/nodes.h>
#include <siri/net/pkg.h>
#include <siri/net/socket.h>
#include <siri/db/walker.h>
#include <siri/db/servers.h>

#define QUERY_TOO_LONG -1
#define QUERY_MAX_LENGTH 8192
#define QUERY_EXTRA_ALLOC_SIZE 200

static void siridb_send_invalid_query_error(uv_async_t * handle);
static void QUERY_parse(uv_async_t * handle);
static int QUERY_walk(cleri_node_t * node, siridb_walker_t * walker);
static void QUERY_to_packer(qp_packer_t * packer, siridb_query_t * query);
static int QUERY_time_expr(
        cleri_node_t * node,
        siridb_walker_t * walker,
        char * buf,
        size_t * size);
static int QUERY_int_expr(cleri_node_t * node, char * buf, size_t * size);
static int QUERY_rebuild(
        cleri_node_t * node,
        char * buf,
        size_t * size,
        const size_t max_size);

void siridb_query_run(
        uint64_t pid,
        uv_handle_t * client,
        const char * q,
        size_t q_len,
        siridb_timep_t time_precision,
        int flags)
{
    siridb_query_t * query;
    uv_async_t * handle = (uv_async_t *) malloc(sizeof(uv_async_t));
    query = (siridb_query_t *) malloc(sizeof(siridb_query_t));

    /* set start time */
    clock_gettime(CLOCK_REALTIME, &query->start);

    /* bind pid, client and flags so we can send back the result */
    query->pid = pid;
    query->client = client;
    query->flags = flags;

    /* bind time precision (this can never be equal to the SiriDB precision) */
    query->time_precision = time_precision;

    /* set the default callback, this might change when custom
     * data is linked to the query handle
     */
    query->free_cb = siridb_query_free;

    /* set query */
    query->q = strndup(q, q_len);

    /* We should initialize the packer based on query type */
    query->packer = NULL;
    query->timeit = NULL;

    /* make sure all *other* pointers are set to NULL */
    query->data = NULL;
    query->pr = NULL;
    query->nodes = NULL;

    log_debug("Parsing query (%d): %s", query->flags, query->q);

    /* send next call */
    uv_async_init(siri.loop, handle, (uv_async_cb) QUERY_parse);
    handle->data = (void *) query;
    uv_async_send(handle);
}

void siridb_query_free(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* free query */
    free(query->q);

    /* free qpack buffers */
    if (query->packer != NULL)
    {
        qp_free_packer(query->packer);
    }

    if (query->timeit != NULL)
    {
        qp_free_packer(query->timeit);
    }

    /* free node list */
    siridb_nodes_free(query->nodes);

    /* free query result */
    cleri_parser_free(query->pr);

    /* free query */
    free(query);

    /* free handle */
    free((uv_async_t *) handle);

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
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }
#endif
    sirinet_pkg_t * package;
    package = sirinet_pkg_new(
            query->pid,
            query->packer->len,
            SN_MSG_RESULT,
            query->packer->buffer);
    sirinet_pkg_send((uv_stream_t *) query->client, package, NULL, NULL);
    free(package);

    uv_close((uv_handle_t *) handle, (uv_close_cb) query->free_cb);
}

void siridb_send_error(
        uv_async_t * handle,
        sirinet_msg_t err)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    sirinet_pkg_t * package;
    size_t len = strlen(query->err_msg);

    package = sirinet_pkg_new(
            query->pid,
            len,
            err,  // usually this is SN_MSG_QUERY_ERROR
            query->err_msg);

    sirinet_pkg_send((uv_stream_t *) query->client, package, NULL, NULL);
    free(package);

    uv_close((uv_handle_t *) handle, (uv_close_cb) query->free_cb);
}

void siridb_query_forward(
        uv_async_t * handle,
        uint16_t tp,
        sirinet_promises_cb_t cb)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* the size is important here, we will use the alloc_size to quess the
     * maximum query size in QUERY_to_packer.
     */
    qp_packer_t * packer =
            qp_packer_new(query->pr->tree->len + QUERY_EXTRA_ALLOC_SIZE);

    qp_add_type(packer, QP_ARRAY2);

    /* add the query to the packer */
    QUERY_to_packer(packer, query);

    qp_add_int8(packer, query->time_precision);

    switch (tp)
    {
    case BP_QUERY_SERVER:
        siridb_servers_send_pkg(
                ((sirinet_socket_t *) query->client->data)->siridb,
                packer->len,
                tp,
                packer->buffer,
                0,
                cb,
                handle);
        break;

    case BP_QUERY_POOL:
        siridb_pools_send_pkg(
                ((sirinet_socket_t *) query->client->data)->siridb,
                packer->len,
                tp,
                packer->buffer,
                0,
                cb,
                handle);
        break;

    default:
        assert (0);
        break;
    }

    qp_free_packer(packer);
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
        qp_extend_from_unpacker(query->timeit, unpacker);
    }
}

static void siridb_send_invalid_query_error(uv_async_t * handle)
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
    siridb_send_error(handle, SN_MSG_QUERY_ERROR);
}

static void siridb_send_motd(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    const char * msg;

    query->packer = qp_packer_new(512);
    qp_add_type(query->packer, QP_MAP1);
    qp_add_raw(query->packer, "motd", 4);
    msg = motd_get_random_msg();
    qp_add_string(query->packer, msg);

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

    if ((query->pr = cleri_parser_new(siri.grammar, query->q)) == NULL)
    {
        return;  /* signal is set */
    }

    if (!query->pr->is_valid)
    {
        siridb_nodes_free(siridb_walker_free(walker));
        return siridb_send_invalid_query_error(handle);
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
        default:
            log_critical("Unknown Return Code received: %d", rc);
            assert(0);
        }
        siridb_nodes_free(siridb_walker_free(walker));
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }

    /* free the walker but keep the nodes list */
    query->nodes = siridb_walker_free(walker);

    if (query->nodes == NULL)
    {
        return siridb_send_motd(handle);
    }

    uv_async_t * forward = (uv_async_t *) malloc(sizeof(uv_async_t));
    uv_async_init(siri.loop, forward, (uv_async_cb) query->nodes->cb);
    forward->data = (void *) handle->data;
    uv_async_send(forward);
    uv_close((uv_handle_t *) handle, (uv_close_cb) free);
}

static void QUERY_to_packer(qp_packer_t * packer, siridb_query_t * query)
{
    int rc;
    if (query->flags & SIRIDB_QUERY_FLAG_REBUILD)
    {
        /* reserve 200 extra chars */
        char buffer[packer->alloc_size];
        size_t size = packer->alloc_size;

        rc = QUERY_rebuild(
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
        if ((func = siriparser_listen_enter[gid]) != NULL)
        {
            siridb_walker_append(walker, node, func);
        }
        if ((func = siriparser_listen_exit[gid]) != NULL)
        {
            siridb_walker_insert(walker, node, func);
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
                "%lu",
                walker->now);
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
                    "%lu",
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
            int64_t ts = iso8601_parse_date(datestr, 432);

            if (ts < 0)
            {
                return EXPR_INVALID_DATE_STRING;
            }

            n = snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%ld",
                    ts * walker->siridb->time->factor);

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

static int QUERY_rebuild(
        cleri_node_t * node,
        char * buf,
        size_t * size,
        const size_t max_size)
{
    switch (node->cl_obj->tp)
    {
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
    case CLERI_TP_REGEX:
    case CLERI_TP_KEYWORD:
        if (node->len >= *size)
        {
            return QUERY_TOO_LONG;
        }
        memcpy(buf + max_size - *size, node->str, node->len);
        *size -= node->len;

        *(buf + max_size - *size) = ' ';
        return (--(*size)) ? 0 : QUERY_TOO_LONG;

    case CLERI_TP_RULE:
        switch (node->cl_obj->via.dummy->gid)
        {
        case CLERI_GID_INT_EXPR:
        case CLERI_GID_TIME_EXPR:
            {
                int n;
                n = snprintf(
                        buf + max_size - *size,
                        *size,
                        "%ld ",
                        node->result);
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
