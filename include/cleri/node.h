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

struct cleri_object_s;
struct cleri_children_s;

typedef struct cleri_node_s
{
    struct cleri_object_s * cl_obj;
    const char * str;
    size_t len;
    uint_fast8_t ref;
    int64_t result;
    struct cleri_children_s * children;
} cleri_node_t;


cleri_node_t * cleri_new_node(
        struct cleri_object_s * cl_obj,
        const char * str,
        size_t len);
void cleri_free_node(cleri_node_t * node);

cleri_node_t * CLERI_EMPTY_NODE;
