/*
 * insert.h - Handler database inserts.
 */
#ifndef SIRIDB_INSERT_H_
#define SIRIDB_INSERT_H_

#define INSERT_FLAG_TEST 1
#define INSERT_FLAG_TESTED 2
#define INSERT_FLAG_POOL 4
#define INSERT_FLAG_INIT_REPL 8

typedef enum
{
    ERR_EXPECTING_ARRAY=-10,
    ERR_EXPECTING_SERIES_NAME,
    ERR_EXPECTING_MAP_OR_ARRAY,
    ERR_EXPECTING_INTEGER_TS,
    ERR_TIMESTAMP_OUT_OF_RANGE,
    ERR_UNSUPPORTED_VALUE,
    ERR_EXPECTING_AT_LEAST_ONE_POINT,
    ERR_EXPECTING_NAME_AND_POINTS,
    ERR_INCOMPATIBLE_SERVER_VERSION,
    ERR_MEM_ALLOC,          /* This is a critical error.        */
} siridb_insert_err_t;

enum
{
    INSERT_LOCAL_CANCELLED=-2,
    INSERT_LOCAL_ERROR,
    INSERT_LOCAL_SUCESS,
};

typedef struct siridb_insert_s siridb_insert_t;
typedef struct siridb_insert_local_s siridb_insert_local_t;

#include <siri/db/db.h>
#include <qpack/qpack.h>
#include <siri/db/forward.h>
#include <uv.h>
#include <siri/db/pcache.h>

ssize_t siridb_insert_assign_pools(
        siridb_t * siridb,
        qp_unpacker_t * unpacker,
        qp_packer_t * packer[]);
const char * siridb_insert_err_msg(siridb_insert_err_t err);
siridb_insert_t * siridb_insert_new(
        siridb_t * siridb,
        uint16_t pid,
        sirinet_stream_t * client);
void siridb_insert_free(siridb_insert_t * insert);
int siridb_insert_points_to_pools(siridb_insert_t * insert, size_t npoints);
int insert_init_backend_local(
        siridb_t * siridb,
        sirinet_stream_t * client,
        sirinet_pkg_t * pkg,
        uint8_t flags);

struct siridb_insert_s
{
    uv_close_cb free_cb;    /* must be on top */
    uint8_t ref;
    uint8_t flags;
    uint16_t pid;
    sirinet_stream_t * client;
    size_t npoints;        /* number of points */
    uint16_t packer_size; /* number of packers (one for each pool) */
    qp_packer_t * packer[];
};

struct siridb_insert_local_s
{
    uv_close_cb free_cb;    /* must be on top */
    uint8_t ref;
    uint8_t flags;
    int8_t status;
    qp_unpacker_t unpacker;
    qp_obj_t qp_series_name;
    siridb_t * siridb;
    sirinet_promise_t * promise;
    siridb_forward_t * forward;
    siridb_pcache_t * pcache;
};

#endif  /* SIRIDB_INSERT_H_ */
