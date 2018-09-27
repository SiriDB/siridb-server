#include "../test.h"
#include <xpath/xpath.h>


int main()
{
    test_start("xpath");

    /* xpath_file_exist */
    {
        _assert (xpath_file_exist("./test.h"));
        _assert (!xpath_file_exist("./test.foo"));
    }

    return test_end();
}
