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
#include <stdlib.h>
#include <assert.h>

static cleri_object_t end_of_statement =
    {CLERI_TP_END_OF_STATEMENT, 1, NULL, NULL, {.dummy=NULL}};

cleri_object_t * CLERI_END_OF_STATEMENT = &end_of_statement;

/*
 * Returns NULL in case an error has occurred.
 */
cleri_object_t * cleri_object_new(
        cleri_object_tp tp,
        cleri_free_object_t free_object,
        cleri_parse_object_t parse_object)
{
    cleri_object_t * cl_object;

    cl_object = (cleri_object_t *) malloc(sizeof(cleri_object_t));
    if (cl_object != NULL)
    {
		cl_object->tp = tp;
		cl_object->ref = 1;
		cl_object->via.dummy = NULL;
		cl_object->free_object = free_object;
		cl_object->parse_object = parse_object;
    }
    return cl_object;
}

/*
 * Increment reference counter on cleri object.
 */
inline void cleri_object_incref(cleri_object_t * cl_object)
{
    cl_object->ref++;
}

/*
 * Decrement reference counter, except when the object is THIS because THIS is
 * allocated in stack memory and does not have a free call-back.
 * For any other object the free-callback will be called once.
 */
void cleri_object_decref(cleri_object_t * cl_object)
{
    if (cl_object->tp == CLERI_TP_THIS)
    {
        return;
    }

#ifdef DEBUG
    assert (cl_object->free_object != NULL);
#endif

    if (cl_object->via.dummy != NULL)
    {
        (*cl_object->free_object)(cl_object);
        cl_object->via.dummy = NULL;
        cleri_object_decref(cl_object);
    }

    if (!--cl_object->ref)
    {
        free(cl_object);
    }
}


