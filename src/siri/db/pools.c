/*
 * pools.c - Collection of pools.
 */
#include <assert.h>
#include <llist/llist.h>
#include <logger/logger.h>
#include <siri/db/pools.h>
#include <siri/db/server.h>
#include <siri/net/promises.h>
#include <siri/optimize.h>
#include <stdlib.h>
#include <string.h>
#include <siri/err.h>

static void POOLS_max_pool(siridb_server_t * server, uint16_t * max_pool);
static void POOLS_arrange(siridb_server_t * server, siridb_t * siridb);

/*
 * This function can raise an ALLOC signal.
 */
void siridb_pools_init(siridb_t * siridb)
{
    assert (siridb->pools == NULL);
    assert (siridb->servers != NULL && siridb->servers->len > 0);
    assert (siridb->server != NULL);

    siridb->pools = malloc(sizeof(siridb_pools_t));
    if (siridb->pools == NULL)
    {
        ERR_ALLOC
        return;
    }
    uint16_t max_pool = 0;
    uint16_t n;

    /* get max_pool (this can be used to get the number of pools) */
    llist_walk(siridb->servers, (llist_cb) POOLS_max_pool, &max_pool);

    /* set number of pools */
    siridb->pools->len = max_pool + 1;

    /* allocate memory for all pools */
    siridb->pools->pool = malloc(sizeof(siridb_pool_t) * siridb->pools->len);

    if (siridb->pools->pool == NULL)
    {
        ERR_ALLOC
        free(siridb->pools);
        siridb->pools = NULL;
        return;
    }

    /* initialize number of servers with zero for each pool */
    for (n = 0; n < siridb->pools->len; n++)
    {
        siridb->pools->pool[n].len = 0;
    }

    /* signal can be raised if creating a fifo buffer fails */
    llist_walk(siridb->servers, (llist_cb) POOLS_arrange, siridb);

    siridb->pools->prev_lookup = NULL;

    /* generate pool lookup for series */
    siridb->pools->lookup = siridb_lookup_new(siridb->pools->len);
    if (siridb->pools->lookup == NULL)
    {
        siridb_pools_free(siridb->pools);
        siridb->pools = NULL;
        /* signal is raised */
    }
}

/*
 * Destroy pools. (parsing NULL is NOT allowed)
 */
void siridb_pools_free(siridb_pools_t * pools)
{
    free(pools->pool);
    siridb_lookup_free(pools->lookup);
    siridb_lookup_free(pools->prev_lookup);
    free(pools);
}

/*
 * Append a new pool to pools and returns the new created pool object.
 *
 * Note: pools->prev_lookup is set to the previous lookup table and a new
 *       lookup is created and set to pools->lookup.
 *
 * Returns NULL and raises a SIGNAL in case an error has occurred and pools
 * remains unchanged in this case.
 */
siridb_pool_t * siridb_pools_append(
        siridb_pools_t * pools,
        siridb_server_t * server)
{
    siridb_pool_t * pool = NULL;
    siridb_lookup_t * lookup = siridb_lookup_new(pools->len + 1);
    if (lookup != NULL)
    {
        pool = (siridb_pool_t *)
                realloc(pools->pool, sizeof(siridb_pool_t) * (pools->len + 1));
        if (pool == NULL)
        {
            ERR_ALLOC
            siridb_lookup_free(lookup);
        }
        else
        {
            pools->pool = pool;
            pool = &pools->pool[pools->len];
            pool->len = 0;
            siridb_pool_add_server(pool, server);
            pools->len++;

            assert (pools->prev_lookup == NULL);

            pools->prev_lookup = pools->lookup;
            pools->lookup = lookup;
        }
    }
    return pool;
}


/*
 * Returns 1 (true) if at least one server in each pool is online, 0 (false)
 * if at least one pool has no server online. ('this' pool is NOT included)
 *
 * A server is considered  'online' when connected and authenticated.
 */
int siridb_pools_online(siridb_t * siridb)
{
    uint16_t pid ;
    for (pid = 0; pid < siridb->pools->len; pid++)
    {
        if (    pid != siridb->server->pool &&
                !siridb_pool_online(siridb->pools->pool + pid))
        {
            return 0;  /* false  */
        }
    }
    return 1;  /* true  */
}

/*
 * Returns 1 (true) if at least one server in each pool is available, 0 (false)
 * if at least one pool has no server available. ('this' pool is NOT included)
 *
 * A server is  'available' when and ONLY when connected and authenticated.
 */
int siridb_pools_available(siridb_t * siridb)
{
    uint16_t pid;
    for (pid = 0; pid < siridb->pools->len; pid++)
    {
        if (    pid != siridb->server->pool &&
                !siridb_pool_available(siridb->pools->pool + pid))
        {
            return 0;  /* false  */
        }
    }
    return 1;  /* true  */
}


/*
 * Returns 1 (true) if at least one server in each pool is accessible, 0 (false)
 * if at least one pool has no server accessible. ('this' pool is NOT included)
 *
 * A server is  'accessible' when exactly 'available' or 're-indexing'.
 */
int siridb_pools_accessible(siridb_t * siridb)
{
    uint16_t pid;
    for (pid = 0; pid < siridb->pools->len; pid++)
    {
        if (    pid != siridb->server->pool &&
                !siridb_pool_accessible(siridb->pools->pool + pid))
        {
            return 0;  /* false  */
        }
    }
    return 1;  /* true  */
}

/*
 * This function will send a package to one accessible server in each pool,
 * 'this' pool not included. The promises call-back function should be
 * used to check if the package has been send successfully to all pools.
 *
 * This function can raise a SIGNAL when allocation errors occur.
 *
 * If promises could not be created, the 'cb' function with cb(NULL, data)
 *
 * Note that 'pkg->pid' will be overwritten with a new package id and pkg
 * will always be destroyed.
 */
void siridb_pools_send_pkg(
        siridb_t * siridb,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data,
        int flags)
{
    sirinet_promises_t * promises =
            sirinet_promises_new(siridb->pools->len - 1, cb, data, pkg);

    if (promises == NULL)
    {
        free(pkg);
        cb(NULL, data);
    }
    else
    {
        siridb_pool_t * pool;
        uint16_t pid;

        for (pid = 0; pid < siridb->pools->len; pid++)
        {
            if (pid == siridb->server->pool)
            {
                continue;
            }

            pool = siridb->pools->pool + pid;

            if (siridb_pool_send_pkg(
                    pool,
                    pkg,
                    timeout,
                    (sirinet_promise_cb) sirinet_promises_on_response,
                    promises,
                    FLAG_KEEP_PKG | flags))
            {
                log_debug(
                        "Cannot send package to pool '%u' "
                        "(no accessible server found)",
                        pid);
                vec_append(promises->promises, NULL);
            }
        }

        SIRINET_PROMISES_CHECK(promises)
    }
}

/*
 * This function will send a package to one accessible server in the pools
 * provided by the 'vec'. The promises call-back function should be
 * used to check if the package has been send successfully to all pools.
 *
 * The 'vec' can be destroyed after this function has returned.
 *
 * This function can raise a SIGNAL when allocation errors occur.
 *
 * If promises could not be created, the 'cb' function with cb(NULL, data)
 *
 * Note that 'pkg->pid' will be overwritten with a new package id and pkg
 * will always be destroyed.
 */
void siridb_pools_send_pkg_2some(
        vec_t * vec,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promises_cb cb,
        void * data,
        int flags)
{
    sirinet_promises_t * promises =
            sirinet_promises_new(vec->len, cb, data, pkg);

    if (promises == NULL)
    {
        free(pkg);
        cb(NULL, data);
    }
    else
    {
        siridb_pool_t * pool;
        size_t i;

        for (i = 0; i < vec->len; i++)
        {
            pool = vec->data[i];

            if (siridb_pool_send_pkg(
                    pool,
                    pkg,
                    timeout,
                    (sirinet_promise_cb) sirinet_promises_on_response,
                    promises,
                    FLAG_KEEP_PKG | flags))
            {
                log_debug(
                        "Cannot send package to at least on pool "
                        "(no accessible server found)");
                vec_append(promises->promises, NULL);
            }
        }

        SIRINET_PROMISES_CHECK(promises)
    }
}


static void POOLS_max_pool(siridb_server_t * server, uint16_t * max_pool)
{
    if (server->pool > *max_pool)
    {
        *max_pool = server->pool;
    }
}

/*
 * Signal can be raised by this function when a fifo buffer for an optional
 * replica server can't be created.
 */
static void POOLS_arrange(siridb_server_t * server, siridb_t * siridb)
{
    siridb_pool_t * pool = siridb->pools->pool + server->pool;

    /* if the server is member of the same pool, it must be the replica */
    if (siridb->server != server && siridb->server->pool == server->pool)
    {
        siridb->replica = server;

        /* set synchronize flag */
        siridb->server->flags |= SERVER_FLAG_SYNCHRONIZING;

        /* pause optimize task while synchronize flag is set */
        siri_optimize_pause();

        /* initialize replica */
        siridb->fifo = siridb_fifo_new(siridb);

        if (siridb->fifo == NULL)
        {
            log_critical("Cannot initialize fifo buffer for replica server");
            /* signal is set */
        }
        else
        {
            /* signal can be raised by 'siridb_replicate_init' */
            siridb_replicate_init(
                    siridb,
                    siridb_initsync_open(siridb, 0));
        }
    }

    siridb_pool_add_server(pool, server);
}
