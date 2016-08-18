/*
 * groups.h - Groups (saved regular expressions).
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 16-08-2016
 *
 */
#pragma once

#include <ctree/ctree.h>
#include <slist/slist.h>
#include <uv.h>
#include <siri/db/db.h>

#define GROUPS_FLAG_INIT 1
#define GROUPS_FLAG_NEW_GROUP 2
#define GROUPS_FLAG_NEW_SERIES 4
#define GROUPS_FLAG_DROPPED_SERIES 8

typedef struct siridb_groups_s
{
    uint8_t flags;
    ct_t * groups;
    char * fn;
    uv_mutex_t mutex;
    slist_t * nseries;  /* list of series we need to assign to groups */
    slist_t * ngroups;  /* list of groups which need initialization */
} siridb_groups_t;
