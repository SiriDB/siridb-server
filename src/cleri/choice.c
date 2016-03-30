/*
 * choice.c - this cleri element can hold other elements and the grammar
 *            has to choose one of them.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/choice.h>
#include <logger/logger.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static void cleri_free_choice(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t * cleri_parse_choice(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_node_t * parse_most_greedy(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_node_t * parse_first_match(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_choice(
        uint32_t gid,
        int most_greedy,
        size_t len,
        ...)
{
    va_list ap;
    cleri_object_t * cl_object;
    cleri_olist_t * olist;

    olist = cleri_new_olist();
    va_start(ap, len);
    while(len--)
        cleri_olist_add(olist, va_arg(ap, cleri_object_t *));
    va_end(ap);

    cl_object = cleri_new_object(
            CLERI_TP_CHOICE,
            &cleri_free_choice,
            &cleri_parse_choice);
    cl_object->cl_obj->choice =
            (cleri_choice_t *) malloc(sizeof(cleri_choice_t));
    cl_object->cl_obj->choice->gid = gid;
    cl_object->cl_obj->choice->most_greedy = most_greedy;
    cl_object->cl_obj->choice->olist = olist;

    return cl_object;
}

static void cleri_free_choice(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    cleri_free_olist(grammar, cl_obj->cl_obj->choice->olist);
    free(cl_obj->cl_obj->choice);
}

static cleri_node_t * cleri_parse_choice(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    return (cl_obj->cl_obj->choice->most_greedy) ?
            parse_most_greedy(pr, parent, cl_obj, rule) :
            parse_first_match(pr, parent, cl_obj, rule);
}

static cleri_node_t * parse_most_greedy(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;
    cleri_node_t * mg_node = NULL;
    const char * str = parent->str + parent->len;

    olist = cl_obj->cl_obj->choice->olist;
    while (olist != NULL)
    {
        node = cleri_new_node(cl_obj, str, 0);
        rnode = cleri_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);
        if (rnode != NULL && (mg_node == NULL || node->len > mg_node->len))
        {
            cleri_free_node(mg_node);
            mg_node = node;
        }
        else
            cleri_free_node(node);
        olist = olist->next;
    }
    if (mg_node != NULL)
    {
        parent->len += mg_node->len;
        cleri_children_add(parent->children, mg_node);
    }
    return mg_node;
}

static cleri_node_t * parse_first_match(
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
        if (rnode != NULL)
        {
            parent->len += node->len;
            cleri_children_add(parent->children, node);
            return node;
        }
        olist = olist->next;
    }
    cleri_free_node(node);
    return NULL;
}
