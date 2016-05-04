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
