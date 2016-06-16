/*
 * cexpr.h - Conditional expressions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-06-2016
 *
 */
#pragma once
#include <inttypes.h>
#include <qpack/qpack.h>
#include <cleri/node.h>

#define CEXPR_MAX_CURLY_DEPT 10

typedef enum cexpr_operator
{
    CEXPR_EQ,   // equal
    CEXPR_NE,   // not equal
    CEXPR_GT,   // greater than
    CEXPR_LT,   // less than
    CEXPR_GE,   // greater than or equal to
    CEXPR_LE,   // less than or equal to
    CEXPR_IN,   // contains (string)
    CEXPR_NI,   // not contains (string)
    CEXPR_AND,
    CEXPR_OR,
} cexpr_operator_t;

typedef struct cexpr_condition_s
{
    uint32_t prop;
    cexpr_operator_t operator;
    int64_t int64;
    char * str;
} cexpr_condition_t;

typedef int (*cexpr_cb_t)(void * obj, cexpr_condition_t * cond);

typedef struct cexpr_s cexpr_t;

typedef struct cexpr_list_s
{
    size_t len;
    cexpr_t * cexpr[CEXPR_MAX_CURLY_DEPT];
} cexpr_list_t;

typedef union cexpr_via_u
{
    cexpr_condition_t * cond;
    cexpr_t * cexpr;
} cexpr_via_t;

typedef struct cexpr_s
{
    cexpr_operator_t operator; // AND/OR
    int8_t tp_a;
    int8_t tp_b;
    cexpr_via_t via_a;
    cexpr_via_t via_b;
} cexpr_t;

cexpr_t * cexpr_from_node(cleri_node_t * node);
int cexpr_int_cmp(
        const cexpr_operator_t operator,
        const int64_t a,
        const int64_t b);
int cexpr_str_cmp(
        const cexpr_operator_t operator,
        const char * a,
        const char * b);
int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj);
void cexpr_free(cexpr_t * cexpr);
