/*
 * db.h - contains functions and constants for a SiriDB database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#pragma once

#include <siri/err.h>
#include <siri/db/time.h>
#include <siri/db/user.h>
#include <qpack/qpack.h>
#include <siri/db/server.h>
#include <siri/db/pools.h>
#include <ctree/ctree.h>
#include <imap32/imap32.h>
#include <imap64/imap64.h>
#include <iso8601/iso8601.h>
#include <llist/llist.h>
#include <string.h>
#include <uv.h>
#include <siri/db/fifo.h>
#include <siri/db/replicate.h>

#define SIRIDB_MAX_SIZE_ERR_MSG 1024
#define SIRIDB_MAX_DBNAME_LEN 256  // 255 + NULL

#define SIRIDB_GET_FN(FN, FILENAME)                         \
    char FN[strlen(siridb->dbpath) + strlen(FILENAME) + 1]; \
    sprintf(FN, "%s%s", siridb->dbpath, FILENAME);

/* Schema File Check
 * Needs fn and unpacker, free unpacker in case of an error.
 * Returns current function with -1 in case of an error and
 * raises a SIGNAL in case of a memory error.
 */
#define siridb_schema_check(SCHEMA)                             \
    /* read and check schema */                                 \
    qp_obj_t * qp_schema = qp_object_new();                     \
    if (qp_schema == NULL)                                      \
    {                                                           \
        ERR_ALLOC                                               \
        qp_unpacker_free(unpacker);                             \
        return -1;                                              \
    }                                                           \
    if (!qp_is_array(qp_next(unpacker, NULL)) ||                \
            qp_next(unpacker, qp_schema) != QP_INT64 ||         \
            qp_schema->via->int64 != SCHEMA)                    \
    {                                                           \
        log_critical("Invalid schema detected in '%s'", fn);    \
        qp_object_free(qp_schema);                              \
        qp_unpacker_free(unpacker);                             \
        return -1;                                              \
    }                                                           \
    /* finished schema check, free schema object */             \
    qp_object_free(qp_schema);

typedef struct siridb_time_s siridb_time_t;
typedef struct siridb_server_s siridb_server_t;
typedef struct siridb_users_s siridb_users_t;
typedef struct siridb_pools_s siridb_pools_t;
typedef struct ct_node_s ct_node_t;
typedef struct imap32_s imap32_t;
typedef struct imap64_s imap64_t;
typedef struct siridb_fifo_s siridb_fifo_t;
typedef struct siridb_replicate_s siridb_replicate_t;

typedef struct siridb_s
{
    uuid_t uuid;
    iso8601_tz_t tz;
    uint16_t shard_mask_num;
    uint16_t shard_mask_log;
    uint8_t ref;
    size_t buffer_size;
    size_t buffer_len;
    time_t start_ts;                  // in seconds, to calculate up-time.
    uint32_t max_series_id;
    uint64_t duration_num;              // number duration in s, ms, us or ns
    uint64_t duration_log;              // log duration in s, ms, us or ns
    char * dbname;
    char * dbpath;
    char * buffer_path;
    size_t index_size;
    size_t received_points;
    siridb_time_t * time;
    siridb_server_t * server;
    siridb_server_t * replica;
    llist_t * users;
    llist_t * servers;
    siridb_pools_t * pools;
    ct_t * series;
    imap32_t * series_map;
    uv_mutex_t series_mutex;
    uv_mutex_t shards_mutex;
    imap64_t * shards;
    FILE * buffer_fp;
    FILE * dropped_fp;
    qp_fpacker_t * store;
    siridb_fifo_t * fifo;
    siridb_replicate_t * replicate;
} siridb_t;


int siridb_from_unpacker(
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        char * err_msg);

siridb_t * siridb_get(llist_t * siridb_list, const char * dbname);
void siridb_free_cb(siridb_t * siridb, void * args);
void siridb_incref(siridb_t * siridb);
void siridb_decref(siridb_t * siridb);
int siridb_open_files(siridb_t * siridb);
