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

#include <cleri/cleri.h>

typedef struct siridb_presuf_s siridb_presuf_t;

typedef struct siridb_presuf_s
{
    char * prefix;
    char * suffix;
    size_t len;  /* prefix len + suffix len + terminator char */
    siridb_presuf_t * prev;
} siridb_presuf_t;


void siridb_presuf_free(siridb_presuf_t * presuf);
siridb_presuf_t * siridb_presuf_add(
        siridb_presuf_t ** presuf,
        cleri_node_t * node);
int siridb_presuf_is_unique(siridb_presuf_t * presuf);
void siridb_presuf_cleanup(void);
const char * siridb_presuf_name(
        siridb_presuf_t * presuf,
        const char * name,
        size_t len);
