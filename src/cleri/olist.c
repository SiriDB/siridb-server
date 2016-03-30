/*
 * olist.c - linked list for keeping cleri objects.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/olist.h>
#include <logger/logger.h>
#include <stdlib.h>

cleri_olist_t * cleri_new_olist(void)
{
    cleri_olist_t * olist;
    olist = (cleri_olist_t *) malloc(sizeof(cleri_olist_t));
    olist->cl_obj = NULL;
    olist->next = NULL;
    return olist;
}

void cleri_olist_add(cleri_olist_t * olist, cleri_object_t * cl_object)
{
    if (olist->cl_obj == NULL)
    {
        olist->cl_obj = cl_object;
        olist->next = NULL;
        return;
    }

    while (olist->next != NULL)
        olist = olist->next;
    olist->next = (cleri_olist_t *) malloc(sizeof(cleri_olist_t));
    olist->next->cl_obj = cl_object;
    olist->next->next = NULL;
}

void cleri_free_olist(cleri_grammar_t * grammar, cleri_olist_t * olist)
{
    cleri_olist_t * next;
    while (olist != NULL)
    {
        next = olist->next;
        cleri_free_object(grammar, olist->cl_obj);
        free(olist);
        olist = next;
    }
}

void cleri_empty_olist(cleri_olist_t * olist)
{
    cleri_olist_t * current;

    if (olist == NULL)
        return;

    /* set root object to NULL but we do not need to free root */
    olist->cl_obj = NULL;

    /* set root next to NULL */
    current = olist->next;
    olist->next = NULL;

    /* cleanup all the rest */
    while (current != NULL)
    {
        olist = current->next;
        free(current);
        current = olist;
    }
}

int cleri_olist_has(cleri_olist_t * olist, cleri_object_t * cl_object)
{
    if (olist->cl_obj == NULL) return 0;

    while (olist != NULL)
    {
        if (olist->cl_obj == cl_object)
            return 1;
        olist = olist->next;
    }
    return 0;
}
