/*
 * pool.h - SiriDB pool containing one or two servers.
 */
#ifndef SIRIDB_POOL_H_
#define SIRIDB_POOL_H_

typedef struct siridb_pool_s siridb_pool_t;
typedef struct siridb_pool_walker_s siridb_pool_walker_t;

#include <cexpr/cexpr.h>
#include <inttypes.h>
#include <siri/db/db.h>
#include <siri/db/pools.h>
#include <siri/db/server.h>
#include <siri/net/pkg.h>
#include <siri/net/promise.h>

int siridb_pool_cexpr_cb(siridb_pool_walker_t * wpool, cexpr_condition_t * cond);
int siridb_pool_online(siridb_pool_t * pool);
int siridb_pool_available(siridb_pool_t * pool);
int siridb_pool_accessible(siridb_pool_t * pool);
int siridb_pool_send_pkg(
        siridb_pool_t * pool,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data,
        int flags);
void siridb_pool_add_server(siridb_pool_t * pool, siridb_server_t * server);


struct siridb_pool_s
{
    uint16_t len;
    siridb_server_t * server[2];
};

struct siridb_pool_walker_s
{
    uint_fast16_t pool;
    uint_fast8_t servers;
    size_t series;
};

#endif  /* SIRIDB_POOL_H_ */
