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
#include <siri/db/db.h>
#include <siri/db/tag.h>

#define SIRIDB_TAGS_PATH "tags/"

#define TAGS_FLAG_REQUIRE_SAVE 2

typedef struct siridb_s siridb_t;
typedef struct siridb_tag_s siridb_tag_t;

typedef struct siridb_tags_s
{
    uint16_t flags;
    uint16_t ref;
    uint32_t next_id;
    char * path;
    ct_t * tags;
    slist_t * cleanup;
    uv_mutex_t mutex;
} siridb_tags_t;

int siridb_tags_init(siridb_t * siridb);
void siridb_tags_decref(siridb_tags_t * tags);
siridb_tag_t * siridb_tags_add(siridb_tags_t * tags, const char * name);
sirinet_pkg_t * siridb_tags_pkg(siridb_tags_t * tags, uint16_t pid);
ct_t * siridb_tags_lookup(siridb_tags_t * tags);
int siridb__tags_cleanup(siridb_tags_t * tags);

#define siridb_tags_require_save(__tags, __tag) 		\
		__tags->flags |= TAGS_FLAG_REQUIRE_SAVE;		\
		__tag->flags |= TAG_FLAG_REQUIRE_SAVE

#define siridb_tags_cleanup(__tags)								\
		if (__tags->cleanup->len) siridb__tags_cleanup(__tags)
