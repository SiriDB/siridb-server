/*
 * tags.h - Tags.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-06-2017
 *
 */
#pragma once

#include <inttypes.h>
#include <ctree/ctree.h>
#include <uv.h>


#define TAGS_FLAG_DROPPED_SERIES 1
#define TAGS_FLAG_REQUIRE_SAVE 2

typedef struct siridb_tags_s
{
    uint8_t _pad0;
    uint8_t flags;
    uint8_t ref;
    char * path;
    ct_t * tags;
    uv_mutex_t mutex;
    uint64_t next_id;
} siridb_tags_t;

siridb_tags_t * siridb_tags_new(const char * dbpath);
void siridb_tags_decref(siridb_tags_t * tags);
