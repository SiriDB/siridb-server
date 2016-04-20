/*
 * this.c - cleri THIS element. there should be only one single instance
 *          of this which can even be shared over different grammars.
 *          Always use this element using its constant CLERI_THIS and
 *          somewhere within a prio element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/this.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <cleri/expecting.h>

static cleri_node_t * cleri_parse_this(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_dummy_t cleri_dummy = {.gid=0};

static cleri_object_t cleri_this = {
        .tp=CLERI_TP_THIS,
        .free_object=NULL,
        .parse_object=&cleri_parse_this,
        .cl_obj=(cleri_object_u *)  &cleri_dummy};

cleri_object_t * CLERI_THIS = &cleri_this;

static cleri_node_t * cleri_parse_this(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_rule_tested_t * tested;
    const char * str = parent->str + parent->len;

    if (cleri_init_rule_tested(&tested, rule->tested, str))
    {
        node = cleri_new_node(cl_obj, str, 0);
        tested->node = cleri_walk(
                pr,
                node,
                rule->root_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);

        if (tested->node == NULL)
        {
            cleri_free_node(node);
            return NULL;
        }

    }
    else
    {
        node = tested->node;
        if (node == NULL)
            return NULL;
        node->ref++;
    }

    parent->len += tested->node->len;
    cleri_children_add(parent->children, node);
    return node;
}
