/*
 * cexpr.c - Conditional expressions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-06-2016
 *
 */
#include <cexpr/cexpr.h>
#include <assert.h>

#define VIA_EXPR 0
#define VIA_COND 1

int cexpr_icmp(cexpr_operator_t operator, int64_t a, int64_t b)
{
    switch (operator)
    {
    case CEXPR_EQ:
        return a == b;
    case CEXPR_NE:
        return a != b;
    case CEXPR_GT:
        return a > b;
    case CEXPR_LT:
        return a < b;
    case CEXPR_GE:
        return a >= b;
    case CEXPR_LE:
        return a <= b;
    default:
        assert (0); /* we should never get here */
    }
    return -1;
}

int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj)
{
    switch (cexpr->operator)
    {
    case CEXPR_AND:
        return  ((cexpr->tp_a == VIA_EXPR) ?
                    cexpr_run(cexpr->via_a.expr, cb, obj) :
                    cb(obj, cexpr->via_a.cond)) &&
                ((cexpr->tp_b == VIA_EXPR) ?
                    cexpr_run(cexpr->via_b.expr, cb, obj) :
                    cb(obj, cexpr->via_b.cond));
    case CEXPR_OR:
        return  ((cexpr->tp_a == VIA_EXPR) ?
                    cexpr_run(cexpr->via_a.expr, cb, obj) :
                    cb(obj, cexpr->via_a.cond)) ||
                ((cexpr->tp_b == VIA_EXPR) ?
                    cexpr_run(cexpr->via_b.expr, cb, obj) :
                    cb(obj, cexpr->via_b.cond));
    default:
        assert (0);
    }
    return -1;
}
