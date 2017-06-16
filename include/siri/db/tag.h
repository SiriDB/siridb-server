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

typedef struct siridb_tag_s
{
    uint16_t ref;
    uint16_t flags;
    uint32_t n;     /* total series (needs an update from all pools) */
    char * name;
    char * fn;
    imap_t * series;
} siridb_tag_t;


void siridb__tag_decref(siridb_tag_t * group);
void siridb__tag_free(siridb_tag_t * group);

#define siridb_tag_incref(tag) tag->ref++
#define siridb_tag_decref(tag__) \
		if (!--tag__->ref) siridb__tag_free(tag__)
