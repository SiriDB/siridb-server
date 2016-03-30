/*
 * object.c - each cleri element is a cleri object.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/object.h>
#include <logger/logger.h>
#include <stdlib.h>

static cleri_object_t end_of_statement =
    {CLERI_TP_END_OF_STATEMENT, NULL, NULL, NULL};

cleri_object_t * CLERI_END_OF_STATEMENT = &end_of_statement;

cleri_object_t * cleri_new_object(
        int tp,
        cleri_free_object_t free_object,
        cleri_parse_object_t parse_object)
{
    cleri_object_t * cl_object;

    cl_object = (cleri_object_t *) malloc(sizeof(cleri_object_t));
    cl_object->tp = tp;
    cl_object->cl_obj =
            (cleri_object_u *) malloc(sizeof(cleri_object_u));
    cl_object->free_object = free_object;
    cl_object->parse_object = parse_object;
    return cl_object;
}

void cleri_free_object(cleri_grammar_t * grammar, cleri_object_t * cl_object)
{
    /* Grammar keep a 'olist' of objects who are freed to prevent freeing
     * them twice. When this list is cleaned, we pass NULL for grammar.
     */
    if (    grammar == NULL ||
            cleri_olist_has(grammar->olist, cl_object) ||
            cl_object->tp == CLERI_TP_THIS)
        return;

    /* add this object to grammars olist so we will not free the
     * object again.
     */
    cleri_olist_add(grammar->olist, cl_object);

    if (cl_object->free_object != NULL)
        (*cl_object->free_object)(grammar, cl_object);

    free((cl_object)->cl_obj);
    free(cl_object);
}
