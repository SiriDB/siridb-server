/*
 * db.c - contains functions  and constants for a SiriDB database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#include <siri/db/db.h>
#include <logger/logger.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <siri/db/time.h>
#include <siri/db/users.h>
#include <uuid/uuid.h>
#include <siri/db/series.h>
#include <siri/db/shard.h>
#include <siri/db/servers.h>
#include <math.h>
#include <procinfo/procinfo.h>
#include <lock/lock.h>
#include <siri/db/shards.h>
#include <cfgparser/cfgparser.h>
#include <siri/siri.h>
#include <xpath/xpath.h>
#include <lock/lock.h>
#include <siri/err.h>

#define SIRIDB_SHEMA 1

static siridb_t * SIRIDB_new(void);
static void SIRIDB_free(siridb_t * siridb);

#define READ_DB_EXIT_WITH_ERROR(ERROR_MSG)  \
{                                           \
    sprintf(err_msg, "error: " ERROR_MSG);  \
    SIRIDB_free(*siridb);                   \
    *siridb = NULL;                         \
    qp_object_free(qp_obj);                 \
    return -1;                              \
}

siridb_t * siridb_new(const char * dbpath, int lock_flags)
{
    size_t len = strlen(dbpath);
    lock_t lock_rc;
    char buffer[PATH_MAX];
    cfgparser_t * cfgparser;
    cfgparser_option_t * option = NULL;
    qp_unpacker_t * unpacker;
    siridb_t * siridb;
    char err_msg[512];
    int rc;

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

    lock_rc = lock_lock(dbpath, lock_flags);

    switch (lock_rc)
    {
    case LOCK_IS_LOCKED_ERR:
    case LOCK_PROCESS_NAME_ERR:
    case LOCK_WRITE_ERR:
    case LOCK_READ_ERR:
    case LOCK_MEM_ALLOC_ERR:
        log_error("%s (%s)", lock_str(lock_rc), dbpath);
        return NULL;
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

    /* read database.conf */
    snprintf(buffer,
            PATH_MAX,
            "%sdatabase.conf",
            dbpath);

    cfgparser = cfgparser_new();
    if (cfgparser == NULL)
    {
        return NULL;  /* signal is raised */
    }
    if ((rc = cfgparser_read(cfgparser, buffer)) != CFGPARSER_SUCCESS)
    {
        log_error("Could not read '%s': %s",
                buffer,
                cfgparser_errmsg(rc));
        cfgparser_free(cfgparser);
        return NULL;
    }

    snprintf(buffer,
            PATH_MAX,
            "%sdatabase.dat",
            dbpath);

    if ((unpacker = qp_unpacker_from_file(buffer)) == NULL)
    {
        /* qp_unpacker has done some logging */
        cfgparser_free(cfgparser);
        return NULL;
    }

    if (siridb_from_unpacker(
            unpacker,
            &siridb,
            err_msg))
    {
        log_error("Could not read '%s': %s", buffer, err_msg);
        qp_unpacker_free(unpacker);
        cfgparser_free(cfgparser);
        return NULL;
    }

    qp_unpacker_free(unpacker);

    log_info("Start loading database: '%s'", siridb->dbname);

    /* set dbpath */
    siridb->dbpath = strdup(dbpath);
    if (siridb->dbpath == NULL)
    {
        ERR_ALLOC
        siridb_decref(siridb);
        cfgparser_free(cfgparser);
        return NULL;
    }

    /* read buffer_path from database.conf */
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "buffer",
                "buffer_path");

    siridb->buffer_path = (
            rc == CFGPARSER_SUCCESS &&
            option->tp == CFGPARSER_TP_STRING) ?
                    strdup(option->val->string) : siridb->dbpath;

    /* free cfgparser */
    cfgparser_free(cfgparser);

    if (siridb->buffer_path == NULL)
    {
        ERR_ALLOC
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

    /* load buffer */
    if (siridb_buffer_load(siridb))
    {
        log_error("Could not read buffer for database '%s'", siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    /* open buffer */
    if (siridb_buffer_open(siridb))
    {
        log_error("Could not open buffer for database '%s'", siridb->dbname);
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

    /* update series props */
    log_info("Updating series properties");

    /* create a copy since 'siridb_series_update_props' might drop a series */
    slist_t * slist = imap32_2slist(siridb->series_map);

    if (slist == NULL)
    {
        log_error("Could update series properties for database '%s'",
                siridb->dbname);
        siridb_decref(siridb);
        return NULL;
    }

    for (size_t i = 0; i < slist->len; i++)
    {
        siridb_series_update_props(siridb, (siridb_series_t * )slist->data[i]);
    }

    slist_free(slist);

    /* generate pools, this can raise a signal */
    siridb_pools_init(siridb);

    if (!siri_err)
    {
        siridb->reindex = siridb_reindex_open(siridb, 0);
        if (siridb->reindex != NULL && siridb->replica == NULL)
        {
            siridb_reindex_start(siridb->reindex->timer);
        }
    }

    siridb->start_ts = time(NULL);

    uv_mutex_lock(&siri.siridb_mutex);

    /* append SiriDB to siridb_list (reference counter is already 1) */
    if (llist_append(siri.siridb_list, siridb))
    {
        siridb_decref(siridb);
        return NULL;
    }

    uv_mutex_unlock(&siri.siridb_mutex);

    log_info("Finished loading database: '%s'", siridb->dbname);

    return siridb;
}

/*
 * Read SiriDB from unpacker. (reference counter is initially set to 1)
 *
 * Returns 0 if successful or another value in case of an error.
 * (a SIGNAL can be set in case of an error)
 */
int siridb_from_unpacker(
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        char * err_msg)
{
    *siridb = NULL;
    qp_obj_t * qp_obj = qp_object_new();
    if (qp_obj == NULL)
    {
        sprintf(err_msg, "error: allocation error.");
        return -1;
    }
    if (!qp_is_array(qp_next(unpacker, NULL)) ||
            qp_next(unpacker, qp_obj) != QP_INT64)
    {
        sprintf(err_msg, "error: corrupted database file.");
        qp_object_free(qp_obj);
        return -1;
    }

    /* check schema */
    if (qp_obj->via->int64 != SIRIDB_SHEMA)
    {
        sprintf(err_msg, "error: unsupported schema found: %ld",
                qp_obj->via->int64);
        qp_object_free(qp_obj);
        return -1;
    }

    /* create a new SiriDB structure */
    *siridb = SIRIDB_new();
    if (*siridb == NULL)
    {
        sprintf(err_msg, "error: cannot create SiriDB instance");
        qp_object_free(qp_obj);
        return -1;
    }

    /* read uuid */
    if (qp_next(unpacker, qp_obj) != QP_RAW || qp_obj->len != 16)
        READ_DB_EXIT_WITH_ERROR("cannot read uuid.")

    /* copy uuid */
    memcpy(&(*siridb)->uuid, qp_obj->via->raw, qp_obj->len);

    /* read database name */
    if (qp_next(unpacker, qp_obj) != QP_RAW ||
            qp_obj->len >= SIRIDB_MAX_DBNAME_LEN)
        READ_DB_EXIT_WITH_ERROR("cannot read database name.")

    /* alloc mem for database name */
    (*siridb)->dbname = strndup(qp_obj->via->raw, qp_obj->len);
    if ((*siridb)->dbname == NULL)
        READ_DB_EXIT_WITH_ERROR("cannot allocate database name.")

    /* read time precision */
    if (qp_next(unpacker, qp_obj) != QP_INT64 ||
            qp_obj->via->int64 < SIRIDB_TIME_SECONDS ||
            qp_obj->via->int64 > SIRIDB_TIME_NANOSECONDS)
        READ_DB_EXIT_WITH_ERROR("cannot read time-precision.")

    /* bind time precision to SiriDB */
    (*siridb)->time =
            siridb_time_new((siridb_timep_t) qp_obj->via->int64);
    if ((*siridb)->time == NULL)
        READ_DB_EXIT_WITH_ERROR("cannot create time instance.")

    /* read buffer size */
    if (qp_next(unpacker, qp_obj) != QP_INT64)
        READ_DB_EXIT_WITH_ERROR("cannot read buffer size.")

    /* bind buffer size and len to SiriDB */
    (*siridb)->buffer_size = (size_t) qp_obj->via->int64;
    (*siridb)->buffer_len = (*siridb)->buffer_size / sizeof(siridb_point_t);

    /* read number duration  */
    if (qp_next(unpacker, qp_obj) != QP_INT64)
        READ_DB_EXIT_WITH_ERROR("cannot read number duration.")

    /* bind number duration to SiriDB */
    (*siridb)->duration_num = (uint64_t) qp_obj->via->int64;

    /* calculate 'shard_mask_num' based on number duration */
    (*siridb)->shard_mask_num =
            (uint16_t) sqrt((double) siridb_time_in_seconds(
                    *siridb, (*siridb)->duration_num)) / 24;

    /* read log duration  */
    if (qp_next(unpacker, qp_obj) != QP_INT64)
        READ_DB_EXIT_WITH_ERROR("cannot read log duration.")

    /* bind log duration to SiriDB */
    (*siridb)->duration_log = (uint64_t) qp_obj->via->int64;

    /* calculate 'shard_mask_log' based on log duration */
    (*siridb)->shard_mask_log =
            (uint16_t) sqrt((double) siridb_time_in_seconds(
                    *siridb, (*siridb)->duration_log)) / 24;

    log_debug("Set number duration mask to %d", (*siridb)->shard_mask_num);
    log_debug("Set log duration mask to %d", (*siridb)->shard_mask_log);

    /* read timezone */
    if (qp_next(unpacker, qp_obj) != QP_RAW)
        READ_DB_EXIT_WITH_ERROR("cannot read timezone.")

    /* bind timezone to SiriDB */
    char * tzname = strndup(qp_obj->via->raw, qp_obj->len);

    if (tzname == NULL)
        READ_DB_EXIT_WITH_ERROR("cannot allocate timezone name.")

    if (((*siridb)->tz = iso8601_tz(tzname)) < 0)
    {
        log_critical("Unknown timezone found: '%s'.", tzname);
        free(tzname);
        READ_DB_EXIT_WITH_ERROR("cannot read timezone.")
    }
    free(tzname);

    /* free qp_object */
    qp_object_free(qp_obj);

    return 0;
}

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

void siridb_free_cb(siridb_t * siridb, void * args)
{
    siridb_decref(siridb);
}

inline void siridb_incref(siridb_t * siridb)
{
    siridb->ref++;
}

void siridb_decref(siridb_t * siridb)
{
    if (!--siridb->ref)
    {
        SIRIDB_free(siridb);
    }
}

int siridb_open_files(siridb_t * siridb)
{
    int open_files = procinfo_open_files(siridb->dbpath);

    if (    siridb->buffer_path != siridb->dbpath &&
            strncmp(
                siridb->dbpath,
                siridb->buffer_path,
                strlen(siridb->dbpath)))
    {
        open_files += procinfo_open_files(siridb->buffer_path);
    }

    return open_files;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
static siridb_t * SIRIDB_new(void)
{
    siridb_t * siridb = (siridb_t *) malloc(sizeof(siridb_t));
    if (siridb == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        siridb->series = ct_new();
        if (siridb->series == NULL)
        {
            free(siridb);
            siridb = NULL;  /* signal is raised */
        }
        else
        {
            siridb->series_map = imap32_new();
            if (siridb->series_map == NULL)
            {
                ct_free(siridb->series, NULL);
                free(siridb);
                siridb = NULL;  /* signal is raised */
            }
            else
            {
                siridb->shards = imap64_new();
                if (siridb->shards == NULL)
                {
                    imap32_free(siridb->series_map);
                    ct_free(siridb->series, NULL);
                    free(siridb);
                    siridb = NULL;  /* signal is raised */
                }
                else
                {

                    siridb->dbname = NULL;
                    siridb->dbpath = NULL;
                    siridb->ref = 1;
                    siridb->flags = 0;
                    siridb->buffer_path = NULL;
                    siridb->time = NULL;
                    siridb->users = NULL;
                    siridb->servers = NULL;
                    siridb->pools = NULL;
                    siridb->max_series_id = 0;
                    siridb->received_points = 0;
                    siridb->buffer_size = -1;
                    siridb->tz = -1;
                    siridb->server = NULL;
                    siridb->replica = NULL;
                    siridb->fifo = NULL;
                    siridb->replicate = NULL;
                    siridb->reindex = NULL;

                    /* make file pointers are NULL when file is closed */
                    siridb->buffer_fp = NULL;
                    siridb->dropped_fp = NULL;
                    siridb->store = NULL;

                    uv_mutex_init(&siridb->series_mutex);
                    uv_mutex_init(&siridb->shards_mutex);
                }
            }
        }
    }
    return siridb;
}

static void SIRIDB_free(siridb_t * siridb)
{
#ifdef DEBUG
    log_debug("Free database: %s", siridb->dbname);
#endif

    /* first we should close all open files */
    if (siridb->buffer_fp != NULL)
    {
        fclose(siridb->buffer_fp);
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

    /* free imap32 (series) */
    if (siridb->series_map != NULL)
    {
        imap32_free(siridb->series_map);
    }

    /* free c-tree lookup and series */
    if (siridb->series != NULL)
    {
        ct_free(siridb->series, (ct_free_cb) &siridb_series_decref);
    }

    /* free shards using imap64 walk an free the imap64 */
    if (siridb->shards != NULL)
    {
        imap64_walk(siridb->shards, (imap64_cb_t) &siridb_shard_decref, NULL);
        imap64_free(siridb->shards);
    }

    /* only free buffer path when not equal to db_path */
    if (siridb->buffer_path != siridb->dbpath)
    {
        free(siridb->buffer_path);
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

    free(siridb->dbpath);
    free(siridb->dbname);
    free(siridb->time);

    uv_mutex_destroy(&siridb->series_mutex);
    uv_mutex_destroy(&siridb->shards_mutex);

    free(siridb);
}


