/*
 * forward.h - Handle forwarding series while re-indexing
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 31-07-2016
 *
 */
#pragma once

#include <siri/db/db.h>
#include <inttypes.h>
#include <uv.h>

typedef struct siridb_s siridb_t;

typedef struct siridb_forward_s
{
    uv_close_cb free_cb;    /* must be on top */
    uint8_t ref;
    uint16_t size; /* number of packers (one for each pool - 1) */
    siridb_t * siridb;
    qp_packer_t * packer[];
} siridb_forward_t;


siridb_forward_t * siridb_forward_new(siridb_t * siridb);
void siridb_forward_free(siridb_forward_t * forward);
void siridb_forward_points_to_pools(uv_async_t * handle);
