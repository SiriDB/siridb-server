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
#include <siri/db/pools.h>
#include <stdlib.h>
#include <string.h>
#include <logger/logger.h>
#include <siri/grammar/grammar.h>
#include <assert.h>

uint16_t siridb_pool_sn(
        siridb_t * siridb,
        const char * sn)
{
    uint32_t n = 0;
    for (; *sn; sn++)
    {
        n += *sn;
    }
    return (*siridb->pools->lookup)[n % SIRIDB_LOOKUP_SZ];
}

uint16_t siridb_pool_sn_raw(
        siridb_t * siridb,
        const char * sn,
        size_t len)
{
    uint32_t n = 0;
    while (len--)
    {
        n += sn[len];
    }
    return (*siridb->pools->lookup)[n % SIRIDB_LOOKUP_SZ];
}

int siridb_pool_cexpr_cb(siridb_pool_walker_t * wpool, cexpr_condition_t * cond)
{
    switch (cond->prop)
    {
    case CLERI_GID_K_POOL:
        return cexpr_int_cmp(cond->operator, wpool->pid, cond->int64);
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
