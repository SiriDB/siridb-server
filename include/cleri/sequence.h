/*
 * sequence.h - cleri sequence element.
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
#include <cleri/olist.h>

struct cleri_object_s;
struct cleri_olist_s;

typedef struct cleri_sequence_s
{
    uint32_t gid;
    struct cleri_olist_s * olist;
} cleri_sequence_t;

struct cleri_object_s * cleri_sequence(
        uint32_t gid,
        size_t len,
        ...);

