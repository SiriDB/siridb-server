/*
 * query.h - Responsible for parsing queries.
 */
#ifndef SIRIDB_QUERY_H_
#define SIRIDB_QUERY_H_

#define SIRIDB_QUERY_FLAG_MASTER 1
#define SIRIDB_QUERY_FLAG_REBUILD 2
#define SIRIDB_QUERY_FLAG_UPDATE_REPLICA 4
#define SIRIDB_QUERY_FLAG_ERR 8

/*
 * Note(*) : servers must be 'accessible' unless FLAG_ONLY_CHECK_ONLINE is used
 */
typedef enum
{
    SIRIDB_QUERY_FWD_SERVERS,       /* Forward to all 'online' servers      */
    SIRIDB_QUERY_FWD_POOLS,         /* Forward to all pools(*)              */
    SIRIDB_QUERY_FWD_SOME_POOLS,    /* Forward to some pools(*)             */
    SIRIDB_QUERY_FWD_UPDATE         /* Forward to all pools, upd repl(*)    */
} siridb_query_fwd_t;

typedef enum siridb_err_tp
{
    SIRIDB_SUCCESS,

    /* Server Errors 100 - 199 */
    SIRIDB_ERR_SHUTTINGDOWN=100,

    /* Query Errors 200 - 999 */
    SIRIDB_ERR_INVALID_QUERY=200

} siridb_err_t;

typedef struct siridb_query_s siridb_query_t;

#include <uv.h>
#include <inttypes.h>
#include <sys/time.h>
#include <cleri/cleri.h>
#include <qpack/qpack.h>
#include <siri/db/time.h>
#include <siri/db/nodes.h>
#include <siri/db/series.h>
#include <siri/db/db.h>
#include <siri/net/protocol.h>

void siridb_query_run(
        uint16_t pid,
        sirinet_stream_t * client,
        const char * q,
        size_t q_len,
        float factor,
        int flags);
void siridb_query_free(uv_handle_t * handle);
void siridb_send_query_result(uv_async_t * handle);
void siridb_query_send_error(
        uv_async_t * handle,
        cproto_server_t err);
void siridb_query_forward(
        uv_async_t * handle,
        siridb_query_fwd_t fwd,
        sirinet_promises_cb cb,
        int flags);
void siridb_query_timeit_from_unpacker(
        siridb_query_t * query,
        qp_unpacker_t * unpacker);
int siridb_query_err_from_pkg(siridb_query_t * query, sirinet_pkg_t * pkg);

struct siridb_query_s
{
    uv_close_cb free_cb;    /* must be on top */
    uint8_t ref;
    uint8_t flags;
    uint16_t pid;
    float factor;
    void * data;
    sirinet_stream_t * client;
    char * q;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    qp_packer_t * packer;
    qp_packer_t * timeit;
    cleri_parse_t * pr;
    siridb_nodes_t * nodes;
    struct timespec start;
};

#endif  /* SIRIDB_QUERY_H_ */
