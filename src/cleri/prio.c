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
#include <cleri/expecting.h>
#include <cleri/olist.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define PRIO_MAX_RECUSION_DEPTH 200

static void PRIO_free(cleri_object_t * cl_obj);

static cleri_node_t *  PRIO_parse(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_object_t * cleri_prio(
        uint32_t gid,
        size_t len,
        ...)
{
    va_list ap;

    cleri_object_t * cl_object = cleri_object_new(
            CLERI_TP_PRIO,
            &PRIO_free,
            &PRIO_parse);

    if (cl_object == NULL)
    {
        return NULL;  /* signal is set */
    }

    cl_object->via.prio =
            (cleri_prio_t *) malloc(sizeof(cleri_prio_t));

    if (cl_object->via.prio == NULL)
    {
        free(cl_object);
        return NULL;
    }

    cl_object->via.prio->gid = 0;
    cl_object->via.prio->olist = cleri_olist_new();

    if (cl_object->via.prio->olist == NULL)
    {
        cleri_object_decref(cl_object);
        return NULL;
    }

    va_start(ap, len);
    while(len--)
    {
        if (cleri_olist_append(
                cl_object->via.prio->olist,
                va_arg(ap, cleri_object_t *)))
        {
            cleri_object_decref(cl_object);
            return NULL;
        }
    }
    va_end(ap);

    return cleri_rule(gid, cl_object);
}

/*
 * Destroy prio object.
 */
static void PRIO_free(cleri_object_t * cl_object)
{
    cleri_olist_free(cl_object->via.prio->olist);
    free(cl_object->via.prio);
}

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t *  PRIO_parse(
        cleri_parse_t * pr,
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
    if (	rule->depth++ > PRIO_MAX_RECUSION_DEPTH ||
    		cleri_rule_init(&tested, rule->tested, str) == CLERI_RULE_ERROR)
    {
    	pr->is_valid = -1;
        return NULL;
    }

    olist = cl_obj->via.prio->olist;

    while (olist != NULL)
    {
        if ((node = cleri_node_new(cl_obj, str, 0)) == NULL)
        {
        	pr->is_valid = -1;
            return NULL;
        }
        rnode = cleri__parse_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);
        if (rnode != NULL &&
                (tested->node == NULL || node->len > tested->node->len))
        {
            cleri_node_free(tested->node);
            tested->node = node;
        }
        else
        {
            cleri_node_free(node);
        }
        olist = olist->next;
    }
    if (tested->node != NULL)
    {
        parent->len += tested->node->len;
        if (cleri_children_add(parent->children, tested->node))
        {
			 /* error occurred, reverse changes set mg_node to NULL */
        	pr->is_valid = -1;
			parent->len -=  tested->node->len;
			cleri_node_free(tested->node);
			tested->node = NULL;
        }
        return tested->node;
    }
    return NULL;
}

