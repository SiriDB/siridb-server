/*
 * pool.c - Generate pool lookup.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 25-03-2016
 *
 */

#include <siri/db/pool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <logger/logger.h>

static void make_pools(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup);

void siridb_gen_pools(siridb_t * siridb)
{
    siridb_free_pools(siridb->pools);

    siridb->pools = (siridb_pools_t *) malloc(sizeof(siridb_pools_t));
    uint16_t max_pool = 0;
    uint16_t n;
    siridb_servers_t * servers;
    siridb_pool_t * pool;

    /* we must have at least one server */
    assert(siridb->servers->server != NULL);

    /* get max_pool (this can be used to get the number of pools) */
    for (servers = siridb->servers; servers != NULL; servers = servers->next)
        if (servers->server->pool > max_pool)
            max_pool = servers->server->pool;

    /* set number of pools */
    siridb->pools->size = max_pool + 1;

    /* allocate memory for all pools */
    siridb->pools->pool = (siridb_pool_t *)
            malloc(sizeof(siridb_pool_t) * siridb->pools->size);

    /* initialize number of servers with zero for each pool */
    for (n = 0; n < siridb->pools->size; n++)
        siridb->pools->pool[n].size = 0;

    for (servers = siridb->servers; servers != NULL; servers = servers->next)
    {
        pool = siridb->pools->pool + servers->server->pool;
        pool->size++;
        if (pool->size == 1)
        {
            /* this is the first server found for this pool */
            pool->server[0] = servers->server;
        }
        else
        {
            /* we can only have 1 or 2 servers per pool */
            assert (pool->size == 2);

            /* add the server to the pool, ordered by UUID */
            if (siridb_server_cmp(pool->server[0], servers->server) < 0)
                pool->server[1] = servers->server;
            else
            {
                assert(siridb_server_cmp(
                        pool->server[0], servers->server) > 0);
                pool->server[1] = pool->server[0];
                pool->server[0] = servers->server;
            }
        }
    }

    /* generate pool lookup for series */
    siridb->pools->lookup = siridb_gen_lookup(siridb->pools->size);
}

void siridb_free_pools(siridb_pools_t * pools)
{
    if (pools == NULL)
        return;
    free(pools->pool);
    free(pools->lookup);
    free(pools);
}

uint16_t siridb_pool_sn(
        siridb_t * siridb,
        const char * sn)
{
    uint32_t n = 0;
    for (; *sn; sn++)
        n += *sn;
    return (*siridb->pools->lookup)[n % SIRIDB_LOOKUP_SZ];
}

uint16_t siridb_pool_sn_raw(
        siridb_t * siridb,
        const char * sn,
        size_t len)
{
    uint32_t n = 0;
    while (len--)
        n += sn[len];
    return (*siridb->pools->lookup)[n % SIRIDB_LOOKUP_SZ];
}

siridb_lookup_t * siridb_gen_lookup(uint_fast16_t num_pools)
{
    siridb_lookup_t * lookup =
            (siridb_lookup_t *) calloc(1, sizeof(siridb_lookup_t));
    make_pools(1, num_pools, lookup);
    return lookup;
}

static void make_pools(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup)
{
    uint_fast16_t i;
    uint_fast16_t m;
    if (n == num_pools)
        return;

    uint_fast16_t counters[n];
    for (i = 0; i < n; i++)
        counters[i] = i;

    m = n + 1;
    for (i = 0; i < SIRIDB_LOOKUP_SZ; i++)
    {
        counters[(*lookup)[i]]++;
        if (counters[(*lookup)[i]] % m == 0)
            (*lookup)[i] = n;
    }
    make_pools(m, num_pools, lookup);
}
