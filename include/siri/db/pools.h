/*
 * pools.h - Generate pools lookup.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 25-03-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <inttypes.h>
#include <siri/db/pool.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_pool_s siridb_pool_t;

#define SIRIDB_LOOKUP_SZ 8192

typedef uint_fast16_t siridb_lookup_t[SIRIDB_LOOKUP_SZ];

typedef struct siridb_pools_s
{
    uint16_t size;
    siridb_lookup_t * lookup;
    siridb_pool_t * pool;
} siridb_pools_t;

void siridb_pools_gen(siridb_t * siridb);
void siridb_pools_free(siridb_pools_t * pools);
siridb_lookup_t * siridb_pools_gen_lookup(uint_fast16_t num_pools);
