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

struct cleri_object_s;

typedef struct cleri_keyword_s
{
    uint32_t gid;
    const char * keyword;
    int ign_case;
    size_t len;
} cleri_keyword_t;

struct cleri_object_s * cleri_keyword(
        uint32_t gid,
        const char * keyword,
        int ign_case);
