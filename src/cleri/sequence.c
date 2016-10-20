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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static void SEQUENCE_free(cleri_object_t * cl_object);

static cleri_node_t * SEQUENCE_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL and in case an error has occurred.
 */
cleri_object_t * cleri_sequence(
        uint32_t gid,
        size_t len,
        ...)
{
    va_list ap;
    cleri_object_t * cl_object = cleri_object_new(
            CLERI_TP_SEQUENCE,
            &SEQUENCE_free,
            &SEQUENCE_parse);

    if (cl_object == NULL)
    {
        return NULL;
    }

    cl_object->via.sequence =
            (cleri_sequence_t *) malloc(sizeof(cleri_sequence_t));

    if (cl_object->via.sequence == NULL)
    {
        free(cl_object);
        return NULL;
    }

    cl_object->via.sequence->gid = gid;
    cl_object->via.sequence->olist = cleri_olist_new();

    if (cl_object->via.sequence->olist == NULL)
    {
        cleri_object_decref(cl_object);
        return NULL;
    }

    va_start(ap, len);
    while(len--)
    {
        if (cleri_olist_append(
                cl_object->via.sequence->olist,
                va_arg(ap, cleri_object_t *)))
        {
            cleri_object_decref(cl_object);
            return NULL;
        }
    }
    va_end(ap);

    return cl_object;
}

/*
 * Destroy sequence object.
 */
static void SEQUENCE_free(cleri_object_t * cl_object)
{
    cleri_olist_free(cl_object->via.sequence->olist);
    free(cl_object->via.sequence);
}

/*
 * Returns a node or NULL. In case of an error cleri_err is set to -1.
 */
static cleri_node_t * SEQUENCE_parse(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;

    olist = cl_obj->via.sequence->olist;
    if ((node = cleri_node_new(cl_obj, parent->str + parent->len, 0)) == NULL)
    {
    	cleri_err = -1;
        return NULL;
    }

    while (olist != NULL)
    {
        rnode = cleri__parser_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI_EXP_MODE_REQUIRED);
        if (rnode == NULL)
        {
            cleri_node_free(node);
            return NULL;
        }
        olist = olist->next;
    }

    parent->len += node->len;
    if (cleri_children_add(parent->children, node))
    {
		 /* error occurred, reverse changes set mg_node to NULL */
		cleri_err = -1;
		parent->len -= node->len;
		cleri_node_free(node);
		node = NULL;
    }
    return node;
}
