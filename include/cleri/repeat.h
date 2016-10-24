/*
 * repeat.h - cleri regular repeat element.
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
#include <stdbool.h>
#include <inttypes.h>
#include <cleri/object.h>

typedef struct cleri_object_s cleri_object_t;

typedef struct cleri_repeat_s
{
    uint32_t gid;
    struct cleri_object_s * cl_obj;
    size_t min;
    size_t max;
} cleri_repeat_t;


cleri_object_t * cleri_repeat(
        uint32_t gid,
        struct cleri_object_s * cl_obj,
        size_t min,
        size_t max);

