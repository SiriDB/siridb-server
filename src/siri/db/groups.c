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

    asprintf(&siridb->groups->fn, "%s%s", siridb->dbpath, SIRIDB_GROUPS_FN);

    if (siridb->groups == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    if ((unpacker = qp_unpacker_from_file(fn)) == NULL)
    {
        return -1;  /* a signal is raised is case of a memory error */
    }

    return 0;
}


void siridb_groups_free(siridb_groups_t * groups)
{
    free(groups->fn);
    slist_free()
}
