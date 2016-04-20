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
#include <siri/db/listener.h>
#include <siri/db/node.h>
#include <siri/net/handle.h>
#include <string.h>
#include <siri/net/clserver.h>
#include <cleri/olist.h>
#include <motd/motd.h>
#include <assert.h>

static void siridb_parse_query(uv_async_t * handle);
static void siridb_walk(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        const uint64_t now);
static void siridb_send_invalid_query_error(uv_async_t * handle);

static void siridb_time_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        const uint64_t now,
        char ** buff);

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

    /* bind pid and client so we can send back the result */
    query->pid = pid;
    query->client = client;

    /* bind time precision */
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

siridb_q_select_t * siridb_new_select_query(void)
{
    siridb_q_select_t * q_select =
            (siridb_q_select_t *) malloc(sizeof(siridb_q_select_t));
    q_select->ct_series = ct_new();
    return q_select;
}

void siridb_free_select_query(siridb_q_select_t * q_select)
{
    ct_free(q_select->ct_series);
    free(q_select);
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
    qp_add_map1(query->packer);
    qp_add_raw(query->packer, "motd", 4);
    msg = motd_get_random_msg();
    qp_add_raw(query->packer, msg, strlen(msg));

    siridb_send_query_result(handle);
}

static void siridb_parse_query(uv_async_t * handle)
{
    siridb_query_t * query = (siridb_query_t *) handle->data;
    siridb_node_walker_t * walker = siridb_new_node_walker();

    uint64_t now = siridb_time_now(
            ((sirinet_handle_t *) query->client->data)->siridb,
            query->start);

    query->pr = cleri_parse(siri.grammar, query->q);

    if (!query->pr->is_valid)
        return siridb_send_invalid_query_error(handle);

    siridb_walk(
            query->pr->tree->children->node,
            walker,
            now);

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

static void siridb_walk(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        const uint64_t now)
{
    uint32_t gid;
    cleri_children_t * current;
    uv_async_cb func;

    if ((gid = node->cl_obj->cl_obj->dummy->gid))
    {
        if ((func = siridb_listen_enter[gid]) != NULL)
            siridb_append_enter_node(walker, node, func);
        if ((func = siridb_listen_exit[gid]) != NULL)
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
            siridb_walk(current->node, walker, now);
            current = current->next;
        }
    }
    else
    {
        /* we can have nested integer and time expressions */
        if (node->cl_obj->cl_obj->rule->gid == CLERI_GID_TIME_EXPR)
        {
            char buffer[1024];
            char * pt = buffer;
            siridb_time_expr(node, walker, now, &pt);
            *pt = 0;
            log_debug("Buffer: '%s'", buffer);
        }
    }
}

static void siridb_time_expr(
        cleri_node_t * node,
        siridb_node_walker_t * walker,
        const uint64_t now,
        char ** buff)
{
    cleri_children_t * current;

    switch (node->cl_obj->tp)
    {
    case CLERI_TP_TOKEN:
    case CLERI_TP_TOKENS:
        **buff = *node->str;
        (*buff)++;
        break;
    case CLERI_TP_KEYWORD:
        (*buff) += sprintf(*buff, "%ld", now);
        break;
    case CLERI_TP_REGEX:
        break;
    default:
        current = node->children;
        while (current != NULL && current->node != NULL)
        {
            siridb_time_expr(current->node, walker, now, buff);
            current = current->next;
        }
    }
}
