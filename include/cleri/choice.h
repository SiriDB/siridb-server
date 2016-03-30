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

struct cleri_object_s;
struct cleri_olist_s;

typedef struct cleri_choice_s
{
    uint32_t gid;
    int most_greedy;
    struct cleri_olist_s * olist;
} cleri_choice_t;

struct cleri_object_s * cleri_choice(
        uint32_t gid,
        int most_greedy,
        size_t len,
        ...);
