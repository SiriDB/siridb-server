/*
 * pool.h - Generate pool lookup.
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

#include <siri/db/server.h>
#include <siri/db/db.h>
#include <inttypes.h>

struct siridb_s;
struct siridb_server_s;

#define SIRIDB_LOOKUP_SZ 8192

typedef uint_fast16_t siridb_lookup_t[SIRIDB_LOOKUP_SZ];

typedef struct siridb_pool_s
{
    uint16_t size;
    struct siridb_server_s * server[2];
} siridb_pool_t;

typedef struct siridb_pools_s
{
    uint16_t size;
    siridb_lookup_t * lookup;
    siridb_pool_t * pool;
} siridb_pools_t;

siridb_lookup_t * siridb_gen_lookup(uint_fast16_t num_pools);
void siridb_gen_pools(struct siridb_s * siridb);
void siridb_free_pools(siridb_pools_t * pools);

/* lookup by 0 terminated string */
uint16_t siridb_pool_sn(
        struct siridb_s * siridb,
        const char * sn);

/* lookup by raw string */
uint16_t siridb_pool_sn_raw(
        struct siridb_s * siridb,
        const char * sn,
        size_t len);
