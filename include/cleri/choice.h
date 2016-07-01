/*
 * choice.h - this cleri element can hold other elements and the grammar
 *            has to choose one of them.
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

typedef struct cleri_choice_s
{
    uint32_t gid;
    int most_greedy;
    cleri_olist_t * olist;
} cleri_choice_t;

cleri_object_t * cleri_choice(
        uint32_t gid,
        int most_greedy,
        size_t len,
        ...);
