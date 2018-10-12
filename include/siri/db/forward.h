/*
 * forward.h - Handle forwarding series while re-indexing.
 */
#ifndef SIRIDB_FORWARD_H_
#define SIRIDB_FORWARD_H_

typedef struct siridb_forward_s siridb_forward_t;

#include <inttypes.h>
#include <uv.h>
#include <siri/db/db.h>

siridb_forward_t * siridb_forward_new(siridb_t * siridb);
void siridb_forward_free(siridb_forward_t * forward);
void siridb_forward_points_to_pools(uv_async_t * handle);

struct siridb_forward_s
{
    uv_close_cb free_cb;    /* must be on top */
    uint8_t ref;
    uint16_t size; /* number of packers (one for each pool - 1) */
    siridb_t * siridb;
    qp_packer_t * packer[];
};

#endif  /* SIRIDB_FORWARD_H_ */
