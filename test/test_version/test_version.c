#include <stdio.h>
#include <stdlib.h>
#include "../test.h"
#include <siri/version.h>



int old_version_cmp(const char * version_a, const char * version_b)
{
    long int a, b;

    char * str_a = (char *) version_a;
    char * str_b = (char *) version_b;

    while (1)
    {
        a = strtol(str_a, &str_a, 10);
        b = strtol(str_b, &str_b, 10);

        if (a != b)
        {
            return a - b;
        }
        else if (!*str_a && !*str_b)
        {
            return 0;
        }
        else if (!*str_a)
        {
            return -1;
        }
        else if (!*str_b)
        {
            return 1;
        }
        str_a++;
        str_b++;
    }

    /* we should NEVER get here */
    _assert (0);
    return 0;
}


int main()
{
    test_start("version");

    /* alpha verioning should be ignored by version compare */
    _assert (siri_version_cmp("1.0.0", "2.0.0") < 0);
    _assert (siri_version_cmp("1.0.0", "2.0.0-alpha-0") < 0);
    _assert (siri_version_cmp("2.0.0", "1.0.0") > 0);
    _assert (siri_version_cmp("2.0.0", "1.0.0") > 0);
    _assert (siri_version_cmp("2.2.0", "2.32.0") < 0);
    _assert (siri_version_cmp("2.32.0", "2.2.0") > 0);
    _assert (siri_version_cmp("2.0.5", "2.0.22") < 0);
    _assert (siri_version_cmp("2.0.22", "2.0.5") > 0);
    _assert (siri_version_cmp("2.0.5-alpha-0", "2.0.22") < 0);
    _assert (siri_version_cmp("2.0.22-alpha-0", "2.0.5") > 0);
    _assert (siri_version_cmp("a", "") == 0);
    _assert (siri_version_cmp("", "b") == 0);
    _assert (siri_version_cmp("", "") == 0);
    _assert (siri_version_cmp("2.0.30", "2.0.30") == 0);
    _assert (siri_version_cmp("2.0.30-alpha-1-debug", "2.0.30") == 0);
    _assert (siri_version_cmp("2.0.30-alpha-1", "2.0.30-alpha-0") == 0);

    /* old version compare function should not break with -alpha versions */
    _assert (old_version_cmp("2.0.5-alpha-0", "2.0.22") < 0);
    _assert (old_version_cmp("2.0.22-alpha-0", "2.0.5") > 0);
    _assert (old_version_cmp("2.0.30-alpha-1-debug", "2.0.30") > 0);
    /* This last one is < 0 since -1 < -0 */
    _assert (old_version_cmp("2.0.30-alpha-1", "2.0.30-alpha-0") < 0);

    return test_end();
}
