/*
 * cexpr.h - Conditional expressions.
 */
#ifndef CEXPR_H_
#define CEXPR_H_

#define CEXPR_MAX_CURLY_DEPTH 6

typedef enum
{
    CEXPR_EQ,   /* equal                        */
    CEXPR_NE,   /* not equal                    */
    CEXPR_GT,   /* greater than                 */
    CEXPR_LT,   /* less than                    */
    CEXPR_GE,   /* greater than or equal to     */
    CEXPR_LE,   /* less than or equal to        */
    CEXPR_IN,   /* contains (string)            */
    CEXPR_NI,   /* not contains (string)        */
    CEXPR_AND,
    CEXPR_OR,
} cexpr_operator_t;

typedef union cexpr_via_u cexpr_via_t;
typedef struct cexpr_condition_s cexpr_condition_t;
typedef struct cexpr_s cexpr_t;
typedef struct cexpr_list_s cexpr_list_t;

#include <inttypes.h>
#include <qpack/qpack.h>
#include <cleri/cleri.h>

typedef int (*cexpr_cb_t)(void * obj, cexpr_condition_t * cond);
typedef int (*cexpr_cb_prop_t)(uint32_t prop);

cexpr_t * cexpr_from_node(cleri_node_t * node);
int cexpr_int_cmp(
        const cexpr_operator_t operator,
        const int64_t a,
        const int64_t b);
int cexpr_double_cmp(
        const cexpr_operator_t operator,
        const double a,
        const double b);
int cexpr_str_cmp(
        const cexpr_operator_t operator,
        const char * a,
        const char * b);
int cexpr_bool_cmp(
        const cexpr_operator_t operator,
        const int64_t a,
        const int64_t b);
int cexpr_run(cexpr_t * cexpr, cexpr_cb_t cb, void * obj);
int cexpr_contains(cexpr_t * cexpr, cexpr_cb_prop_t cb);
void cexpr_free(cexpr_t * cexpr);
cexpr_operator_t cexpr_operator_fn(cleri_node_t * node);

union cexpr_via_u
{
    cexpr_condition_t * cond;
    cexpr_t * cexpr;
};

struct cexpr_condition_s
{
    uint32_t prop;
    cexpr_operator_t operator;
    int64_t int64;
    char * str;
};

struct cexpr_list_s
{
    size_t len;
    cexpr_t * cexpr[CEXPR_MAX_CURLY_DEPTH];
};

struct cexpr_s
{
    cexpr_operator_t operator; /* AND/OR        */
    int8_t tp_a;
    int8_t tp_b;
    cexpr_via_t via_a;
    cexpr_via_t via_b;
};

#endif  /* CEXPR_H_ */
