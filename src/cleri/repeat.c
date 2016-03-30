/*
 * repeat.c - cleri regular repeat element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/repeat.h>
#include <logger/logger.h>
#include <stdlib.h>

static void cleri_free_repeat(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_repeat(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_repeat(
        uint32_t gid,
        cleri_object_t * cl_obj,
        size_t min,
        size_t max)
{
    /*
     * cl_ob :      object to repeat
     * min :        should be equal to or higher then 0.
     * max :        should be equal to or higher then 0 but when 0 it means
     *              unlimited.
     */
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_REPEAT,
            &cleri_free_repeat,
            &cleri_parse_repeat);
    cl_object->cl_obj->repeat =
            (cleri_repeat_t *) malloc(sizeof(cleri_repeat_t));
    cl_object->cl_obj->repeat->gid = gid;
    cl_object->cl_obj->repeat->cl_obj = cl_obj;

    cl_object->cl_obj->repeat->min = min;
    cl_object->cl_obj->repeat->max = max;

    return cl_object;
}

static void cleri_free_repeat(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_object(grammar, cl_obj->cl_obj->repeat->cl_obj);
    free(cl_obj->cl_obj->repeat);
}

static cleri_node_t * cleri_parse_repeat(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_node_t * rnode;
    size_t i;
    node = cleri_new_node(cl_obj, parent->str + parent->len, 0);

    for (i = 0;
         cl_obj->cl_obj->repeat->max == 0 || i < cl_obj->cl_obj->repeat->max;
         i++)
    {
        rnode = cleri_walk(
                pr,
                node,
                cl_obj->cl_obj->repeat->cl_obj,
                rule,
                i < cl_obj->cl_obj->repeat->min); // 1 = REQUIRED
        if (rnode == NULL)
            break;
    }

    if (i < cl_obj->cl_obj->repeat->min)
    {
        cleri_free_node(node);
        return NULL;
    }
    parent->len += node->len;
    cleri_children_add(parent->children, node);
    return node;
}
