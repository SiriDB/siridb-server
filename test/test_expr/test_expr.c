#include "../test.h"
#include <expr/expr.h>


int main()
{
    test_start("expr");

    int64_t result;

    _assert(expr_parse(&result, "5+37") == 0 && result == 42);
    _assert(expr_parse(&result, "2+2*20") == 0 && result == 42);
    _assert(expr_parse(&result, "(2+4)*7") == 0 && result == 42);
    _assert(expr_parse(&result, "16%10*7") == 0 && result == 42);
    _assert(expr_parse(&result, "7*14%56") == 0 && result == 42);
    _assert(expr_parse(&result, "21/3*6") == 0 && result == 42);
    _assert(expr_parse(&result, "22/3*6") == 0 && result == 42);

    /* division by zero is not a good idea */
    _assert(expr_parse(&result, "42/(2-2)") == EXPR_DIVISION_BY_ZERO);

    /* module by zero is not a good idea either */
    _assert(expr_parse(&result, "42%(2-2)") == EXPR_MODULO_BY_ZERO);

    return test_end();
}
