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
#include <cleri/parse.h>
#include <cleri/rule.h>
#include <cleri/this.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_grammar_s cleri_grammar_t;
typedef struct cleri_keyword_s cleri_keyword_t;
typedef struct cleri_sequence_s cleri_sequence_t;
typedef struct cleri_optional_s cleri_optional_t;
typedef struct cleri_choice_s cleri_choice_t;
typedef struct cleri_regex_s cleri_regex_t;
typedef struct cleri_list_s cleri_list_t;
typedef struct cleri_repeat_s cleri_repeat_t;
typedef struct cleri_token_s cleri_token_t;
typedef struct cleri_tokens_s cleri_tokens_t;
typedef struct cleri_prio_s cleri_prio_t;
typedef struct cleri_rule_s cleri_rule_t;
typedef struct cleri_rule_store_s cleri_rule_store_t;
typedef struct cleri_node_s cleri_node_t;
typedef struct cleri_parse_s cleri_parse_t;

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
    cleri_keyword_t * keyword;
    cleri_sequence_t * sequence;
    cleri_optional_t * optional;
    cleri_choice_t * choice;
    cleri_regex_t * regex;
    cleri_list_t * list;
    cleri_repeat_t * repeat;
    cleri_token_t * token;
    cleri_tokens_t * tokens;
    cleri_prio_t * prio;
    cleri_rule_t * rule;
    cleri_dummy_t * dummy; /* place holder so we can easy get a gid */
} cleri_object_u;

typedef void (*cleri_free_object_t)(cleri_object_t *);

typedef cleri_node_t * (*cleri_parse_object_t)(
        cleri_parse_t *,
        cleri_node_t *,
        cleri_object_t *,
        cleri_rule_store_t *);

typedef struct cleri_object_s
{
    cleri_object_tp tp;
    uint32_t ref;
    cleri_free_object_t free_object;
    cleri_parse_object_t parse_object;
    cleri_object_u via;
} cleri_object_t;

cleri_object_t * cleri_object_new(
        cleri_object_tp tp,
        cleri_free_object_t free_object,
        cleri_parse_object_t parse_object);

void cleri_object_incref(cleri_object_t * cl_object);
void cleri_object_decref(cleri_object_t * cl_object);

cleri_object_t * CLERI_END_OF_STATEMENT;


