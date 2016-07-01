/*
 * expecting.h - holds elements which the grammar expects at one position.
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
#pragma once

#include <cleri/olist.h>
#include <cleri/object.h>
#include <stdlib.h>
#include <inttypes.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_olist_s cleri_olist_t;

#define CLERI_EXP_MODE_OPTIONAL 0
#define CLERI_EXP_MODE_REQUIRED 1

typedef struct cleri_expecting_modes_s
{
    int mode;
    const char * str;
    struct cleri_expecting_modes_s * next;
} cleri_exp_modes_t;

typedef struct cleri_expecting_s
{
    const char * str;
    cleri_olist_t * required;
    cleri_olist_t * optional;
    cleri_exp_modes_t * modes;
} cleri_expecting_t;

cleri_expecting_t * cleri_expecting_new(const char * str);

int cleri_expecting_update(
        cleri_expecting_t * expecting,
        cleri_object_t * cl_obj,
        const char * str);

int cleri_expecting_set_mode(
        cleri_expecting_t * expecting,
        const char * str,
        int mode);

void cleri_expecting_free(cleri_expecting_t * expecting);
void cleri_expecting_combine(cleri_expecting_t * expecting);
void cleri_expecting_remove(cleri_expecting_t * expecting, uint32_t gid);
