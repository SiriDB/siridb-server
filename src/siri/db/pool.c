/*
 * pool.c - SiriDB pool containing one or two servers.
 */
#include <assert.h>
#include <logger/logger.h>
#include <siri/db/pool.h>
#include <siri/db/pools.h>
#include <siri/grammar/grammar.h>
#include <stdlib.h>
#include <string.h>
#include <siri/db/server.h>


/*
 * Returns 1 (true) if at least one server in the pool is online, 0 (false)
 * if no server in the pool is online.
 *
 * Warning: this function should not be used on 'this' pool.
 *
 * Server is 'online' when at least running and authenticated but not
 * queue-full
 */
int siridb_pool_online(siridb_pool_t * pool)
{
    uint16_t i;
    for (i = 0; i < pool->len; i++)
    {
        if (siridb_server_is_online(pool->server[i]))
        {
            return 1;  /* true  */
        }
    }
    return 0;  /* false  */
}

/*
 * Returns 1 (true) if at least one server in the pool is available, 0 (false)
 * if no server in the pool is available.
 *
 * Warning: this function should not be used on 'this' pool.
 *
 * A server is  'available' when and ONLY when connected and authenticated.
 */
int siridb_pool_available(siridb_pool_t * pool)
{
    uint16_t i;
    for (i = 0; i < pool->len; i++)
    {
        if (siridb_server_is_available(pool->server[i]))
        {
            return 1;  /* true  */
        }
    }
    return 0;  /* false  */
}

/*
 * Returns 1 (true) if at least one server in the pool is accessible, 0 (false)
 * if no server in the pool is accessible.
 *
 * Warning: this function should not be used on 'this' pool.
 *
 * A server is  'accessible' when exactly 'available' or 're-indexing'.
 */
int siridb_pool_accessible(siridb_pool_t * pool)
{
    uint16_t i;
    for (i = 0; i < pool->len; i++)
    {
        if (siridb_server_is_accessible(pool->server[i]))
        {
            return 1;  /* true  */
        }
    }
    return 0;  /* false  */
}


/*
 * Call-back function used to validate pools in a where expression.
 *
 * Returns 0 or 1 (false or true).
 */
int siridb_pool_cexpr_cb(
        siridb_pool_walker_t * wpool,
        cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(cond->operator, wpool->pool, cond->int64);
    case CLERI_GID_K_SERVERS:
        return cexpr_int_cmp(cond->operator, wpool->servers, cond->int64);
    case CLERI_GID_K_SERIES:
        return cexpr_int_cmp(cond->operator, wpool->series, cond->int64);
    }
    /* we must NEVER get here */
    log_critical("Unexpected pool property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * Add a server object to a pool.
 */
void siridb_pool_add_server(siridb_pool_t * pool, siridb_server_t * server)
{
    pool->len++;

    if (pool->len == 1)
    {
        /* this is the first server found for this pool */
        pool->server[0] = server;
        pool->server[0]->id = 0;
    }
    else
    {
        /* we can only have 1 or 2 servers per pool */
        assert (pool->len == 2);
        /* add the server to the pool, ordered by UUID */
        if (siridb_server_cmp(pool->server[0], server) < 0)
        {
            pool->server[1] = server;
            pool->server[1]->id = 1;
        }
        else
        {
            assert (siridb_server_cmp(pool->server[0], server) > 0);
            pool->server[1] = pool->server[0];
            pool->server[0] = server;

            /* set server id */
            pool->server[0]->id = 0;
            pool->server[1]->id = 1;
        }
    }
}

/*
 * Returns 0 if we have send the package to one 'accessible'
 * server in the pool. The package will be send only to one server, even if
 * the pool has more servers 're-indexing'.
 *
 * This function can raise a SIGNAL when allocation errors occur but be aware
 * that 0 can still be returned in this case.
 *
 * The call-back function is not called when -1 is returned.
 *
 * A server is  're-indexing' when exactly running and authenticated and
 * optionally re-indexing.
 *
 * Note that 'pkg->pid' will be overwritten with a new package id.
 *
 * In case flag 'FLAG_ONLY_CHECK_ONLINE' is set, we do not check 'accessible'
 * but only 'online' is enough.
 *
 * pkg will be destroyed when and ONLY when 0 is returned. (Except when
 * FLAG_KEEP_PKG is set)
 */
int siridb_pool_send_pkg(
        siridb_pool_t * pool,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data,
        int flags)
{
    siridb_server_t * server = NULL;
    uint16_t i;

    for (i = 0; i < pool->len; i++)
    {
        if ((flags & FLAG_ONLY_CHECK_ONLINE) ?
                siridb_server_is_online(pool->server[i]) :
                siridb_server_is_accessible(pool->server[i]))
        {
            server = (server == NULL) ?
                    pool->server[i] : pool->server[rand() % 2];
        }
    }

    return (server == NULL) ?
            -1: siridb_server_send_pkg(server, pkg, timeout, cb, data, flags);
}
