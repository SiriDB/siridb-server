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

inline int GROUPS_free(siridb_group_t * group, void * args);

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
        else
        {
            if (asprintf(
                    &groups->fn,
                    "%s%s",
                    siridb->dbpath,
                    SIRIDB_GROUPS_FN) < 0 || GROUPS_load(groups))
            {
                ERR_ALLOC
                siridb_groups_free(groups);
                groups = NULL;
            }
        }
    }
    return groups;
}

int GROUPS_load(siridb_groups_t * groups)
{
    if (!xpath_file_exist(groups->fn))
    {
        return 0; // no groups file, nothing to do
    }

    qp_unpacker_t * unpacker = siridb_misc_open_schema_file(
            SIRIDB_GROUPS_SCHEMA,
            groups->fn);

    if (unpacker == NULL)
    {
        return -1;
    }

    qp_unpacker_ff_free(unpacker);

    return 0;
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
        siridb_group_free(group);
        break;

    case CT_ERR:
        sprintf(err_msg, "Memory allocation error.");
        siridb_group_free(group);
        break;

    case CT_OK:
        groups->flags |= GROUPS_FLAG_NEW_GROUP;
        break;

    default:
        assert (0);
        break;
    }

    return rc;

}

void siridb_groups_free(siridb_groups_t * groups)
{
    free(groups->fn);

    if (groups->nseries != NULL)
    {
        for (size_t i = 0; i < groups->nseries->len; i++)
        {
            siridb_decref_series((siridb_series_t *) groups->nseries->data[i]);
        }
        slist_free(groups->nseries);
    }

    if (groups->groups != NULL)
    {
        ct_free(groups->groups, (ct_free_cb) siridb_group_free);
    }
}

