/*
 * node.h - node is created while parsing a string. a node old the result
 *          for one element.
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
#include <cleri/object.h>
#include <cleri/children.h>
#include <inttypes.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_children_s cleri_children_t;

typedef struct cleri_node_s
{
    cleri_object_t * cl_obj;
    const char * str;
    size_t len;
    uint_fast8_t ref;
    int64_t result;
    cleri_children_t * children;
} cleri_node_t;


cleri_node_t * cleri_node_new(
        cleri_object_t * cl_obj,
        const char * str,
        size_t len);

void cleri_node_free(cleri_node_t * node);

cleri_node_t * CLERI_EMPTY_NODE;
