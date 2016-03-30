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

struct cleri_object_s;
struct cleri_olist_s;

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
    struct cleri_olist_s * required;
    struct cleri_olist_s * optional;
    struct cleri_expecting_modes_s * modes;
} cleri_expecting_t;

cleri_expecting_t * cleri_new_expecting(const char * str);
void cleri_expecting_update(
        cleri_expecting_t * expecting,
        struct cleri_object_s * cl_obj,
        const char * str);
void cleri_expecting_set_mode(
        cleri_expecting_t * expecting,
        const char * str,
        int mode);
void cleri_free_expecting(cleri_expecting_t * expecting);

/* append optional to required and sets optional to NULL */
void cleri_combine_expecting(cleri_expecting_t * expecting);

/* removes an gid from expecting if the gid exists in the list.
 * note: we only look in required since this is where the final
 * expects are stored.
 */
void cleri_remove_from_expecting(cleri_expecting_t * expecting, uint32_t gid);
