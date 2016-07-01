/*
 * grammar.h - this should contain the 'start' or your grammar.
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

#include <pcre.h>
#include <cleri/object.h>
#include <cleri/olist.h>

typedef struct cleri_object_s cleri_object_t;

#define CLERI_DEFAULT_RE_KEYWORDS "^\\w+"

typedef struct cleri_grammar_s
{
    cleri_object_t * start;
    pcre * re_keywords;
    pcre_extra * re_kw_extra;
} cleri_grammar_t;


cleri_grammar_t * cleri_grammar(
        cleri_object_t * start,
        const char * re_keywords);

void cleri_grammar_free(cleri_grammar_t * grammar);
