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

typedef enum
{
    GROUPS_INIT,
    GROUPS_RUNNING,
    GROUPS_STOPPING,
    GROUPS_CLOSED
} siridb_groups_status_t;


#define GROUPS_FLAG_NEW_GROUP 1
#define GROUPS_FLAG_NEW_SERIES 2
#define GROUPS_FLAG_DROPPED_SERIES 4


typedef struct siridb_groups_s
{
    uint8_t status;
    uint8_t flags;
    char * fn;
    ct_t * groups;
    slist_t * nseries;  /* list of series we need to assign to groups */
    slist_t * ngroups;  /* list of groups which need initialization */
    uv_mutex_t mutex;
    uv_work_t work;
} siridb_groups_t;
