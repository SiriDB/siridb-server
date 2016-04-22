/*
 * expr.h - Parse expression
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-04-2016
 *
 */
#pragma once

#include <inttypes.h>

#define EXPR_DIVISION_BY_ZERO -1
#define EXPR_MODULO_BY_ZERO -2

/* values below are used by SiriDB and are not necessary for this library */
#define EXPR_TOO_LONG -3
#define EXPR_TIME_OUT_OF_RANGE -4
#define EXPR_MAX_SIZE 512


/* Returns 0 when result is successful set */
int expr_parse(int64_t * result, const char * expr);
