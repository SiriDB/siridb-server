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
#include <logger/logger.h>
#include <stdlib.h>
#include <assert.h>


static cleri_exp_modes_t * cleri_new_exp_modes(const char * str);
static void cleri_empty_exp(cleri_expecting_t * expecting);
static int cleri_exp_get_mode(cleri_exp_modes_t * modes, const char * str);
static void cleri_exp_shift_modes(
        cleri_exp_modes_t ** modes,
        const char * str);
static void cleri_free_exp_modex(cleri_exp_modes_t * modes);


cleri_expecting_t * cleri_new_expecting(const char * str)
{
    cleri_expecting_t * expecting;
    expecting = (cleri_expecting_t *) malloc(sizeof(cleri_expecting_t));
    expecting->str = str;
    expecting->required = cleri_new_olist();
    expecting->optional = cleri_new_olist();
    expecting->modes = cleri_new_exp_modes(str);

    return expecting;
}

void cleri_expecting_update(
        cleri_expecting_t * expecting,
        cleri_object_t * cl_obj,
        const char * str)
{
    if (str > expecting->str)
    {
        cleri_empty_exp(expecting);
        expecting->str = str;
        cleri_exp_shift_modes(&(expecting->modes), str);
    }
    if (expecting->str == str)
    {
        if (cleri_exp_get_mode(expecting->modes, str))
            /* true (1) is required */
            cleri_olist_add(expecting->required, cl_obj);
        else
            /* false (0) is optional */
            cleri_olist_add(expecting->optional, cl_obj);
    }
}

void cleri_expecting_set_mode(
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
            return;
        }
    }
    current->next = (cleri_exp_modes_t *) malloc(sizeof(cleri_exp_modes_t));
    current->next->mode = mode;
    current->next->next = NULL;
    current->next->str = str;
}

void cleri_free_expecting(cleri_expecting_t * expecting)
{
    cleri_empty_exp(expecting);
    free(expecting->required);
    free(expecting->optional);
    cleri_free_exp_modex(expecting->modes);
    free(expecting);
}

void cleri_combine_expecting(cleri_expecting_t * expecting)
{
    cleri_olist_t * required = expecting->required;

    if (required->cl_obj == NULL)
    {
        cleri_empty_olist(expecting->required);
        free(expecting->required);
        expecting->required = expecting->optional;
    }
    else
    {
        while (required->next != NULL)
            required = required->next;
        required->next = expecting->optional;
    }
    expecting->optional = NULL;
}

void cleri_remove_from_expecting(cleri_expecting_t * expecting, uint32_t gid)
{
    cleri_olist_t * curr = expecting->required;
    cleri_olist_t * prev = NULL;

    if (curr == NULL || curr->cl_obj == NULL)
        return;

    for (; curr != NULL; prev = curr, curr = curr->next)
    {
        if (curr->cl_obj != NULL && curr->cl_obj->cl_obj->dummy->gid == gid)
        {
            if (prev == NULL)
                expecting->required = curr->next;
            else
                prev->next = curr->next;

            /* free current */
            free(curr);
            return;
        }
    }
}


static cleri_exp_modes_t * cleri_new_exp_modes(const char * str)
{
    cleri_exp_modes_t * modes;
    modes = (cleri_exp_modes_t *) malloc(sizeof(cleri_exp_modes_t));
    modes->mode = CLERI_EXP_MODE_REQUIRED;
    modes->next = NULL;
    modes->str = str;
    return modes;
}

static void cleri_exp_shift_modes(
        cleri_exp_modes_t ** modes,
        const char * str)
{
    cleri_exp_modes_t * next;

    while ((*modes)->next != NULL)
    {
        if ((*modes)->str == str)
            break;
        next = (*modes)->next;
        free(*modes);
        *modes = next;
    }
}

static void cleri_free_exp_modex(cleri_exp_modes_t * modes)
{
    cleri_exp_modes_t * next;
    while (modes != NULL)
    {
        next = modes->next;
        free(modes);
        modes = next;
    }
}

static int cleri_exp_get_mode(cleri_exp_modes_t * modes, const char * str)
{
    for (; modes != NULL; modes = modes->next)
        if (modes->str == str)
            return modes->mode;
    return CLERI_EXP_MODE_REQUIRED;
}

static void cleri_empty_exp(cleri_expecting_t * expecting)
{
    cleri_empty_olist(expecting->required);
    cleri_empty_olist(expecting->optional);
}
