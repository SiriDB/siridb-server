/*
 * keyword.h - cleri keyword element
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

#include <stddef.h>
#include <inttypes.h>
#include <cleri/object.h>

typedef struct cleri_object_s cleri_object_t;

typedef struct cleri_keyword_s
{
    uint32_t gid;
    const char * keyword;
    int ign_case;
    size_t len;
} cleri_keyword_t;

cleri_object_t * cleri_keyword(
        uint32_t gid,
        const char * keyword,
        int ign_case);
