/*
 * query.h - Responsible for parsing queries.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#pragma once

#define SIRIDB_QUERY_FLAG_MASTER 1
#define SIRIDB_QUERY_FLAG_POOL 2
#define SIRIDB_QUERY_FLAG_REBUILD 4

#include <uv.h>
#include <inttypes.h>
#include <sys/time.h>
#include <cleri/parser.h>
#include <qpack/qpack.h>
#include <siri/db/time.h>
#include <siri/db/nodes.h>
#include <siri/db/series.h>
#include <siri/db/db.h>
#include <siri/net/protocol.h>

typedef struct cleri_parser_s cleri_parser_t;
typedef struct siridb_node_list_s siridb_node_list_t;

typedef enum siridb_err_tp
{
    SIRIDB_SUCCESS,

    /* Server Errors 100 - 199 */
    SIRIDB_ERR_SHUTTINGDOWN=100,

    /* Query Errors 200 - 999 */
    SIRIDB_ERR_INVALID_QUERY=200

} siridb_err_t;

typedef struct siridb_query_s
{
    void * data;
    uint64_t pid;
    uv_handle_t * client;
    char * q;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    qp_packer_t * packer;
    qp_packer_t * timeit;
    siridb_timep_t time_precision;
    uv_close_cb free_cb;
    cleri_parser_t * pr;
    siridb_nodes_t * nodes;
    struct timespec start;
    int flags;

} siridb_query_t;

void siridb_query_run(
        uint64_t pid,
        uv_handle_t * client,
        const char * q,
        size_t q_len,
        siridb_timep_t time_precision,
        int flags);

void siridb_query_free(uv_handle_t * handle);
void siridb_send_query_result(uv_async_t * handle);
void siridb_send_error(
        uv_async_t * handle,
        sirinet_msg_t err);

/* Can be used to forward a query to all pools or all servers.
 * Use tp = BP_QUERY_SERVER when the query needs to be send to all servers,
 * or BP_QUERY_POOL for sending the query to all pools.
 */
void siridb_query_forward(
        uv_async_t * handle,
        uint16_t tp,
        sirinet_promises_cb_t cb);

void siridb_query_timeit_from_unpacker(
        siridb_query_t * query,
        qp_unpacker_t * unpacker);
