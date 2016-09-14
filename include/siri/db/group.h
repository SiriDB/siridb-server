/*
 * group.h - Group (saved regular expressions).
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

#include <slist/slist.h>
#include <siri/db/series.h>
#include <pcre.h>

#define GROUP_FLAG_NEW 1
#define GROUP_FLAG_DROPPED 2

typedef struct siridb_series_s siridb_series_t;

typedef struct siridb_group_s
{
    uint16_t ref;
    uint16_t flags;
    uint32_t nseries;  /* total series (needs an update from all pools) */
    char * name;
    char * source;  /* pattern/flags representation */
    slist_t * series;
    pcre * regex;
    pcre_extra * regex_extra;
} siridb_group_t;

siridb_group_t * siridb_group_new(
        const char * name,
        const char * source,
        size_t source_len,
        char * err_msg);
void siridb_group_incref(siridb_group_t * group);
void siridb_group_decref(siridb_group_t * group);
void siridb_group_cleanup(siridb_group_t * group);
int siridb_group_test_series(siridb_group_t * group, siridb_series_t * series);
int siridb_group_cexpr_cb(siridb_group_t * group, cexpr_condition_t * cond);
void siridb_group_prop(siridb_group_t * group, qp_packer_t * packer, int prop);
