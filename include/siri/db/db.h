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

#include <siri/db/time.h>
#include <siri/db/user.h>
#include <qpack/qpack.h>
#include <siri/db/server.h>
#include <siri/db/pool.h>
#include <ctree/ctree.h>
#include <imap32/imap32.h>

#define SIRIDB_MAX_DBNAME_LEN 256  // 255 + NULL

#define siridb_get_fn(FN, FILENAME)                         \
    char FN[strlen(siridb->dbpath) + strlen(FILENAME) + 1]; \
    sprintf(FN, "%s%s", siridb->dbpath, FILENAME);

struct siridb_server_s;
struct siridb_users_s;
struct siridb_servers_s;
struct siridb_pools_s;
struct ct_node_s;
struct imap32_s;

typedef struct siridb_s
{
    uuid_t uuid;
    char * dbname;
    char * dbpath;
    char * buffer_path;
    int time_precision;
    uint32_t start_ts;                  // in seconds, to calculate up-time.
    struct siridb_server_s * server;
    struct siridb_server_s * replica;
    struct siridb_users_s * users;
    struct siridb_servers_s * servers;
    struct siridb_pools_s * pools;
    uint32_t max_series_id;
    struct ct_node_s * series;
    struct imap32_s * series_map;
    size_t buffer_size;
    FILE * buffer_fp;
} siridb_t;

typedef struct siridb_list_s
{
    siridb_t * siridb;
    struct siridb_list_s * next;
} siridb_list_t;


siridb_list_t * siridb_new_list(void);
void siridb_free_list(siridb_list_t * siridb_list);


int siridb_add_from_unpacker(
        siridb_list_t * siridb_list,
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        char * err_msg);

siridb_t * siridb_get(siridb_list_t * siridb_list, const char * dbname);
