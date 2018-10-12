#include "../test.h"
#include <xstr/xstr.h>


int main()
{
    test_start("xstr");

    /* xstr_to_double */
    {
        _assert (xstr_to_double("0.5", 3) == 0.5);
        _assert (xstr_to_double("0.55", 3) == 0.5);
        _assert (xstr_to_double("123.456", 7) == 123.456);
        _assert (xstr_to_double("123", 3) == 123);
        _assert (xstr_to_double("123.", 4) == 123);
        _assert (xstr_to_double("123456.", 3) == 123);
        _assert (xstr_to_double("-0.5", 3) == -0.5);
    }

    return test_end();
}
