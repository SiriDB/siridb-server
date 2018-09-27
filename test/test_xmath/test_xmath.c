#include "../test.h"
#include <xmath/xmath.h>


int main()
{
    test_start("xmath");

    /* xmath_ipow */
    {
        _assert (xmath_ipow(1000, 0) == 1);
        _assert (xmath_ipow(1000, 1) == 1000);
        _assert (xmath_ipow(1000, 2) == 1000000);
        _assert (xmath_ipow(1000, 3) == 1000000000);
        _assert (xmath_ipow(2, 8) == 256);
    }

    /* xmath_max_size */
    {
        _assert (xmath_max_size(3, 10, 20, 30) == 30);
        _assert (xmath_max_size(3, 30, 20, 10) == 30);
        _assert (xmath_max_size(3, 10, 30, 20) == 30);
    }

    return test_end();
}
