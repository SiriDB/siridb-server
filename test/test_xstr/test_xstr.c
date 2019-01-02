#include "../test.h"
#include <xstr/xstr.h>


int main()
{
    test_start("xstr");

    /* xstr_to_double */
    {
        _assert (xstr_to_double("0.5") == 0.5);
        _assert (xstr_to_double("0.55") == 0.55);
        _assert (xstr_to_double("123.456") == 123.456);
        _assert (xstr_to_double("123") == 123.0);
        _assert (xstr_to_double("123.") == 123.0);
        _assert (xstr_to_double("123.") == 123.0);
        _assert (xstr_to_double("+1234") == 1234.0);
        _assert (xstr_to_double("-1234") == -1234.0);
        _assert (xstr_to_double("123456.") == 123456.0);
        _assert (xstr_to_double("-0.5") == -0.5);
        _assert (xstr_to_double("-0.56") == -0.56);
        _assert (xstr_to_double("+0.5") == 0.5);
        _assert (xstr_to_double("-.5") == -0.5);
        _assert (xstr_to_double("+.55") == 0.55);
        _assert (xstr_to_double(".55") == 0.55);
        _assert (xstr_to_double("-.55") == -0.55);
    }

    return test_end();
}
