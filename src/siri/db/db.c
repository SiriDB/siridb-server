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
#include <siri/db/user.h>
#include <uuid/uuid.h>
#include <siri/db/series.h>

static siridb_t * siridb_new(void);
static void siridb_free(siridb_t * siridb);
static void siridb_add(siridb_list_t * siridb_list, siridb_t * siridb);

siridb_list_t * siridb_new_list(void)
{
    siridb_list_t * siridb_list;
    siridb_list = (siridb_list_t *) malloc(sizeof(siridb_list_t));
    siridb_list->siridb = NULL;
    siridb_list->next = NULL;

    return siridb_list;
}

void siridb_free_list(siridb_list_t * siridb_list)
{
    siridb_list_t * next;
    while (siridb_list != NULL)
    {
        next = siridb_list->next;
        siridb_free(siridb_list->siridb);
        free(siridb_list);
        siridb_list = next;
    }
}

int siridb_add_from_unpacker(
        siridb_list_t * siridb_list,
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        char * err_msg)
{
    if (!qp_is_array(qp_next_object(unpacker)) ||
            qp_next_object(unpacker) != QP_INT64)
    {
        sprintf(err_msg, "error: corrupted database file.");
        return 1;
    }

    /* check schema */
    if (unpacker->qp_obj->via->int64 != 1)
    {
        sprintf(err_msg, "error: unsupported schema found: %ld",
                unpacker->qp_obj->via->int64);
        return 1;
    }

    /* create a new SiriDB structure */
    *siridb = siridb_new();

    /* read uuid */
    if (qp_next_object(unpacker) != QP_RAW ||
            unpacker->qp_obj->len != 16)
    {
        sprintf(err_msg, "error: cannot read uuid.");
        siridb_free(*siridb);
        return 1;
    }

    /* copy uuid */
    memcpy(&(*siridb)->uuid, unpacker->qp_obj->via->raw, unpacker->qp_obj->len);

    /* read database name */
    if (qp_next_object(unpacker) != QP_RAW ||
            unpacker->qp_obj->len >= SIRIDB_MAX_DBNAME_LEN)
    {
        sprintf(err_msg, "error: cannot read database name.");
        siridb_free(*siridb);
        return 1;
    }

    /* alloc mem for database name */
    (*siridb)->dbname = (char *) malloc(unpacker->qp_obj->len + 1);

    /* copy datatabase name */
    memcpy((*siridb)->dbname, unpacker->qp_obj->via->raw, unpacker->qp_obj->len);

    /* set terminator */
    (*siridb)->dbname[unpacker->qp_obj->len] = 0;

    /* read time precision */
    if (qp_next_object(unpacker) != QP_INT64 ||
            unpacker->qp_obj->via->int64 < SIRIDB_TIME_SECONDS ||
            unpacker->qp_obj->via->int64 > SIRIDB_TIME_NANOSECONDS)
    {
        sprintf(err_msg, "error: cannot read time-precision.");
        siridb_free(*siridb);
        return 1;
    }

    /* bind time precision to SiriDB */
    (*siridb)->time_precision = (siridb_time_t) unpacker->qp_obj->via->int64;

    /* add SiriDB to list */
    siridb_add(siridb_list, *siridb);

    return 0;
}

siridb_t * siridb_get(siridb_list_t * siridb_list, const char * dbname)
{
    siridb_list_t * current = siridb_list;
    size_t len = strlen(dbname);
    const char * chk;

    if (current->siridb == NULL)
        return NULL;

    while (current != NULL)
    {
        chk = current->siridb->dbname;

        if (strlen(chk) == len && strncmp(chk, dbname, len) == 0)
            return current->siridb;

        current = current->next;
    }

    return NULL;
}

static siridb_t * siridb_new(void)
{
    siridb_t * siridb;
    siridb = (siridb_t *) malloc(sizeof(siridb_t));
    siridb->dbname = NULL;
    siridb->dbpath = NULL;
    siridb->time_precision = -1;
    siridb->users = NULL;
    siridb->servers = NULL;
    siridb->pools = NULL;
    siridb->max_series_id = 0;
    siridb->series = ct_new();
    siridb->series_map = imap32_new();
    return siridb;
}

static void siridb_free(siridb_t * siridb)
{
    log_debug("Free SiriDB...");
    if (siridb != NULL)
    {
        free(siridb->dbname);
        free(siridb->dbpath);
        siridb_free_users(siridb->users);
        /* we do not need to free server and replica since they exist in
         * this list and therefore will be freed.
         */
        siridb_free_servers(siridb->servers);
        siridb_free_pools(siridb->pools);
        ct_free(siridb->series);
        imap32_walk(siridb->series_map, (imap32_cb_t) &siridb_free_series);
        imap32_free(siridb->series_map);
        free(siridb);
    }
}

static void siridb_add(siridb_list_t * siridb_list, siridb_t * siridb)
{
    if (siridb_list->siridb == NULL)
    {
        siridb_list->siridb = siridb;
        return;
    }

    while (siridb_list->next != NULL)
        siridb_list = siridb_list->next;

    siridb_list->next = (siridb_list_t *) malloc(sizeof(siridb_list_t));
    siridb_list->next->siridb = siridb;
    siridb_list->next->next = NULL;
}
