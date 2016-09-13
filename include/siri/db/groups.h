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
    GROUPS_RUNNING,
    GROUPS_STOPPING,
    GROUPS_CLOSED
} siridb_groups_status_t;


#define GROUPS_FLAG_DROPPED_SERIES 4


typedef struct siridb_groups_s
{
    uint8_t status;
    uint8_t flags;
    uint8_t ref;
    char * fn;
    ct_t * groups;
    slist_t * nseries;  /* list of series we need to assign to groups */
    slist_t * ngroups;  /* list of groups which need initialization */
    uv_mutex_t mutex;
    uv_work_t work;
} siridb_groups_t;

siridb_groups_t * siridb_groups_new(siridb_t * siridb);
int siridb_groups_save(siridb_groups_t * groups);
void siridb_groups_destroy(siridb_groups_t * groups);
void siridb_groups_decref(siridb_groups_t * groups);
int siridb_groups_add_group(
        siridb_groups_t * groups,
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg);
int siridb_groups_add_series(
        siridb_groups_t * groups,
        siridb_series_t * series);
