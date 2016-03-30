/*
 * object.h - each cleri element is a cleri object.
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

#include <cleri/parser.h>
#include <cleri/expecting.h>
#include <cleri/keyword.h>
#include <cleri/sequence.h>
#include <cleri/optional.h>
#include <cleri/choice.h>
#include <cleri/list.h>
#include <cleri/regex.h>
#include <cleri/repeat.h>
#include <cleri/token.h>
#include <cleri/tokens.h>
#include <cleri/grammar.h>
#include <cleri/prio.h>
#include <cleri/node.h>
#include <cleri/rule.h>
#include <cleri/this.h>

struct cleri_object_s;
struct cleri_grammar_s;
struct cleri_keyword_s;
struct cleri_sequence_s;
struct cleri_optional_s;
struct cleri_choice_s;
struct cleri_regex_s;
struct cleri_list_s;
struct cleri_repeat_s;
struct cleri_token_s;
struct cleri_tokens_s;
struct cleri_prio_s;
struct cleri_node_s;
struct cleri_rule_s;
struct cleri_rule_store_s;
struct cleri_parse_result_s;

typedef enum {
    CLERI_TP_SEQUENCE,
    CLERI_TP_OPTIONAL,
    CLERI_TP_CHOICE,
    CLERI_TP_LIST,
    CLERI_TP_REPEAT,
    CLERI_TP_PRIO,
    CLERI_TP_RULE,
    CLERI_TP_THIS,
    /* all items after this will not get children */
    CLERI_TP_KEYWORD,
    CLERI_TP_TOKEN,
    CLERI_TP_TOKENS,
    CLERI_TP_REGEX,
    CLERI_TP_END_OF_STATEMENT
} cleri_object_tp;

typedef struct cleri_dummy_s
{
    uint32_t gid;
} cleri_dummy_t;

typedef union
{
    struct cleri_keyword_s * keyword;
    struct cleri_sequence_s * sequence;
    struct cleri_optional_s * optional;
    struct cleri_choice_s * choice;
    struct cleri_regex_s * regex;
    struct cleri_list_s * list;
    struct cleri_repeat_s * repeat;
    struct cleri_token_s * token;
    struct cleri_tokens_s * tokens;
    struct cleri_prio_s * prio;
    struct cleri_rule_s * rule;
    struct cleri_dummy_s * dummy; /* place holder so we can easy get a gid */
} cleri_object_u;

typedef void (*cleri_free_object_t)(
        struct cleri_grammar_s *,
        struct cleri_object_s *);

typedef struct cleri_node_s * (*cleri_parse_object_t)(
        struct cleri_parse_result_s *,
        struct cleri_node_s *,
        struct cleri_object_s *,
        struct cleri_rule_store_s *);

typedef struct cleri_object_s
{
    cleri_object_tp tp;
    cleri_free_object_t free_object;
    cleri_parse_object_t parse_object;
    cleri_object_u * cl_obj;
} cleri_object_t;

cleri_object_t * cleri_new_object(
        int tp,
        cleri_free_object_t free_object,
        cleri_parse_object_t parse_object);

void cleri_free_object(
        struct cleri_grammar_s * grammar,
        cleri_object_t * cl_object);

cleri_object_t * CLERI_END_OF_STATEMENT;


