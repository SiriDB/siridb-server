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

typedef struct cleri_object_s cleri_object_t;

typedef struct cleri_list_s
{
    uint32_t gid;
    cleri_object_t * cl_obj;
    cleri_object_t * delimiter;
    size_t min;
    size_t max;
    int opt_closing;
} cleri_list_t;

cleri_object_t * cleri_list(
        uint32_t gid,
		cleri_object_t * cl_obj,
		cleri_object_t * delimiter,
        size_t min,
        size_t max,
        int opt_closing);

