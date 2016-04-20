/*
 * node.c - node is created while parsing a string. a node old the result
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
#include <cleri/node.h>
#include <logger/logger.h>
#include <stdlib.h>

static cleri_node_t Cleri_empty_node = {
        .children=NULL,
        .cl_obj=NULL,
        .len=0,
        .str=NULL
};

cleri_node_t * CLERI_EMPTY_NODE = &Cleri_empty_node;

cleri_node_t * cleri_new_node(
        cleri_object_t * cl_obj,
        const char * str,
        size_t len)
{
    cleri_node_t * node;
    node = (cleri_node_t *) malloc(sizeof(cleri_node_t));
    node->cl_obj = cl_obj;
    node->ref = 1;

    node->str = str;
    node->len = len;
    if (cl_obj == NULL || cl_obj->tp <= CLERI_TP_THIS)
        /* NULL when initializing the root node but we do need children */
        node->children = cleri_new_children();
    else
        /* we do not need children for some objects */
        node->children = NULL;

    return node;
}

void cleri_free_node(cleri_node_t * node)
{
    /* node can be NULL or this could be an CLERI_EMPTY_NODE */
    if (node == NULL || node == CLERI_EMPTY_NODE || --node->ref)
        return;

    cleri_free_children(node->children);
    free(node);
}

