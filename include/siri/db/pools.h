/*
 * pools.h - Collection of pools.
 */
#ifndef SIRIDB_POOLS_H_
#define SIRIDB_POOLS_H_

typedef struct siridb_pools_s siridb_pools_t;

#include <inttypes.h>
#include <siri/db/db.h>
#include <siri/db/pool.h>
#include <siri/db/server.h>
#include <siri/net/pkg.h>
#include <siri/net/promise.h>
#include <siri/net/promises.h>
#include <vec/vec.h>
#include <siri/db/lookup.h>

void siridb_pools_init(siridb_t * siridb);
void siridb_pools_free(siridb_pools_t * pools);
siridb_pool_t * siridb_pools_append(
        siridb_pools_t * pools,
        siridb_server_t * server);
siridb_lookup_t * siridb_pools_gen_lookup(uint_fast16_t num_pools);
int siridb_pools_online(siridb_t * siridb);
int siridb_pools_available(siridb_t * siridb);
int siridb_pools_accessible(siridb_t * siridb);
void siridb_pools_send_pkg(
        siridb_t * siridb,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data,
        int flags);
void siridb_pools_send_pkg_2some(
        vec_t * vec,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data,
        int flags);

struct siridb_pools_s
{
    uint16_t len;
    siridb_pool_t * pool;
    siridb_lookup_t * lookup;
    siridb_lookup_t * prev_lookup;
};

#endif  /* SIRIDB_POOLS_H_ */
