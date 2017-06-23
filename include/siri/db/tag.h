/*
 * tag.h - Tag.
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
#include <imap/imap.h>
#include <uv.h>

#define TAG_FLAG_REQUIRE_SAVE 2

typedef struct siridb_tag_s
{
    uint16_t ref;
    uint16_t flags;
    uint32_t n;     /* total series (needs an update from all pools) */
    uint64_t id;
    char * name;
    char * fn;
    imap_t * series;
} siridb_tag_t;


void siridb__tag_decref(siridb_tag_t * group);
void siridb__tag_free(siridb_tag_t * group);
int siridb_tag_is_valid_fn(const char * fn);
siridb_tag_t * siridb_tag_load(siridb_t * siridb, const char * fn);
int siridb_tag_save(siridb_tag_t * tag, uv_mutex_t * lock);

#define siridb_tag_incref(tag) tag->ref++
#define siridb_tag_decref(tag__) \
		if (!--tag__->ref) siridb__tag_free(tag__)
