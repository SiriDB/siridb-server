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

struct cleri_olist_s;
struct cleri_object_s;

#define CLERI_DEFAULT_RE_KEYWORDS "^\\w+"

typedef struct cleri_grammar_s
{
    struct cleri_object_s * start;
    pcre * re_keywords;
    pcre_extra * re_kw_extra;
    struct cleri_olist_s * olist;
} cleri_grammar_t;


cleri_grammar_t * cleri_grammar(
        struct cleri_object_s * start,
        const char * re_keywords);

void cleri_free_grammar(cleri_grammar_t * grammar);
