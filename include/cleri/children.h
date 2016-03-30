/*
 * children.h - linked list for keeping node results
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

#include <cleri/node.h>

struct cleri_node_s;

typedef struct cleri_children_s
{
    struct cleri_node_s * node;
    struct cleri_children_s * next;
} cleri_children_t;

cleri_children_t * cleri_new_children(void);
void cleri_free_children(cleri_children_t * children);
void cleri_children_add(
        cleri_children_t * children,
        struct cleri_node_s * node);
void cleri_empty_children(cleri_children_t * children);


