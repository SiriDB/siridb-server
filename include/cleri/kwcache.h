/*
 * kwcache.h - holds keyword regular expression result while parsing.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#pragma once

#include <cleri/object.h>
#include <cleri/parser.h>
#include <sys/types.h>

typedef struct cleri_parser_s cleri_parser_t;

typedef struct cleri_kwcache_s {
    size_t len;
    const char * str;
    struct cleri_kwcache_s * next;
} cleri_kwcache_t;

cleri_kwcache_t * cleri_kwcache_new(void);
ssize_t cleri_kwcache_match(
        cleri_parser_t * pr,
        const char * str);
void cleri_kwcache_free(cleri_kwcache_t * kwcache);




