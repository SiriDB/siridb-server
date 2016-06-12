/*
 * condition.h - Condition helpers
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 12-06-2016
 *
 */
#pragma once
#include <inttypes.h>
#include <qpack/qpack.h>

typedef enum operator
{
    SIRIDB_CONDITION_EQ,   // equal
    SIRIDB_CONDITION_NE,   // not equal
    SIRIDB_CONDITION_GT,   // greater than
    SIRIDB_CONDITION_LT,   // less than
    SIRIDB_CONDITION_GE,   // greater than or equal to
    SIRIDB_CONDITION_LE   // less than or equal to
} operator_t;

typedef struct siridb_condition_s
{
    operator_t operator;
    uint32_t prop;
    qp_via_t val;

} siridb_condition_t;

int siridb_condition_int_cmp(operator_t operator, int64_t a, int64_t b);
