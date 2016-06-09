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
#include <siri/db/node.h>
#include <siri/db/time.h>
#include <siri/net/handle.h>
#include <string.h>
#include <siri/net/clserver.h>
#include <cleri/olist.h>
#include <motd/motd.h>
#include <assert.h>
#include <strextra/strextra.h>
#include <iso8601/iso8601.h>
#include <expr/expr.h>

static void siridb_parse_query(uv_async_t * handle);
static int QUERY_walk(
        cleri_node_t * node,
        siridb_node_walker_t * walker);
static void siridb_send_invalid_query_error(uv_async_t * handle);

static int QUERY_time_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        char * buf,
        size_t * size);

static int QUERY_int_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        char * buf,
        size_t * size);

void siridb_async_query(
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
    query->free_cb = siridb_free_query;

    /* set query */
    query->q = (char *) malloc(q_len + 1);
    memcpy(query->q, q, q_len); // copy query
    query->q[q_len] = 0;        // write null terminator

    /* We should initialize the packer based on query type */
    query->packer = NULL;
    query->timeit = NULL;

    /* make sure all *other* pointers are set to NULL */
    query->data = NULL;
    query->pr = NULL;
    query->node_list = NULL;

    /* send next call */
    uv_async_init(siri.loop, handle, (uv_async_cb) siridb_parse_query);
    handle->data = (void *) query;
    uv_async_send(handle);
}

void siridb_free_query(uv_handle_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;

    /* free query */
    free(query->q);

    /* free qpack buffers */
    qp_free_packer(query->packer);
    qp_free_packer(query->timeit);

    /* free node list */
    siridb_free_node_list(query->node_list);

    /* free query result */
    cleri_free_parse_result(query->pr);

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
    package = sirinet_new_pkg(
            query->pid,
            query->packer->len,
            SN_MSG_RESULT,
            query->packer->buffer);
    sirinet_send_pkg(query->client, package, NULL);
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

    package = sirinet_new_pkg(
            query->pid,
            len,
            err,  // usually this is SN_MSG_QUERY_ERROR
            query->err_msg);

    sirinet_send_pkg(query->client, package, NULL);
    free(package);

    uv_close((uv_handle_t *) handle, (uv_close_cb) query->free_cb);
}

static void siridb_send_invalid_query_error(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    size_t len = 0;
    int count = 0;
    const char * expect;
    cleri_olist_t * expecting;

    /* remove comment from suggestions since this is boring info */
    cleri_remove_from_expecting(query->pr->expecting, CLERI_GID_R_COMMENT);

    /* we always need required since cleri uses required as its final
     * suggestions tree.
     */
    expecting = query->pr->expecting->required;

    /* start building the error message */
    snprintf(query->err_msg,
            SIRIDB_MAX_SIZE_ERR_MSG,
            "Query error at position %zd. Expecting ",
            query->pr->pos);

    /* expand the error message with suggestions. we try to add nice names
     * for regular expressions etc.
     */
    while (expecting != NULL && expecting->cl_obj != NULL)
    {
        len = strlen(query->err_msg);
        if (expecting->cl_obj->tp == CLERI_TP_END_OF_STATEMENT)
            expect = "end_of_statement";
        else if (expecting->cl_obj->tp == CLERI_TP_KEYWORD)
            expect = expecting->cl_obj->cl_obj->keyword->keyword;
        else if (expecting->cl_obj->tp == CLERI_TP_TOKENS)
            expect = expecting->cl_obj->cl_obj->tokens->spaced;
        else if (expecting->cl_obj->tp == CLERI_TP_TOKEN)
            expect = expecting->cl_obj->cl_obj->token->token;
        else switch (expecting->cl_obj->cl_obj->dummy->gid)
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
            snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    "%s",
                    expect);
        else if (expecting->next == NULL || expecting->next->cl_obj == NULL)
            snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    " or %s",
                    expect);
        else
            snprintf(query->err_msg + len,
                    SIRIDB_MAX_SIZE_ERR_MSG - len,
                    ", %s",
                    expect);

        expecting = expecting->next;
    }
    siridb_send_error(handle, SN_MSG_QUERY_ERROR);
}

static void siridb_send_motd(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    const char * msg;

    query->packer = qp_new_packer(512);
    qp_add_type(query->packer, QP_MAP1);
    qp_add_raw(query->packer, "motd", 4);
    msg = motd_get_random_msg();
    qp_add_raw(query->packer, msg, strlen(msg));

    siridb_send_query_result(handle);
}

static void siridb_parse_query(uv_async_t * handle)
{
    int rc;
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_t * siridb = ((sirinet_handle_t *) query->client->data)->siridb;
    siridb_node_walker_t * walker = siridb_new_node_walker(
            siridb,
            siridb_time_now(siridb, query->start));

    query->pr = cleri_parse(siri.grammar, query->q);

    if (!query->pr->is_valid)
        return siridb_send_invalid_query_error(handle);

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
            /* Unknown error */
            assert(0);
        }
        return siridb_send_error(handle, SN_MSG_QUERY_ERROR);
    }


    /* siridb_node_chain also free's the walker; we now only need to
     * cleanup the node list.
     */
    query->node_list = siridb_node_chain(walker);

    if (query->node_list == NULL)
        return siridb_send_motd(handle);

    uv_async_t * forward = (uv_async_t *) malloc(sizeof(uv_async_t));
    uv_async_init(siri.loop, forward, (uv_async_cb) query->node_list->cb);
    forward->data = (void *) handle->data;
    uv_async_send(forward);
    uv_close((uv_handle_t *) handle, (uv_close_cb) sirinet_free_async);
}

static int QUERY_walk(
        cleri_node_t * node,
        siridb_node_walker_t * walker)
{
    int rc;
    uint32_t gid;
    cleri_children_t * current;
    uv_async_cb func;

    if ((gid = node->cl_obj->cl_obj->dummy->gid))
    {
        if ((func = siriparser_listen_enter[gid]) != NULL)
            siridb_append_enter_node(walker, node, func);
        if ((func = siriparser_listen_exit[gid]) != NULL)
            siridb_insert_exit_node(walker, node, func);
    }

    /* we should not simple walk rules, better do something else..
     * (simply walk the rules would break because THIS has no
     *  cl_obj->cl_obj)
     */
    if (node->cl_obj->tp != CLERI_TP_RULE)
    {
        current = node->children;
        while (current != NULL && current->node != NULL)
        {
            if ((rc = QUERY_walk(current->node, walker)))
                return rc;
            current = current->next;
        }
    }
    else
    {
        char buffer[EXPR_MAX_SIZE];
        size_t size = EXPR_MAX_SIZE;

        /* we can have nested integer and time expressions */
        if (node->cl_obj->cl_obj->rule->gid == CLERI_GID_TIME_EXPR)
        {
            if ((rc = QUERY_time_expr(
                    node,
                    walker,
                    buffer,
                    &size)))
                return rc;

            /* terminate buffer */
            buffer[EXPR_MAX_SIZE - size] = 0;

            /* evaluate the expression */
            if ((rc = expr_parse(&node->result, buffer)))
                return rc;

            /* check if timestamp is valid */
            if (!siridb_int64_valid_ts(walker->siridb, node->result))
                return EXPR_TIME_OUT_OF_RANGE;
        }
        else
        {
#ifdef DEBUG
            assert (node->cl_obj->cl_obj->rule->gid == CLERI_GID_INT_EXPR);
#endif
            if ((rc = QUERY_int_expr(
                    node,
                    walker,
                    buffer,
                    &size)))
                return rc;

            /* terminate buffer */
            buffer[EXPR_MAX_SIZE - size] = 0;

            /* evaluate the expression */
            if ((rc = expr_parse(&node->result, buffer)))
                return rc;
        }
    }
    return 0;
}

static int QUERY_time_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        char * buf,
        size_t * size)
{
    cleri_children_t * current;

    switch (node->cl_obj->tp)
    {
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
        /* tokens like + - ( etc. */
        *(buf + EXPR_MAX_SIZE - *size) = *node->str;
        return (--(*size)) ? 0 : EXPR_TOO_LONG;

    case CLERI_TP_KEYWORD:
        /* this is now */
        *size -= snprintf(
                buf + EXPR_MAX_SIZE - *size,
                *size,
                "%ld",
                walker->now);
        return (*size) ? 0 : EXPR_TOO_LONG;

    case CLERI_TP_REGEX:
        /* can be an integer or time string like 2d or something */
        switch (node->cl_obj->cl_obj->regex->gid)
        {
        case CLERI_GID_R_INTEGER:
            if (node->len >= *size)
                return EXPR_TOO_LONG;
            memcpy(buf + EXPR_MAX_SIZE - *size, node->str, node->len);
            *size -= node->len;
            return 0;

        case CLERI_GID_R_TIME_STR:
            *size -= snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%ld",
                    siridb_time_parse(node->str, node->len) *
                        walker->siridb->time->factor);
            return (*size) ? 0 : EXPR_TOO_LONG;
        }
        /* we should never get here */
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
                return EXPR_INVALID_DATE_STRING;

            *size -= snprintf(
                    buf + EXPR_MAX_SIZE - *size,
                    *size,
                    "%ld",
                    ts * walker->siridb->time->factor);
        }
        return (*size) ? 0 : EXPR_TOO_LONG;

    default:
        /* anything else, probably THIS or a sequence */
        {
            int rc;
            current = node->children;
            while (current != NULL && current->node != NULL)
            {
                if ((rc = QUERY_time_expr(
                        current->node,
                        walker,
                        buf,
                        size)))
                    return rc;
                current = current->next;
            }
        }
    }
    return 0;
}

static int QUERY_int_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        char * buf,
        size_t * size)
{
    cleri_children_t * current;

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
            return EXPR_TOO_LONG;
        memcpy(buf + EXPR_MAX_SIZE - *size, node->str, node->len);
        *size -= node->len;
        return 0;

    default:
        /* anything else, probably THIS or a sequence */
        {
            int rc;
            current = node->children;
            while (current != NULL && current->node != NULL)
            {
                if ((rc = QUERY_int_expr(
                        current->node,
                        walker,
                        buf,
                        size)))
                    return rc;
                current = current->next;
            }
        }
    }
    return 0;
}

