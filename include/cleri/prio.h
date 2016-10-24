/*
 * prio.h - cleri prio element. (this element create a cleri rule object
 *          holding this prio element)
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
#include <cleri/rule.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_olist_s cleri_olist_t;

typedef struct cleri_prio_s
{
    uint32_t gid;
    cleri_olist_t * olist;
} cleri_prio_t;

cleri_object_t * cleri_prio(
        uint32_t gid,
        size_t len,
        ...);

