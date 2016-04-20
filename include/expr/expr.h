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
#include <stddef.h>

int64_t expr_parser(const char * expr, size_t len);
