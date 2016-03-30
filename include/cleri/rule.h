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

struct cleri_object_s;
struct cleri_node_s;

typedef struct cleri_rule_tested_s
{
    const char * str;
    struct cleri_node_s * node;
    struct cleri_rule_tested_s * next;
} cleri_rule_tested_t;

typedef struct cleri_rule_store_s
{
    cleri_rule_tested_t * tested;
    struct cleri_object_s * root_obj;
} cleri_rule_store_t;

typedef struct cleri_rule_s
{
    uint32_t gid;
    struct cleri_object_s * cl_obj;
} cleri_rule_t;

struct cleri_object_s * cleri_rule(
        uint32_t gid,
        struct cleri_object_s * cl_obj);

int cleri_init_rule_tested(
        cleri_rule_tested_t ** target,
        cleri_rule_tested_t * tested,
        const char * str);
