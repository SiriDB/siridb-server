/*
 * xmath.h - Extra math functions which are useful.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 12-03-2016
 *
 */
#ifndef XMATH_H_
#define XMATH_H_

#include <inttypes.h>
#include <stddef.h>

uint32_t xmath_ipow(int base, int exp);
size_t xmath_max_size(size_t n, ...);

#endif  /* XMATH_H_ */
