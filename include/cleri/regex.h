/*
 * regex.h - cleri regular expression element.
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

#include <pcre.h>
#include <stddef.h>
#include <inttypes.h>
#include <cleri/object.h>

typedef struct cleri_object_s cleri_object_t;

typedef struct cleri_regex_s
{
    uint32_t gid;
    pcre * regex;
    pcre_extra * regex_extra;
} cleri_regex_t;

cleri_object_t * cleri_regex(
        uint32_t gid,
        const char * pattern);


