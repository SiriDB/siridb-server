/*
 * condition.c - Condition helpers
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 12-06-2016
 *
 */
#include <siri/db/condition.h>
#include <assert.h>

int siridb_condition_int_cmp(operator_t operator, int64_t a, int64_t b)
{
    switch (operator)
    {
    case SIRIDB_CONDITION_EQ:
        return a == b;
    case SIRIDB_CONDITION_NE:
        return a != b;
    case SIRIDB_CONDITION_GT:
        return a > b;
    case SIRIDB_CONDITION_LT:
        return a < b;
    case SIRIDB_CONDITION_GE:
        return a >= b;
    case SIRIDB_CONDITION_LE:
        return a <= b;
    }
    assert (0); /* we should never get here */
    return -1;
}
