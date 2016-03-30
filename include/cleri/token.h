/*
 * token.h - cleri token element. note that one single char will parse
 *           slightly faster compared to tokens containing more characters.
 *           (be careful a token should not match the keyword regular
 *           expression)
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

typedef struct cleri_token_s
{
    uint32_t gid;
    const char * token;
    size_t len;
} cleri_token_t;

struct cleri_object_s * cleri_token(
        uint32_t gid,
        const char * token);

