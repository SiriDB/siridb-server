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

#include <siri/net/promise.h>
#include <siri/db/server.h>
#include <siri/db/db.h>
#include <inttypes.h>
#include <cexpr/cexpr.h>
#include <siri/net/pkg.h>

typedef struct siridb_s siridb_t;
typedef struct siridb_server_s siridb_server_t;
typedef struct cexpr_condition_s cexpr_condition_t;
typedef struct sirinet_promise_s sirinet_promise_t;

typedef void (* sirinet_promise_cb)(
        sirinet_promise_t * promise,
        sirinet_pkg_t * pkg,
        int status);

typedef struct siridb_pool_s
{
    uint16_t len;
    siridb_server_t * server[2];
} siridb_pool_t;

typedef struct siridb_pool_walker_s
{
    uint_fast16_t pid;
    uint_fast8_t servers;
    size_t series;
} siridb_pool_walker_t;


uint16_t siridb_pool_sn(
        siridb_t * siridb,
        const char * sn);
uint16_t siridb_pool_sn_raw(
        siridb_t * siridb,
        const char * sn,
        size_t len);
int siridb_pool_cexpr_cb(siridb_pool_walker_t * wpool, cexpr_condition_t * cond);
int siridb_pool_online(siridb_pool_t * pool);
int siridb_pool_available(siridb_pool_t * pool);
int siridb_pool_send_pkg(
        siridb_pool_t * pool,
        sirinet_pkg_t * pkg,
        uint64_t timeout,
        sirinet_promise_cb cb,
        void * data);
void siridb_pool_add_server(siridb_pool_t * pool, siridb_server_t * server);
