/*
 * prio.c - cleri prio element. (this element create a cleri rule object
 *          holding this prio element)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/prio.h>
#include <logger/logger.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static void cleri_free_prio(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t *  cleri_parse_prio(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_prio(
        uint32_t gid,
        size_t len,
        ...)
{
    va_list ap;
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_PRIO,
            &cleri_free_prio,
            &cleri_parse_prio);
    cl_object->cl_obj->prio =
            (cleri_prio_t *) malloc(sizeof(cleri_prio_t));
    cl_object->cl_obj->prio->gid = 0;
    cl_object->cl_obj->prio->olist = cleri_new_olist();

    va_start(ap, len);
    while(len--)
        cleri_olist_add(
                cl_object->cl_obj->prio->olist,
                va_arg(ap, cleri_object_t *));
    va_end(ap);

    return cleri_rule(gid, cl_object);
}

static void cleri_free_prio(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_olist(grammar, cl_obj->cl_obj->prio->olist);
    free(cl_obj->cl_obj->prio);
}

static cleri_node_t *  cleri_parse_prio(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;
    cleri_rule_tested_t * tested;
    const char * str = parent->str + parent->len;

    /* initialize and return rule test, or return an existing test
     * if *str is already in tested */
    cleri_init_rule_tested(&tested, rule->tested, str);

    olist = cl_obj->cl_obj->prio->olist;

    while (olist != NULL)
    {
        node = cleri_new_node(cl_obj, str, 0);
        rnode = cleri_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);
        if (rnode != NULL &&
                (tested->node == NULL || node->len > tested->node->len))
        {
            cleri_free_node(tested->node);
            tested->node = node;
        }
        else
            cleri_free_node(node);
        olist = olist->next;
    }
    if (tested->node != NULL)
    {
        parent->len += tested->node->len;
        cleri_children_add(parent->children, tested->node);
        return tested->node;
    }
    return NULL;
}

