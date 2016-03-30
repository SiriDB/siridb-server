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

struct cleri_object_s;
struct cleri_grammar_s;
struct cleri_node_s;
struct cleri_expecting_s;
struct cleri_kwcache_s;
struct cleri_rule_store_s;

typedef struct cleri_parse_result_s
{
    bool is_valid;
    size_t pos;
    const char * str;
    struct cleri_node_s * tree;
    struct cleri_expecting_s * expecting;
    pcre * re_keywords;
    pcre_extra * re_kw_extra;
    struct cleri_kwcache_s * kwcache;
} cleri_parse_result_t;

cleri_parse_result_t * cleri_parse(
        struct cleri_grammar_s * grammar,
        const char * str);

void cleri_free_parse_result(cleri_parse_result_t * pr);

struct cleri_node_s * cleri_walk(
        cleri_parse_result_t * pr,
        struct cleri_node_s * parent,
        struct cleri_object_s * cl_obj,
        struct cleri_rule_store_s * rule,
        int mode);
