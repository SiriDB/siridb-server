/*
 * expr.h - For parsing expressions.
 */
#ifndef EXPR_H_
#define EXPR_H_

#define EXPR_DIVISION_BY_ZERO -1
#define EXPR_MODULO_BY_ZERO -2

/* values below are used by SiriDB and are not necessary for this library */
#define EXPR_TOO_LONG -3
#define EXPR_TIME_OUT_OF_RANGE -4
#define EXPR_INVALID_DATE_STRING -5
#define EXPR_MEM_ALLOC_ERR -6

/*
 * Max size for expressions
 * (not the total query, just an expression in the query
 */
#define EXPR_MAX_SIZE 512

#include <inttypes.h>

/* Returns 0 when result is successful set */
int expr_parse(int64_t * result, const char * expr);


#endif  /* EXPR_H_ */
