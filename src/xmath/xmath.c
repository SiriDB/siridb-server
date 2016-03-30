/*
 * xmath.c - Extra math functions which are useful.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 12-03-2016
 *
 */
#include <xmath/xmath.h>

/*
 * Got this from : (Elias Yarrkov)
 *  http://stackoverflow.com/questions/101439/the-most-efficient-way-to-
 *      implement-an-integer-based-power-function-powint-int
 */
int ipow(int base, int exp)
{
    int result = 1;
    while (exp)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        base *= base;
    }

    return result;
}

