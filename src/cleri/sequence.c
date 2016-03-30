/*
 * sequence.c - cleri sequence element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/sequence.h>
#include <logger/logger.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static void cleri_free_sequence(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_sequence(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_sequence(
        uint32_t gid,
        size_t len,
        ...)
{
    va_list ap;
    cleri_object_t * cl_object;

    cl_object = cleri_new_object(
            CLERI_TP_SEQUENCE,
            &cleri_free_sequence,
            &cleri_parse_sequence);
    cl_object->cl_obj->sequence =
            (cleri_sequence_t *) malloc(sizeof(cleri_sequence_t));
    cl_object->cl_obj->sequence->gid = gid;
    cl_object->cl_obj->sequence->olist = cleri_new_olist();

    va_start(ap, len);
    while(len--)
        cleri_olist_add(
                cl_object->cl_obj->sequence->olist,
                va_arg(ap, cleri_object_t *));
    va_end(ap);

    return cl_object;
}

static void cleri_free_sequence(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_olist(grammar, cl_obj->cl_obj->sequence->olist);
    free(cl_obj->cl_obj->sequence);
}

static cleri_node_t * cleri_parse_sequence(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;

    olist = cl_obj->cl_obj->sequence->olist;
    node = cleri_new_node(cl_obj, parent->str + parent->len, 0);

    while (olist != NULL)
    {
        rnode = cleri_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);
        if (rnode == NULL)
        {
            cleri_free_node(node);
            return NULL;
        }
        olist = olist->next;
    }
    parent->len += node->len;
    cleri_children_add(parent->children, node);
    return node;
}
