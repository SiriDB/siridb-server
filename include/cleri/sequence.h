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

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_olist_s cleri_olist_t;

typedef struct cleri_sequence_s
{
    uint32_t gid;
    cleri_olist_t * olist;
} cleri_sequence_t;

cleri_object_t * cleri_sequence(
        uint32_t gid,
        size_t len,
        ...);

