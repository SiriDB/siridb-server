/*
 * list.h - cleri list element.
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

struct cleri_object_s;

typedef struct cleri_list_s
{
    uint32_t gid;
    struct cleri_object_s * cl_obj;
    struct cleri_object_s * delimiter;
    size_t min;
    size_t max;
    int opt_closing;
} cleri_list_t;

struct cleri_object_s * cleri_list(
        uint32_t gid,
        struct cleri_object_s * cl_obj,
        struct cleri_object_s * delimiter,
        size_t min,
        size_t max,
        int opt_closing);

