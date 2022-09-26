/*
 * db.c - SiriDB database.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <cfgparser/cfgparser.h>
#include <lock/lock.h>
#include <lock/lock.h>
#include <logger/logger.h>
#include <math.h>
#include <procinfo/procinfo.h>
#include <siri/db/db.h>
#include <siri/db/misc.h>
#include <siri/db/series.h>
#include <siri/db/servers.h>
#include <siri/db/shard.h>
#include <siri/db/shards.h>
#include <siri/db/time.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <siri/siri.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>
#include <xpath/xpath.h>
#include <vec/vec.h>
#include <timeit/timeit.h>

/*
 * database.dat
 *
 * SCHEMA       -> SIRIDB_SHEMA
 * UUID         -> UUID for 'this' server
 * DBNAME       -> Database name
 *
 *
 */

static siridb_t * siridb__new(void);
static int siridb__from_unpacker(
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        const char * dbpath,
        char * err_msg);
static siridb_t * siridb__from_dat(const char * dbpath);
static int siridb__read_conf(siridb_t * siridb);
static int siridb__lock(const char * dbpath, int lock_flags);

#define READ_DB_EXIT_WITH_ERROR(ERROR_MSG)  \
    strcpy(err_msg, ERROR_MSG);             \
    siridb__free(*siridb);                  \
    *siridb = NULL;                         \
    return -1;

/*
 * Returns the up-time in seconds.
 */
int32_t siridb_get_uptime(siridb_t * siridb)
{
    return (int32_t) timeit_get(&siridb->start_time);
}

/*
 * Returns a value in the range 0 and 100 representing how much percent sine
 * up-time is idle time.
 */
int8_t siridb_get_idle_percentage(siridb_t * siridb)
{
    double uptime = (double) siridb_get_uptime(siridb);
    int8_t idle = (uptime)
            ? (int8_t) round(siridb->tasks.idle_time / uptime * 100.0f)
            : 0;
    /* idle time can technically be larger since we start a database before we
     * mark the server as started.
     */
    return (idle > 100) ? 100 : idle;
}

/*
 * Check if at least database.conf and database.dat exist in the path.
 */
int siridb_is_db_path(const char * dbpath)
{
    char buffer[XPATH_MAX];
    buffer[XPATH_MAX-1] = '\0';
    snprintf(buffer,
            XPATH_MAX-1,
            "%sdatabase.conf",
            dbpath);
    if (!xpath_file_exist(buffer))
    {
        return 0;  /* false */
    }
    snprintf(buffer,
            XPATH_MAX-1,
            "%sdatabase.dat",
            dbpath);
    if (!xpath_file_exist(buffer))
    {
        return 0;  /* false */
    }
    return 1;  /* true */
}

/*
 * Returns a siridb object or NULL in case of an error.
 *
 * (lock_flags are simple parsed to the lock function)
 *
 */
siridb_t * siridb_new(const char * dbpath, int lock_flags)
{
    size_t len = strlen(dbpath);
    siridb_t * siridb;
    size_t i;

    if (!len || dbpath[len - 1] != '/')
    {
        log_error("Database path should end with a slash. (got: '%s')",
                dbpath);
        return NULL;
    }

    if (!xpath_is_dir(dbpath))
    {
        log_error("Cannot find database path '%s'", dbpath);
        return NULL;
    }

    if (siridb__lock(dbpath, lock_flags))
    {
        log_error("Cannot lock database path '%s'", dbpath);
        return NULL;
    }

    siridb = siridb__from_dat(dbpath);
    if (siridb == NULL)
    {
        log_error("Cannot load SiriDB from database path '%s'", dbpath);
        return NULL;
    }

    log_info("Start loading database: '%s'", siridb->dbname);

    /* read database.conf */
    if (siridb__read_conf(siridb))
    {
        log_error("Could not read config for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load users */
    if (siridb_users_load(siridb))
    {
        log_error("Could not read users for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load servers */
    if (siridb_servers_load(siridb))
    {
        log_error("Could not read servers for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load series */
    if (siridb_series_load(siridb))
    {
        log_error("Could not read series for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* test buffer path */
    if (siridb_buffer_test_path(siridb))
    {
        log_error("Cannot read buffer for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load shards */
    if (siridb_shards_load(siridb))
    {
        log_error("Could not read shards for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load buffer */
    if (siridb_buffer_load(siridb))
    {
        log_error("Could not read buffer for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* open buffer */
    if (siridb_buffer_open(siridb->buffer))
    {
        log_error("Could not open buffer for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load groups */
    if (siridb_groups_init(siridb))
    {
        log_error("Cannot read groups for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* load tags */
    if (siridb_tags_init(siridb))
    {
        log_error("Cannot read tags for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* update series props */
    log_info("Updating series properties");

    /* create a copy since 'siridb_series_update_props' might drop a series */
    vec_t * vec = imap_2vec(siridb->series_map);

    if (vec == NULL)
    {
        log_error("Could update series properties for database '%s'",
                siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    for (i = 0; i < vec->len; i++)
    {
        siridb_series_update_props(siridb, (siridb_series_t * )vec->data[i]);
    }

    vec_free(vec);

    /* generate pools, this can raise a signal */
    log_info("Initialize pools");
    siridb_pools_init(siridb);

    /* set "tee" Id */
    siridb_tee_set_id(siridb);

    if (!siri_err)
    {
        siridb->reindex = siridb_reindex_open(siridb, 0);
        if (siridb->reindex != NULL && siridb->replica == NULL)
        {
            siridb_reindex_start(siridb->reindex->timer);
        }
    }

    timeit_start(&siridb->start_time);

    uv_mutex_lock(&siri.siridb_mutex);

    /* append SiriDB to siridb_list (reference counter is already 1) */
    if (llist_append(siri.siridb_list, siridb))
    {
        siridb_decref(siridb);
        return NULL;
    }

    uv_mutex_unlock(&siri.siridb_mutex);

    /* start groups update thread */
    siridb_groups_start(siridb);

    /* start tasks */
    siridb_tasks_init(&siridb->tasks);

    log_info("Finished loading database: '%s'", siridb->dbname);

    return siridb;
}

/*
 * Read SiriDB from unpacker. (reference counter is initially set to 1)
 *
 * Returns 0 if successful or a value greater than 0 if the file needs to be
 * saved. In case of an error -1 will be returned.
 *
 * (a SIGNAL can be set in case of an error)
 */
static int siridb__from_unpacker(
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        const char * dbpath,
        char * err_msg)
{
    *siridb = NULL;
    qp_obj_t qp_obj;
    qp_obj_t qp_schema;

    if (!qp_is_array(qp_next(unpacker, NULL)) ||
        qp_next(unpacker, &qp_schema) != QP_INT64)
    {
        sprintf(err_msg, "Corrupted database file.");
        return -1;
    }

    /* check schema */
    if (    qp_schema.via.int64 == 1 ||
            qp_schema.via.int64 == 2 ||
            qp_schema.via.int64 == 3 ||
            qp_schema.via.int64 == 4 ||
            qp_schema.via.int64 == 5 ||
            qp_schema.via.int64 == 6)
    {
        log_info(
                "Found an old database schema (v%d), "
                "migrating to schema v%d...",
                qp_schema.via.int64,
                SIRIDB_SCHEMA);
    }
    else if (qp_schema.via.int64 != SIRIDB_SCHEMA)
    {
        sprintf(err_msg, "Unsupported schema found: %" PRId64,
                qp_schema.via.int64);
        return -1;
    }

    /* create a new SiriDB structure */
    *siridb = siridb__new();
    if (*siridb == NULL)
    {
        sprintf(err_msg, "Cannot create SiriDB instance.");
        return -1;
    }

    /* set dbpath */
    if (((*siridb)->dbpath = strdup(dbpath)) == NULL)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot set dbpath.")
    }

    /* read uuid */
    if (qp_next(unpacker, &qp_obj) != QP_RAW || qp_obj.len != 16)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read uuid.")
    }

    /* copy uuid */
    memcpy(&(*siridb)->uuid, qp_obj.via.raw, qp_obj.len);

    /* read database name */
    if (qp_next(unpacker, &qp_obj) != QP_RAW ||
            qp_obj.len >= SIRIDB_MAX_DBNAME_LEN)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read database name.")
    }

    /* alloc mem for database name */
    (*siridb)->dbname = strndup((const char *) qp_obj.via.raw, qp_obj.len);
    if ((*siridb)->dbname == NULL)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot allocate database name.")
    }

    /* read time precision */
    if (qp_next(unpacker, &qp_obj) != QP_INT64 ||
            qp_obj.via.int64 < SIRIDB_TIME_SECONDS ||
            qp_obj.via.int64 > SIRIDB_TIME_NANOSECONDS)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read time-precision.")
    }

    /* bind time precision to SiriDB */
    (*siridb)->time =
            siridb_time_new((siridb_timep_t) qp_obj.via.int64);
    if ((*siridb)->time == NULL)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot create time instance.")
    }

    /* read buffer size, same buffer_size requirements are used in request.c */
    if (    qp_next(unpacker, &qp_obj) != QP_INT64 ||
            !siridb_buffer_is_valid_size(qp_obj.via.int64))
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read buffer size.")
    }

    /* bind buffer size to SiriDB */
    (*siridb)->buffer->size = (size_t) qp_obj.via.int64;

    /* read number duration  */
    if (qp_next(unpacker, &qp_obj) != QP_INT64)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read number duration.")
    }

    /* bind number duration to SiriDB */
    (*siridb)->duration_num = (uint64_t) qp_obj.via.int64;

    /* calculate 'shard_mask_num' based on number duration */
    (*siridb)->shard_mask_num =
            (uint16_t) sqrt((double) siridb_time_in_seconds(
                    *siridb, (*siridb)->duration_num)) / 24;

    /* read log duration  */
    if (qp_next(unpacker, &qp_obj) != QP_INT64)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read log duration.")
    }

    /* bind log duration to SiriDB */
    (*siridb)->duration_log = (uint64_t) qp_obj.via.int64;

    /* calculate 'shard_mask_log' based on log duration */
    (*siridb)->shard_mask_log =
            (uint16_t) sqrt((double) siridb_time_in_seconds(
                    *siridb, (*siridb)->duration_log)) / 24;

    log_debug("Set number duration mask to %d", (*siridb)->shard_mask_num);
    log_debug("Set log duration mask to %d", (*siridb)->shard_mask_log);

    /* read timezone */
    if (qp_next(unpacker, &qp_obj) != QP_RAW)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read timezone.")
    }

    /* bind timezone to SiriDB */
    char * tzname = strndup((const char *) qp_obj.via.raw, qp_obj.len);

    if (tzname == NULL)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot allocate timezone name.")
    }

    if (((*siridb)->tz = iso8601_tz(tzname)) < 0)
    {
        log_critical("Unknown timezone found: '%s'.", tzname);
        free(tzname);
        READ_DB_EXIT_WITH_ERROR("Cannot read timezone.")
    }
    free(tzname);

    /* read drop threshold */
    if (qp_next(unpacker, &qp_obj) != QP_DOUBLE ||
            qp_obj.via.real < 0.0 || qp_obj.via.real > 1.0)
    {
        READ_DB_EXIT_WITH_ERROR("Cannot read drop threshold.")
    }
    (*siridb)->drop_threshold = qp_obj.via.real;

    if (qp_schema.via.int64 == 1)
    {
        (*siridb)->select_points_limit = DEF_SELECT_POINTS_LIMIT;
        (*siridb)->list_limit = DEF_LIST_LIMIT;
    }
    else
    {
        /* read select points limit */
        if (qp_next(unpacker, &qp_obj) != QP_INT64 || qp_obj.via.int64 < 1)
        {
            READ_DB_EXIT_WITH_ERROR("Cannot read select points limit.")
        }
        (*siridb)->select_points_limit = qp_obj.via.int64;

        /* read list limit */
        if (qp_next(unpacker, &qp_obj) != QP_INT64 || qp_obj.via.int64 < 1)
        {
            READ_DB_EXIT_WITH_ERROR("Cannot read list limit.")
        }
        (*siridb)->list_limit = qp_obj.via.int64;
    }

    /* for older schemas we keep the default tee_pipe_name=NULL */
    if (qp_schema.via.int64 >= 5 && qp_schema.via.int64 <=6)
    {
        qp_next(unpacker, &qp_obj);
        /* Skip the tee pipe name */
    }
    if (qp_schema.via.int64 >= 6)
    {
        /* read select points limit */
        if (qp_next(unpacker, &qp_obj) != QP_INT64 || qp_obj.via.int64 < 0)
        {

            READ_DB_EXIT_WITH_ERROR(
                    "Cannot read shard (log) expiration time.")
        }
        (*siridb)->expiration_log = qp_obj.via.int64;

        /* read list limit */
        if (qp_next(unpacker, &qp_obj) != QP_INT64 || qp_obj.via.int64 < 0)
        {
            READ_DB_EXIT_WITH_ERROR(
                    "Cannot read shard (number) expiration time.")
        }
        (*siridb)->expiration_num = qp_obj.via.int64;
    }
    if (qp_schema.via.int64 >= 7)
    {
        qp_next(unpacker, &qp_obj);

        if (qp_obj.tp == QP_RAW)
        {
            (*siridb)->tee->address = strndup(
                (char *) qp_obj.via.raw,
                qp_obj.len);

            if (!(*siridb)->tee->address)
            {
                READ_DB_EXIT_WITH_ERROR("Cannot allocate tee address.")
            }
        }
        else if (qp_obj.tp != QP_NULL)
        {
            READ_DB_EXIT_WITH_ERROR("Cannot read tee address.")
        }

        if (qp_next(unpacker, &qp_obj) != QP_INT64 ||
            qp_obj.via.int64 < 0 ||
            qp_obj.via.int64 > 65535)
        {
            READ_DB_EXIT_WITH_ERROR("Cannot read tee port.")
        }

        (*siridb)->tee->port = qp_obj.via.int64;
    }

    if ((*siridb)->tee->address == NULL)
    {
        log_debug(
            "No tee configured for database: %s",
            (*siridb)->dbname);
    }
    else
    {
        log_debug(
            "Using tee '%s:%u' for database: '%s'",
            (*siridb)->tee->address,
            (*siridb)->tee->port,
            (*siridb)->dbname);
    }

    return (qp_schema.via.int64 == SIRIDB_SCHEMA) ? 0 : qp_schema.via.int64;
}

/*
 * Get a siridb object by name.
 */
siridb_t * siridb_get(llist_t * siridb_list, const char * dbname)
{
    llist_node_t * node = siridb_list->first;
    siridb_t * siridb;

    while (node != NULL)
    {
        siridb = (siridb_t *) node->data;
        if (strcmp(siridb->dbname, dbname) == 0)
        {
            return siridb;
        }
        node = node->next;
    }

    return NULL;
}

/*
 * Get a siridb object by name.
 */
siridb_t * siridb_getn(llist_t * siridb_list, const char * dbname, size_t n)
{
    llist_node_t * node = siridb_list->first;
    siridb_t * siridb;

    while (node != NULL)
    {
        siridb = (siridb_t *) node->data;
        if (n == strlen(siridb->dbname) &&
            strncmp(siridb->dbname, dbname, n) == 0)
        {
            return siridb;
        }
        node = node->next;
    }

    return NULL;
}

/*
 * Get a siridb object by qpack name.
 */
siridb_t * siridb_get_by_qp(llist_t * siridb_list, qp_obj_t * qp_dbname)
{
    assert (qp_dbname->tp == QP_RAW);

    llist_node_t * node = siridb_list->first;
    siridb_t * siridb;

    while (node != NULL)
    {
        siridb = (siridb_t *) node->data;
        if (qp_dbname->len == strlen(siridb->dbname) &&
            strncmp(
                siridb->dbname,
                (const char *) qp_dbname->via.raw,
                qp_dbname->len) == 0)
        {
            return siridb;
        }
        node = node->next;
    }

    return NULL;
}


/*
 * Sometimes we need a callback function and cannot use a macro expansion.
 */
int siridb_decref_cb(siridb_t * siridb, void * args __attribute__((unused)))
{
    siridb_decref(siridb);
    return 0;
}

/*
 * Typedef: sirinet_clserver_get_file
 *
 * Returns the length of the content for a file and set buffer with the file
 * content. Note that malloc is used to allocate memory for the buffer.
 *
 * In case of an error -1 is returned and buffer will be set to NULL.
 */
ssize_t siridb_get_file(char ** buffer, siridb_t * siridb)
{
    /* get servers file name */
    siridb_misc_get_fn(fn, siridb->dbpath, "database.dat")

    return xpath_get_content(buffer, fn);
}

/*
 * Returns the number of open files by the given database.
 * (includes both the database and buffer path)
 */
int siridb_open_files(siridb_t * siridb)
{
    siridb_buffer_t * buffer = siridb->buffer;
    return procinfo_open_files(
            siridb->dbpath,
            (buffer->fp == NULL) ? -1 : buffer->fd);
}

/*
 * Returns 0 if successful or -1 in case of an error.
 */
int siridb_save(siridb_t * siridb)
{
    char buffer[XPATH_MAX];
    buffer[XPATH_MAX-1] = '\0';
    snprintf(buffer,
            XPATH_MAX-1,
            "%sdatabase.dat",
            siridb->dbpath);

    qp_fpacker_t * fpacker;

    if ((fpacker = qp_open(buffer, "w")) == NULL)
    {
        return -1;
    }

    return (qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||
            qp_fadd_int64(fpacker, SIRIDB_SCHEMA) ||
            qp_fadd_raw(fpacker, (const unsigned char *) siridb->uuid, 16) ||
            qp_fadd_string(fpacker, siridb->dbname) ||
            qp_fadd_int64(fpacker, siridb->time->precision) ||
            qp_fadd_int64(fpacker, siridb->buffer->size) ||
            qp_fadd_int64(fpacker, siridb->duration_num) ||
            qp_fadd_int64(fpacker, siridb->duration_log) ||
            qp_fadd_string(fpacker, iso8601_tzname(siridb->tz)) ||
            qp_fadd_double(fpacker, siridb->drop_threshold) ||
            qp_fadd_int64(fpacker, siridb->select_points_limit) ||
            qp_fadd_int64(fpacker, siridb->list_limit) ||
            qp_fadd_int64(fpacker, siridb->expiration_log) ||
            qp_fadd_int64(fpacker, siridb->expiration_num) ||
            (siridb->tee->address == NULL
                ? qp_fadd_type(fpacker, QP_NULL)
                : qp_fadd_string(fpacker, siridb->tee->address)) ||
            qp_fadd_int64(fpacker, siridb->tee->port) ||
            qp_fadd_type(fpacker, QP_ARRAY_CLOSE) ||
            qp_close(fpacker));
}


/*
 * Destroy SiriDB object.
 *
 * Never call this function but rather call siridb_decref.
 */
void siridb__free(siridb_t * siridb)
{
    /* first we should close the buffer and all other open files */
    if (siridb->buffer != NULL)
    {
        siridb_buffer_close(siridb->buffer);
    }

    if (siridb->dropped_fp != NULL)
    {
        fclose(siridb->dropped_fp);
    }

    if (siridb->store != NULL)
    {
        qp_close(siridb->store);
    }

    /* free users */
    if (siridb->users != NULL)
    {
        siridb_users_free(siridb->users);
    }

    /* we do not need to free server and replica since they exist in
     * this list and therefore will be freed.
     */
    if (siridb->servers != NULL)
    {
        siridb_servers_free(siridb->servers);
    }

    /*
     * Destroy replicate before fifo but after servers so open promises are
     * closed which might depend on siridb->replicate
     *
     * siridb->replicate must be closed, see 'SIRI_set_closing_state'
     */
    if (siridb->replicate != NULL)
    {
        siridb_replicate_free(&siridb->replicate);
    }

    if (siridb->reindex != NULL)
    {
        siridb_reindex_free(&siridb->reindex);
    }

    /* free fifo (in case we have a replica) */
    if (siridb->fifo != NULL)
    {
        siridb_fifo_free(siridb->fifo);
    }

    /* free pools */
    if (siridb->pools != NULL)
    {
        siridb_pools_free(siridb->pools);
    }

    /* free imap (series) */
    if (siridb->series_map != NULL)
    {
        imap_free(siridb->series_map, NULL);
    }

    /* free c-tree lookup and series */
    if (siridb->series != NULL)
    {
        ct_free(siridb->series, (ct_free_cb) &siridb__series_decref);
    }

    /* free shards using imap walk an free the imap */
    if (siridb->shards != NULL)
    {
        imap_free(siridb->shards, (imap_free_cb) &siridb_shards_destroy_cb);
    }

    if (siridb->groups != NULL)
    {
        uv_thread_join(&siridb->groups->thread);
        siridb_groups_decref(siridb->groups);
    }

    if (siridb->tags != NULL)
    {
         siridb_tags_decref(siridb->tags);
    }

    if (siridb->buffer != NULL)
    {
        siridb_buffer_free(siridb->buffer);
    }

    if (siridb->tee != NULL)
    {
        siridb_tee_free(siridb->tee);
    }

    /* unlock the database in case no siri_err occurred */
    if (!siri_err)
    {
        lock_t lock_rc = lock_unlock(siridb->dbpath);
        if (lock_rc != LOCK_REMOVED)
        {
            log_error("%s", lock_str(lock_rc));
        }
    }

    uv_mutex_destroy(&siridb->series_mutex);
    uv_mutex_destroy(&siridb->shards_mutex);
    uv_mutex_destroy(&siridb->values_mutex);

    if (siridb->flags & SIRIDB_FLAG_DROPPED)
    {
        xpath_rmdir(siridb->dbpath);
    }

    free(siridb->dbpath);
    free(siridb->dbname);
    free(siridb->time);
    free(siridb);
}

void siridb_drop(siridb_t * siridb)
{
    if (siridb->flags & SIRIDB_FLAG_DROPPED)
    {
        return;
    }

    log_warning("dropping database '%s'", siridb->dbname);

    siridb->flags |= SIRIDB_FLAG_DROPPED;

    uv_mutex_lock(&siri.siridb_mutex);

    llist_remove(siri.siridb_list, NULL, siridb);

    uv_mutex_unlock(&siri.siridb_mutex);

    if (siridb->replicate != NULL)
    {
        siridb_replicate_close(siridb->replicate);
    }

    if (siridb->reindex != NULL && siridb->reindex->timer != NULL)
    {
        siridb_reindex_close(siridb->reindex);
    }

    if (siridb->groups != NULL)
    {
        siridb_groups_destroy(siridb->groups);
    }

    siridb_decref(siridb);
}

void siridb_update_shard_expiration(siridb_t * siridb)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    uv_mutex_lock(&siridb->values_mutex);

    siridb->exp_at_num = siridb->expiration_num
            ? siridb_time_now(siridb, now) - siridb->expiration_num
            : 0;

    siridb->exp_at_log = siridb->expiration_log
            ? siridb_time_now(siridb, now) - siridb->expiration_log
            : 0;

    uv_mutex_unlock(&siridb->values_mutex);
}


/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static siridb_t * siridb__new(void)
{
    siridb_t * siridb = malloc(sizeof(siridb_t));
    if (siridb == NULL)
    {
        goto fail0;
    }

    siridb->dbname = NULL;
    siridb->dbpath = NULL;
    siridb->ref = 1;
    siridb->insert_tasks = 0;
    siridb->flags = 0;
    siridb->time = NULL;
    siridb->users = NULL;
    siridb->servers = NULL;
    siridb->pools = NULL;
    siridb->dropped_fp = NULL;
    siridb->max_series_id = 0;
    siridb->received_points = 0;
    siridb->selected_points = 0;
    siridb->drop_threshold = DEF_DROP_THRESHOLD;
    siridb->select_points_limit = DEF_SELECT_POINTS_LIMIT;
    siridb->list_limit = DEF_LIST_LIMIT;
    siridb->tz = -1;
    siridb->server = NULL;
    siridb->replica = NULL;
    siridb->fifo = NULL;
    siridb->replicate = NULL;
    siridb->reindex = NULL;
    siridb->groups = NULL;
    siridb->groups = NULL;
    siridb->tags = NULL;
    siridb->store = NULL;
    siridb->exp_at_log = 0;
    siridb->exp_at_num = 0;
    siridb->expiration_log = 0;
    siridb->expiration_num = 0;

    siridb->series = ct_new();
    if (siridb->series == NULL)
    {
        goto fail0;
    }

    siridb->series_map = imap_new();
    if (siridb->series_map == NULL)
    {
        goto fail1;
    }
    siridb->shards = imap_new();
    if (siridb->shards == NULL)
    {
        goto fail2;
    }
    /* allocate a buffer */
    siridb->buffer = siridb_buffer_new();
    if (siridb->buffer == NULL)
    {
        goto fail3;
    }

    /* allocate tee */
    siridb->tee = siridb_tee_new();
    if (siridb->tee == NULL)
    {
        goto fail4;
    }

    uv_mutex_init(&siridb->series_mutex);
    uv_mutex_init(&siridb->shards_mutex);
    uv_mutex_init(&siridb->values_mutex);

    return siridb;

fail4:
    siridb_buffer_free(siridb->buffer);
fail3:
    imap_free(siridb->shards, NULL);
fail2:
    imap_free(siridb->series_map, NULL);
fail1:
    ct_free(siridb->series, NULL);
fail0:
    free(siridb);
    ERR_ALLOC
    return NULL;
}

static siridb_t * siridb__from_dat(const char * dbpath)
{
    int rc;
    siridb_t * siridb = NULL;
    char err_msg[512];
    qp_unpacker_t * unpacker;
    char buffer[XPATH_MAX];
    buffer[XPATH_MAX-1] = '\0';
    snprintf(buffer,
                XPATH_MAX-1,
                "%sdatabase.dat",
                dbpath);

    unpacker = qp_unpacker_ff(buffer);
    if (unpacker == NULL)
    {
        return NULL;
    }

    if ((rc = siridb__from_unpacker(
            unpacker,
            &siridb,
            dbpath,
            err_msg)) < 0)
    {
        log_error("Could not read '%s': %s", buffer, err_msg);
        qp_unpacker_ff_free(unpacker);
        return NULL;
    }

    qp_unpacker_ff_free(unpacker);

    if (rc > 0 && siridb_save(siridb))
    {
        log_error("Could not write file: %s", buffer);
        siridb_decref(siridb);
        return NULL;
    }

    return siridb;
}

static int siridb__read_conf(siridb_t * siridb)
{
    int rc;
    char buf[XPATH_MAX];
    cfgparser_t * cfgparser;
    cfgparser_option_t * option = NULL;
    siridb_buffer_t * buffer = siridb->buffer;
    buf[XPATH_MAX-1] = '\0';
    snprintf(buf,
            XPATH_MAX-1,
            "%sdatabase.conf",
            siridb->dbpath);

    cfgparser = cfgparser_new();
    if (cfgparser == NULL)
    {
        return -1;  /* signal is raised */
    }

    rc = cfgparser_read(cfgparser, buf);

    if (rc != CFGPARSER_SUCCESS)
    {
        log_error("Could not read '%s': %s", buf, cfgparser_errmsg(rc));
        cfgparser_free(cfgparser);
        return -1;
    }

    /* read buffer_path from database.conf */
    rc = cfgparser_get_option(&option, cfgparser, "buffer", "path");
    siridb_buffer_set_path(
        buffer,
        (rc == CFGPARSER_SUCCESS && option->tp == CFGPARSER_TP_STRING) ?
                option->val->string : siridb->dbpath);

    /* read buffer size from database.conf */
    rc = cfgparser_get_option(&option, cfgparser, "buffer", "size");

    if (rc == CFGPARSER_SUCCESS && option->tp == CFGPARSER_TP_INTEGER)
    {
        ssize_t ssize = option->val->integer;
        if (!siridb_buffer_is_valid_size(ssize))
        {
            log_warning(
                "Invalid buffer size: %" PRId64
                " (expecting a multiple of 512 with a maximum of %" PRId64 ")",
                ssize,
                (int64_t) MAX_BUFFER_SZ);
        }
        else
        {
            buffer->_to_size = (buffer->size == (size_t) ssize) ?
                    0 : (size_t) ssize;
        }
    }
    else
    {
        FILE * fp = fopen(buf, "a");
        if (fp != NULL)
        {
            if (rc == CFGPARSER_ERR_SECTION_NOT_FOUND)
            {
                (void) fprintf(fp, "\n[buffer]\nsize = %zu\n", buffer->size);
            }
            else if (rc == CFGPARSER_ERR_OPTION_NOT_FOUND)
            {
                (void) fprintf(fp, "\nsize = %zu\n", buffer->size);
            }
            (void) fclose(fp);
        }
    }
    cfgparser_free(cfgparser);

    return (buffer->path == NULL) ? -1 : 0;
}

static int siridb__lock(const char * dbpath, int lock_flags)
{
    lock_t lock_rc = lock_lock(dbpath, lock_flags);

    switch (lock_rc)
    {
    case LOCK_IS_LOCKED_ERR:
    case LOCK_PROCESS_NAME_ERR:
    case LOCK_WRITE_ERR:
    case LOCK_READ_ERR:
    case LOCK_MEM_ALLOC_ERR:
        log_error("%s (%s)", lock_str(lock_rc), dbpath);
        return -1;
    case LOCK_NEW:
        log_info("%s (%s)", lock_str(lock_rc), dbpath);
        break;
    case LOCK_OVERWRITE:
        log_warning("%s (%s)", lock_str(lock_rc), dbpath);
        break;
    default:
        assert (0);
        break;
    }
    return 0;
}
