/*
 * pools.c - Generate pools lookup.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-05-2016
 *
 */

#include <siri/db/pools.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <logger/logger.h>
#include <llist/llist.h>
#include <siri/net/promise.h>

static void POOLS_make(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup);

static void POOLS_max_pool(siridb_server_t * server, uint16_t * max_pool);
static void POOLS_arrange(siridb_server_t * server, siridb_t * siridb);

void siridb_pools_gen(siridb_t * siridb)
{
    if (siridb->pools != NULL)
    {
        siridb_pools_free(siridb->pools);
    }

    siridb->pools = (siridb_pools_t *) malloc(sizeof(siridb_pools_t));
    uint16_t max_pool = 0;
    uint16_t n;

#ifdef DEBUG
    /* we must have at least one server */
    assert (siridb->servers->len > 0);
#endif

    /* get max_pool (this can be used to get the number of pools) */
    llist_walk(siridb->servers, (llist_cb_t) POOLS_max_pool, &max_pool);

    /* set number of pools */
    siridb->pools->len = max_pool + 1;

    /* allocate memory for all pools */
    siridb->pools->pool = (siridb_pool_t *)
            malloc(sizeof(siridb_pool_t) * siridb->pools->len);

    /* initialize number of servers with zero for each pool */
    for (n = 0; n < siridb->pools->len; n++)
    {
        siridb->pools->pool[n].len = 0;
    }

    llist_walk(siridb->servers, (llist_cb_t) POOLS_arrange, siridb);

    /* generate pool lookup for series */
    siridb->pools->lookup = siridb_pools_gen_lookup(siridb->pools->len);
}

/*
 * Destroy pools. (parsing NULL is NOT allowed)
 */
void siridb_pools_free(siridb_pools_t * pools)
{
    free(pools->pool);
    free(pools->lookup);
    free(pools);
}

siridb_lookup_t * siridb_pools_gen_lookup(uint_fast16_t num_pools)
{
    siridb_lookup_t * lookup =
            (siridb_lookup_t *) calloc(1, sizeof(siridb_lookup_t));
    POOLS_make(1, num_pools, lookup);
    return lookup;
}

/*
 * Returns 1 (true) if at least one server in each pool is online, 0 (false)
 * if at least one pool has no server online. ('this' pool is NOT included)
 *
 * A server is considered  'online' when connected and authenticated.
 */
int siridb_pools_online(siridb_t * siridb)
{
    for (uint16_t pid = 0; pid < siridb->pools->len; pid++)
    {
        if (    pid != siridb->server->pool &&
                !siridb_pool_online(siridb->pools->pool + pid))
        {
            return 0;  // false
        }
    }
    return 1;  //true
}

/*
 * Returns 1 (true) if at least one server in each pool is available, 0 (false)
 * if at least one pool has no server available. ('this' pool is NOT included)
 *
 * A server is  'available' when and ONLY when connected and authenticated.
 */
int siridb_pools_available(siridb_t * siridb)
{
    for (uint16_t pid = 0; pid < siridb->pools->len; pid++)
    {
        if (    pid != siridb->server->pool &&
                !siridb_pool_available(siridb->pools->pool + pid))
        {
            return 0;  // false
        }
    }
    return 1;  //true
}

/*
 * This function will send a package to one available server in each pool,
 * 'this' pool not included. The promises call-back function should be
 * used to check if the package has been send successfully to all pools.
 *
 * This function can raise a SIGNAL when allocation errors occur.
 */
void siridb_pools_send_pkg(
        siridb_t * siridb,
        uint32_t len,
        uint16_t tp,
        const char * content,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data)
{
    sirinet_promises_t * promises =
            sirinet_promises_new(siridb->pools->len - 1, cb, data);

    if (promises != NULL)
    {
        siridb_pool_t * pool;

        for (uint16_t pid = 0; pid < siridb->pools->len; pid++)
        {
            if (pid == siridb->server->pool)
            {
                continue;
            }

            pool = siridb->pools->pool + pid;

            if (siridb_pool_send_pkg(
                    pool,
                    len,
                    tp,
                    content,
                    timeout,
                    sirinet_promise_on_response,
                    promises))
            {
                log_debug(
                        "Cannot send package to pool '%u' "
                        "(no available server found)",
                        pid);
                slist_append(promises->promises, NULL);
            }
        }

        SIRINET_PROMISES_CHECK(promises)
    }
}

static void POOLS_make(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup)
{
    if (n == num_pools)
    {
        return;
    }

    uint_fast16_t i;
    uint_fast16_t m;
    uint_fast16_t counters[n];
    for (i = 0; i < n; i++)
    {
        counters[i] = i;
    }

    m = n + 1;
    for (i = 0; i < SIRIDB_LOOKUP_SZ; i++)
    {
        counters[(*lookup)[i]]++;
        if (counters[(*lookup)[i]] % m == 0)
        {
            (*lookup)[i] = n;
        }
    }
    POOLS_make(m, num_pools, lookup);
}

static void POOLS_max_pool(siridb_server_t * server, uint16_t * max_pool)
{
    if (server->pool > *max_pool)
    {
        *max_pool = server->pool;
    }
}

static void POOLS_arrange(siridb_server_t * server, siridb_t * siridb)
{
    siridb_pool_t * pool;
    pool = siridb->pools->pool + server->pool;
    pool->len++;

    /* if the server is member of the same pool, it must be the replica */
    if (siridb->server != server && siridb->server->pool == server->pool)
    {
        siridb->replica = server;
    }

    if (pool->len == 1)
    {
        /* this is the first server found for this pool */
        pool->server[0] = server;
    }
    else
    {
#ifdef DEBUG
        /* we can only have 1 or 2 servers per pool */
        assert (pool->len == 2);
#endif
        /* add the server to the pool, ordered by UUID */
        if (siridb_server_cmp(pool->server[0], server) < 0)
        {
            pool->server[1] = server;
        }
        else
        {
#ifdef DEBUG
            assert(siridb_server_cmp(pool->server[0], server) > 0);
#endif
            pool->server[1] = pool->server[0];
            pool->server[0] = server;
        }
    }
}
