/*
 * db.h - SiriDB database.
 */
#ifndef SIRIDB_H_
#define SIRIDB_H_

typedef struct siridb_s siridb_t;

#define SIRIDB_MAX_SIZE_ERR_MSG 1024
#define SIRIDB_MAX_DBNAME_LEN 256  /*    255 + NULL     */
#define SIRIDB_SCHEMA 4
#define SIRIDB_FLAG_REINDEXING 1

#define DEF_DROP_THRESHOLD 1.0              /* 100%         */
#define DEF_SELECT_POINTS_LIMIT 1000000     /* one million  */
#define DEF_LIST_LIMIT 10000                /* ten thousand */

#include <string.h>
#include <uv.h>
#include <qpack/qpack.h>
#include <ctree/ctree.h>
#include <imap/imap.h>
#include <imap/imap.h>
#include <iso8601/iso8601.h>
#include <llist/llist.h>
#include <siri/err.h>
#include <siri/db/time.h>
#include <siri/db/user.h>
#include <siri/db/server.h>
#include <siri/db/pools.h>
#include <siri/db/fifo.h>
#include <siri/db/replicate.h>
#include <siri/db/reindex.h>
#include <siri/db/groups.h>
#include <siri/db/tasks.h>
#include <siri/db/time.h>
#include <siri/db/buffer.h>

int32_t siridb_get_uptime(siridb_t * siridb);
int8_t siridb_get_idle_percentage(siridb_t * siridb);
int siridb_is_db_path(const char * dbpath);
siridb_t * siridb_new(const char * dbpath, int lock_flags);
siridb_t * siridb_get(llist_t * siridb_list, const char * dbname);
void siridb_decref_cb(siridb_t * siridb, void * args);
ssize_t siridb_get_file(char ** buffer, siridb_t * siridb);
int siridb_open_files(siridb_t * siridb);
int siridb_save(siridb_t * siridb);
void siridb__free(siridb_t * siridb);

#define siridb_incref(siridb) siridb->ref++
#define siridb_decref(_siridb) if (!--_siridb->ref) siridb__free(_siridb)
#define siridb_is_reindexing(siridb) (siridb->flags & SIRIDB_FLAG_REINDEXING)

struct siridb_s
{
    uint16_t ref;
    uint8_t flags;
    uint8_t pad0;
    uint32_t max_series_id;
    uint16_t insert_tasks;
    uint16_t shard_mask_num;
    uint16_t shard_mask_log;
    uint32_t select_points_limit;
    uint32_t list_limit;
    uuid_t uuid;
    iso8601_tz_t tz;
    struct timespec start_time;     /* to calculate up-time.                */
    uint64_t duration_num;          /* number duration in s, ms, us or ns   */
    uint64_t duration_log;          /* log duration in s, ms, us or ns      */
    char * dbname;
    char * dbpath;
    double drop_threshold;
    size_t received_points;
    size_t selected_points;

    siridb_time_t * time;
    siridb_server_t * server;
    siridb_server_t * replica;
    llist_t * users;
    llist_t * servers;
    siridb_pools_t * pools;
    ct_t * series;
    imap_t * series_map;
    uv_mutex_t series_mutex;
    uv_mutex_t shards_mutex;
    imap_t * shards;
    FILE * dropped_fp;
    qp_fpacker_t * store;
    siridb_fifo_t * fifo;
    siridb_replicate_t * replicate;
    siridb_reindex_t * reindex;
    siridb_groups_t * groups;
    siridb_buffer_t * buffer;
    siridb_tasks_t tasks;
};

#endif  /* SIRIDB_H_ */
