/*
 * xmath.c - Extra math functions functions used by SiriDB.
 */
#include <xmath/xmath.h>
#include <stdarg.h>

/*
 * Got this from : (Elias Yarrkov)
 *  http://stackoverflow.com/questions/101439/the-most-efficient-way-to-
 *      implement-an-integer-based-power-function-powint-int
 */
uint32_t xmath_ipow(int base, int exp)
{
    uint32_t result = 1;
    while (exp)
    {
        if (exp & 1)
        {
            result *= base;
        }
        exp >>= 1;
        base *= base;
    }

    return result;
}

size_t xmath_max_size(size_t n, ...)
{
    size_t t, m = 0;
    va_list args;
    va_start(args, n);
    while (n--)
    {
        t = va_arg(args, size_t);
        m = (t > m) ? t : m;
    }
    va_end(args);
    return m;
}
