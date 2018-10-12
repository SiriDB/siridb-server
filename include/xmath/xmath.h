/*
 * xmath.h - Extra math functions functions used by SiriDB.
 */
#ifndef XMATH_H_
#define XMATH_H_

#include <inttypes.h>
#include <stddef.h>

uint32_t xmath_ipow(int base, int exp);
size_t xmath_max_size(size_t n, ...);

#endif  /* XMATH_H_ */
