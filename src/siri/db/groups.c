/*
 * groups.c - Groups (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/groups.h>
#include <siri/err.h>
#include <stdlib.h>
#include <slist/slist.h>
#include <siri/db/misc.h>
#include <siri/db/db.h>

#define SIRIDB_GROUPS_SCHEMA 1
#define SIRIDB_GROUPS_FN "groups.dat"
#define GROUPS_LOOP_SLEEP 2

static int GROUPS_load(siridb_groups_t * groups);
static void GOUPS_free(siridb_groups_t * groups);
static void GROUPS_loop(uv_work_t * work);
static void GROUPS_loop_finish(uv_work_t * work, int status);

siridb_groups_t * siridb_groups_new(siridb_t * siridb)
{
    siridb_groups_t * groups =
            (siridb_groups_t *) malloc(sizeof(siridb_groups_t));
    if (groups == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        groups->fn = NULL;
        groups->groups = ct_new();
        groups->nseries = slist_new(SLIST_DEFAULT_SIZE);

        if (groups->groups == NULL || groups->nseries == NULL)
        {
            siridb_groups_free(groups);
            groups = NULL; /* signal is raised */
        }
        else if (asprintf(
                    &groups->fn,
                    "%s%s",
                    siridb->dbpath,
                    SIRIDB_GROUPS_FN) < 0 || GROUPS_load(groups))
        {
            ERR_ALLOC
            siridb_groups_free(groups);
            groups = NULL;
        }
        else
        {
            groups->status = GROUPS_INIT;
            groups->flags = 0;
            uv_queue_work(
                    siri.loop,
                    &groups->work,
                    GROUPS_loop,
                    GROUPS_loop_finish);
        }
    }
    return groups;
}

int siridb_groups_add_series(
        siridb_groups_t * groups,
        siridb_series_t * series)
{
    if (slist_append_safe(groups->nseries, series))
    {
        return -1;
    }

    siridb_series_incref(series);
    groups->flags |= GROUPS_FLAG_NEW_SERIES;

    return 0;
}

int siridb_groups_add_group(
        siridb_groups_t * groups,
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg)
{
    int rc;
    siridb_group_t * group = siridb_group_new(
            name,
            source,
            source_len,
            err_msg);

    if (group == NULL)
    {
        return -1;  /* err_msg is set and a SIGNAL is possibly raised */
    }

    rc = ct_add(groups->groups, name, group);

    switch (rc)
    {
    case CT_EXISTS:
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Group '%s' already exists.",
                name);
        break;

    case CT_ERR:
        sprintf(err_msg, "Memory allocation error.");
        break;

    case CT_OK:
        if (slist_append_safe(groups->ngroups, group))
        {
            sprintf(err_msg, "Memory allocation error.");
            rc = -1;
        }
        else
        {
            siridb_group_incref(group);
            groups->flags |= GROUPS_FLAG_NEW_GROUP;
        }
        break;

    default:
        assert (0);
        break;
    }

    if (rc != CT_OK)
    {
        siridb_group_decref(group);
    }

    return rc;
}

inline void siridb_groups_destroy(siridb_groups_t * groups)
{
    groups->status = GROUPS_STOPPING;
}

static void GOUPS_free(siridb_groups_t * groups)
{
#ifdef DEBUG
    LOC("Free groups");
#endif
    free(groups->fn);

    if (groups->nseries != NULL)
    {
        for (size_t i = 0; i < groups->nseries->len; i++)
        {
            siridb_series_decref((siridb_series_t *) groups->nseries->data[i]);
        }
        slist_free(groups->nseries);
    }

    if (groups->groups != NULL)
    {
        ct_free(groups->groups, (ct_free_cb) siridb_group_decref);
    }

    if (groups->ngroups != NULL)
    {
        for (size_t i = 0; i < groups->ngroups->len; i++)
        {
            siridb_group_decref((siridb_group_t *) groups->ngroups->data[i]);
        }
        slist_free(groups->nseries);
    }
}

static void GROUPS_loop(uv_work_t * work)
{
    siridb_t * siridb = (siridb_t *) work->data;
    siridb_groups_t * groups = siridb->groups;

    while (groups->status != GROUPS_STOPPING)
    {
        sleep(GROUPS_LOOP_SLEEP);

        switch((siridb_groups_status_t) groups->status)
        {
        case GROUPS_INIT:
            break;

        case GROUPS_RUNNING:
            break;

        case GROUPS_STOPPING:
            break;

        default:
            assert (0);
            break;


        }
    }

    groups->status = GROUPS_CLOSED;
}

static void GROUPS_loop_finish(uv_work_t * work, int status)
{
    /*
     * Main Thread
     */
    siridb_t * siridb = (siridb_t *) work->data;

    /* free groups */
    GOUPS_free(siridb->groups);
}

static int GROUPS_load(siridb_groups_t * groups)
{
    int rc = 0;

    if (!xpath_file_exist(groups->fn))
    {
        return rc; // no groups file, nothing to do
    }

    qp_unpacker_t * unpacker = siridb_misc_open_schema_file(
            SIRIDB_GROUPS_SCHEMA,
            groups->fn);

    if (unpacker == NULL)
    {
        rc = -1;
    }
    else
    {
        qp_obj_t qp_name, qp_source;
        char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

        while ( !rc &&
                qp_is_array(qp_next(unpacker, NULL)) &&
                qp_next(unpacker, &qp_name) == QP_RAW &&
                qp_is_raw_term(&qp_name) &&
                qp_next(unpacker, &qp_source) == QP_RAW)
        {
            rc = siridb_groups_add_group(
                    groups,
                    qp_name.via.raw,
                    qp_source.via.raw,
                    qp_source.len,
                    err_msg);
        }
        qp_unpacker_ff_free(unpacker);
    }

    return rc;
}
