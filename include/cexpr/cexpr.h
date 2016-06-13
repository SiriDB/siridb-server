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

typedef enum cexpr_operator
{
    CEXPR_EQ,   // equal
    CEXPR_NE,   // not equal
    CEXPR_GT,   // greater than
    CEXPR_LT,   // less than
    CEXPR_GE,   // greater than or equal to
    CEXPR_LE,   // less than or equal to
    CEXPR_AND,
    CEXPR_OR,
} cexpr_operator_t;

typedef struct cexpr_condition_s
{
    uint32_t prop;
    cexpr_operator_t operator;
    qp_via_t val;
} cexpr_condition_t;

typedef int (*cexpr_cb_t)(void * obj, cexpr_condition_t * cond);

typedef struct cexpr_s cexpr_t;

typedef union cexpr_via_u
{
    cexpr_condition_t * cond;
    cexpr_t * expr;
} cexpr_via_t;

typedef struct cexpr_s
{
    cexpr_operator_t operator; // AND/OR
    int8_t tp_a;
    int8_t tp_b;
    cexpr_via_t via_a;
    cexpr_via_t via_b;
} cexpr_t;

int cexpr_icmp(cexpr_operator_t operator, int64_t a, int64_t b);
int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj);
