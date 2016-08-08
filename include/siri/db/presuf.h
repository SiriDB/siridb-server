/*
 * presuf.h - Prefix and Suffix store.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-08-2016
 *
 */
#pragma once

#include <cleri/node.h>

typedef struct siridb_presuf_s siridb_presuf_t;

typedef struct siridb_presuf_s
{
    char * prefix;
    char * suffix;
    siridb_presuf_t * prev;
} siridb_presuf_t;


siridb_presuf_t * siridb_presuf_new(void);
void siridb_presuf_free(siridb_presuf_t * presuf);
siridb_presuf_t * siridb_presuf_add(
        siridb_presuf_t ** presuf,
        cleri_node_t * node);
const char * siridb_presuf_prefix_get(siridb_presuf_t * presuf);
const char * siridb_presuf_suffix_get(siridb_presuf_t * presuf);
int siridb_presuf_is_unique(siridb_presuf_t * presuf);

