/*
 * olist.h - linked list for keeping cleri objects.
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

#include <cleri/object.h>
#include <cleri/grammar.h>

struct cleri_object_s;
struct cleri_grammar_s;

typedef struct cleri_olist_s
{
    struct cleri_object_s * cl_obj;
    struct cleri_olist_s * next;
} cleri_olist_t;

cleri_olist_t * cleri_new_olist(void);

void cleri_olist_add(
        cleri_olist_t * olist,
        struct cleri_object_s * cl_object);

void cleri_free_olist(
        struct cleri_grammar_s * grammar,
        cleri_olist_t * olist);

void cleri_empty_olist(cleri_olist_t * olist);

int cleri_olist_has(
        cleri_olist_t * olist,
        struct cleri_object_s * cl_object);
