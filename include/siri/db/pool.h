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

typedef struct siridb_s siridb_t;
typedef struct siridb_server_s siridb_server_t;

typedef struct siridb_pool_s
{
    uint16_t size;
    siridb_server_t * server[2];
} siridb_pool_t;

/* lookup by 0 terminated string */
uint16_t siridb_pool_sn(
        siridb_t * siridb,
        const char * sn);

/* lookup by raw string */
uint16_t siridb_pool_sn_raw(
        siridb_t * siridb,
        const char * sn,
        size_t len);
