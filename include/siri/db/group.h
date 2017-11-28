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

#define PCRE2_CODE_UNIT_WIDTH 8

#include <slist/slist.h>
#include <siri/db/series.h>
#include <pcre2.h>

#define GROUP_FLAG_INIT 1
#define GROUP_FLAG_DROPPED 2

typedef struct siridb_series_s siridb_series_t;

typedef struct siridb_group_s
{
    uint16_t ref;
    uint16_t flags;
    uint32_t n;     /* total series (needs an update from all pools) */
    char * name;
    char * source;  /* pattern/flags representation */
    slist_t * series;
    pcre2_code * regex;
    pcre2_match_data * match_data;
} siridb_group_t;

siridb_group_t * siridb_group_new(
        const char * source,
        size_t source_len,
        char * err_msg);
int siridb_group_set_name(
        siridb_groups_t * groups,
        siridb_group_t * group,
        const char * name,
        char * err_msg);
int siridb_group_update_expression(
        siridb_groups_t * groups,
        siridb_group_t * group,
        const char * source,
        size_t source_len,
        char * err_msg);
void siridb_group_cleanup(siridb_group_t * group);
int siridb_group_test_series(siridb_group_t * group, siridb_series_t * series);
int siridb_group_cexpr_cb(siridb_group_t * group, cexpr_condition_t * cond);
void siridb_group_prop(siridb_group_t * group, qp_packer_t * packer, int prop);
int siridb_group_is_remote_prop(uint32_t prop);
void siridb__group_decref(siridb_group_t * group);
void siridb__group_free(siridb_group_t * group);

#define siridb_group_incref(group) group->ref++
#define siridb_group_decref(group__) \
        if (!--group__->ref) siridb__group_free(group__)
