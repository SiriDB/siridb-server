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

#define SIRIDB_GROUPS_SCHEMA 1
#define SIRIDB_GROUPS_FN "groups.dat"


siridb_groups_t * siridb_groups_new()
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
        groups->series = slist_new(SLIST_DEFAULT_SIZE);
        if (groups->groups == NULL || groups->series == NULL)
        {
            ERR_ALLOC
        }
    }
    return groups;
}

int siridb_groups_init(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;

    if (asprintf(
            &siridb->groups->fn,
            "%s%s",
            siridb->dbpath,
            SIRIDB_GROUPS_FN) < 0)
    {
        ERR_ALLOC
        return -1;
    }

    unpacker = siridb_misc_open_schema_file(
            SIRIDB_GROUPS_SCHEMA,
            siridb->groups->fn);

    if (unpacker == NULL)
    {
        return -1;
    }

    qp_obj_t * qp_name = qp_object_new();
    qp_obj_t * qp_source = qp_object_new();

    qp_object_free(qp_name);
    qp_object_free(qp_source);

    return 0;
}


void siridb_groups_free(siridb_groups_t * groups)
{
    free(groups->fn);

    /* TODO: decref series */
    slist_free(groups->series);
}
