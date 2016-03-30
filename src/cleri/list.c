/*
 * list.c - cleri list element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/list.h>
#include <logger/logger.h>
#include <stdlib.h>

static void cleri_free_list(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t *  cleri_parse_list(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_list(
        uint32_t gid,
        cleri_object_t * cl_obj,
        cleri_object_t * delimiter,
        size_t min,
        size_t max,
        int opt_closing)
{
    /*
     * cl_obj       :   object to repeat
     * delimiter    :   object (Usually a Token) as delimiter
     * min          :   should be equal to or higher then 0.
     * max          :   should be equal to or higher then 0 but when 0 it
     *                  means unlimited.
     * opt_closing  :   when set to true (1) the list can be closed with a
     *                  delimiter. when false (0) this is not allowed.
     */
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_LIST,
            &cleri_free_list,
            &cleri_parse_list);
    cl_object->cl_obj->list =
            (cleri_list_t *) malloc(sizeof(cleri_list_t));
    cl_object->cl_obj->list->gid = gid;
    cl_object->cl_obj->list->cl_obj = cl_obj;
    cl_object->cl_obj->list->delimiter = delimiter;
    cl_object->cl_obj->list->min = min;
    cl_object->cl_obj->list->max = max;
    cl_object->cl_obj->list->opt_closing = opt_closing;
    return cl_object;
}

static void cleri_free_list(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_object(grammar, cl_obj->cl_obj->list->cl_obj);
    cleri_free_object(grammar, cl_obj->cl_obj->list->delimiter);
    free(cl_obj->cl_obj->list);
}

static cleri_node_t *  cleri_parse_list(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_node_t * node;
    cleri_node_t * rnode;
    size_t i = 0;
    size_t j = 0;
    node = cleri_new_node(cl_obj, parent->str + parent->len, 0);
    while (1)
    {

        rnode = cleri_walk(
                pr,
                node,
                cl_obj->cl_obj->list->cl_obj,
                rule,
                i < cl_obj->cl_obj->list->min); // 1 = REQUIRED
        if (rnode == NULL)
            break;
        i++;
        rnode = cleri_walk(
                pr,
                node,
                cl_obj->cl_obj->list->delimiter,
                rule,
                i < cl_obj->cl_obj->list->min); // 1 = REQUIRED
        if (rnode == NULL)
            break;
        j++;
    }
    if (    i < cl_obj->cl_obj->list->min ||
            (cl_obj->cl_obj->list->max && i > cl_obj->cl_obj->list->max) ||
            ((cl_obj->cl_obj->list->opt_closing == 0) && i && i == j))
    {
        cleri_free_node(node);
        return NULL;
    }
    parent->len += node->len;
    cleri_children_add(parent->children, node);
    return node;
}
