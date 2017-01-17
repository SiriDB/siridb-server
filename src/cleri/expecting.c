/*
 * expecting.c - holds elements which the grammar expects at one position.
 *               this can be used for suggestions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/expecting.h>
#include <stdlib.h>
#include <assert.h>

static cleri_exp_modes_t * EXPECTING_modes_new(const char * str);
static void EXPECTING_empty(cleri_expecting_t * expecting);
static int EXPECTING_get_mode(cleri_exp_modes_t * modes, const char * str);
static void EXPECTING_shift_modes(
        cleri_exp_modes_t ** modes,
        const char * str);
static void EXPECTING_modes_free(cleri_exp_modes_t * modes);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_expecting_t * cleri_expecting_new(const char * str)
{
    cleri_expecting_t * expecting =
            (cleri_expecting_t *) malloc(sizeof(cleri_expecting_t));

    if (expecting != NULL)
    {
        expecting->str = str;

        if ((expecting->required = cleri_olist_new()) == NULL)
        {
            free(expecting);
            return NULL;
        }

        if ((expecting->optional = cleri_olist_new()) == NULL)
        {
            free(expecting->required);
            free(expecting);
            return NULL;
        }

        if ((expecting->modes = EXPECTING_modes_new(str)) == NULL)
        {
            free(expecting->optional);
            free(expecting->required);
            free(expecting);
            return NULL;
        }
    }

    return expecting;
}

/*
 * Returns 0 if the mode is set successful and -1 if an error has occurred.
 */
int cleri_expecting_update(
        cleri_expecting_t * expecting,
        cleri_object_t * cl_obj,
        const char * str)
{
    int rc = 0;

    if (str > expecting->str)
    {
        EXPECTING_empty(expecting);
        expecting->str = str;
        EXPECTING_shift_modes(&(expecting->modes), str);
    }

    if (expecting->str == str)
    {
        if (EXPECTING_get_mode(expecting->modes, str))
        {
            /* true (1) is required */
            rc = cleri_olist_append_nref(expecting->required, cl_obj);
        }
        else
        {
            /* false (0) is optional */
            rc = cleri_olist_append_nref(expecting->optional, cl_obj);
        }
    }

    return rc;
}

/*
 * Returns 0 if the mode is set successful and -1 if an error has occurred.
 */
int cleri_expecting_set_mode(
        cleri_expecting_t * expecting,
        const char * str,
        int mode)
{
    cleri_exp_modes_t * current = expecting->modes;
    for (; current->next != NULL; current = current->next)
    {
        if (current->str == str)
        {
            current->mode = mode && current->mode;
            return 0;
        }
    }
    current->next = (cleri_exp_modes_t *) malloc(sizeof(cleri_exp_modes_t));

    if (current->next == NULL)
    {
        return -1;
    }

    current->next->mode = mode;
    current->next->next = NULL;
    current->next->str = str;

    return 0;
}

/*
 * Destroy expecting object.
 */
void cleri_expecting_free(cleri_expecting_t * expecting)
{
    EXPECTING_empty(expecting);
    free(expecting->required);
    free(expecting->optional);
    EXPECTING_modes_free(expecting->modes);
    free(expecting);
}

/*
 * append optional to required and sets optional to NULL
 */
void cleri_expecting_combine(cleri_expecting_t * expecting)
{
    cleri_olist_t * required = expecting->required;

    if (required->cl_obj == NULL)
    {
        cleri_olist_empty(expecting->required);
        free(expecting->required);
        expecting->required = expecting->optional;
    }
    else
    {
        while (required->next != NULL)
        {
            required = required->next;
        }
        required->next = expecting->optional;
    }
    expecting->optional = NULL;
}

/*
 * removes an gid from expecting if the gid exists in the list.
 * note: we only look in required since this is where the final
 * expects are stored.
 */
void cleri_expecting_remove(cleri_expecting_t * expecting, uint32_t gid)
{
    cleri_olist_t * curr = expecting->required;
    cleri_olist_t * prev = NULL;

    if (curr == NULL || curr->cl_obj == NULL)
    {
        return;
    }

    for (; curr != NULL; prev = curr, curr = curr->next)
    {
        if (curr->cl_obj != NULL && curr->cl_obj->via.dummy->gid == gid)
        {
            if (prev == NULL)
            {
                expecting->required = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }

            /* free current */
            free(curr);
            return;
        }
    }
}

/*
 * Returns NULL in case an error has occurred.
 */
static cleri_exp_modes_t * EXPECTING_modes_new(const char * str)
{
    cleri_exp_modes_t * modes =
            (cleri_exp_modes_t *) malloc(sizeof(cleri_exp_modes_t));
    if (modes != NULL)
    {
        modes->mode = CLERI_EXP_MODE_REQUIRED;
        modes->next = NULL;
        modes->str = str;
    }
    return modes;
}

/*
 * shift from modes
 */
static void EXPECTING_shift_modes(
        cleri_exp_modes_t ** modes,
        const char * str)
{
    cleri_exp_modes_t * next;

    while ((*modes)->next != NULL)
    {
        if ((*modes)->str == str)
        {
            break;
        }
        next = (*modes)->next;
        free(*modes);
        *modes = next;
    }
}

/*
 * Destroy modes.
 */
static void EXPECTING_modes_free(cleri_exp_modes_t * modes)
{
    cleri_exp_modes_t * next;
    while (modes != NULL)
    {
        next = modes->next;
        free(modes);
        modes = next;
    }
}

/*
 * Return modes for a given position in str.
 */
static int EXPECTING_get_mode(cleri_exp_modes_t * modes, const char * str)
{
    for (; modes != NULL; modes = modes->next)
    {
        if (modes->str == str)
        {
            return modes->mode;
        }
    }
    return CLERI_EXP_MODE_REQUIRED;
}

/*
 * Empty both required and optional lists.
 */
static void EXPECTING_empty(cleri_expecting_t * expecting)
{
    cleri_olist_empty(expecting->required);
    cleri_olist_empty(expecting->optional);
}
