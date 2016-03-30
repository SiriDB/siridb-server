/*
 * optional.c - cleri optional element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/optional.h>
#include <logger/logger.h>
#include <stdlib.h>

static void cleri_free_optional(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_optional(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_optional(uint32_t gid, cleri_object_t * cl_obj)
{
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_OPTIONAL,
            &cleri_free_optional,
            &cleri_parse_optional);
    cl_object->cl_obj->optional =
            (cleri_optional_t *) malloc(sizeof(cleri_optional_t));
    cl_object->cl_obj->optional->gid = gid;
    cl_object->cl_obj->optional->cl_obj = cl_obj;
    return cl_object;
}

static void cleri_free_optional(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_object(grammar, cl_obj->cl_obj->optional->cl_obj);
    free(cl_obj->cl_obj->optional);
}

static cleri_node_t * cleri_parse_optional(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_node_t * rnode;

    node = cleri_new_node(cl_obj, parent->str + parent->len, 0);
    rnode = cleri_walk(
            pr,
            node,
            cl_obj->cl_obj->optional->cl_obj,
            rule,
            CLERI_EXP_MODE_OPTIONAL);
    if (rnode != NULL)
    {
        parent->len += node->len;
        cleri_children_add(parent->children, node);
        return node;
    }
    cleri_free_node(node);
    return CLERI_EMPTY_NODE;
}
