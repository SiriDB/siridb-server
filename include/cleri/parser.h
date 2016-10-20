/*
 * parser.h - this contains the start for parsing a string to a grammar.
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

#include <stddef.h>
#include <stdbool.h>
#include <cleri/object.h>
#include <cleri/grammar.h>
#include <cleri/node.h>
#include <cleri/expecting.h>
#include <cleri/kwcache.h>
#include <cleri/rule.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_grammar_s cleri_grammar_t;
typedef struct cleri_node_s cleri_node_t;
typedef struct cleri_expecting_s cleri_expecting_t;
typedef struct cleri_kwcache_s cleri_kwcache_t;
typedef struct cleri_rule_store_s cleri_rule_store_t;

typedef struct cleri_parser_s
{
    bool is_valid;
    size_t pos;
    const char * str;
    cleri_node_t * tree;
    cleri_expecting_t * expecting;
    pcre * re_keywords;
    pcre_extra * re_kw_extra;
    cleri_kwcache_t * kwcache;
} cleri_parser_t;

cleri_parser_t * cleri_parser_new(
        cleri_grammar_t * grammar,
        const char * str);

void cleri_parser_free(cleri_parser_t * pr);

cleri_node_t * cleri__parser_walk(
        cleri_parser_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule,
        int mode);

extern int cleri_err;
