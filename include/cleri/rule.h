/*
 * rule.h - cleri regular rule element. (do not directly use this element but
 *          create a 'prio' instead which will be wrapped by a rule element)
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
#include <inttypes.h>
#include <cleri/object.h>
#include <cleri/node.h>
#include <cleri/expecting.h>

typedef struct cleri_object_s cleri_object_t;
typedef struct cleri_node_s cleri_node_t;

typedef enum cleri_rule_test_e
{
    CLERI_RULE_ERROR=-1,
    CLERI_RULE_FALSE,
    CLERI_RULE_TRUE
} cleri_rule_test_t;

typedef struct cleri_rule_tested_s
{
    const char * str;
    cleri_node_t * node;
    struct cleri_rule_tested_s * next;
} cleri_rule_tested_t;

typedef struct cleri_rule_store_s
{
    cleri_rule_tested_t * tested;
    cleri_object_t * root_obj;
    size_t depth;
} cleri_rule_store_t;

typedef struct cleri_rule_s
{
    uint32_t gid;
    cleri_object_t * cl_obj;
} cleri_rule_t;

cleri_object_t * cleri_rule(uint32_t gid, cleri_object_t * cl_obj);

cleri_rule_test_t cleri_rule_init(
        cleri_rule_tested_t ** target,
        cleri_rule_tested_t * tested,
        const char * str);
