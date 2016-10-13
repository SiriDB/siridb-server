/*
 * version.c - contains SiriDB version info.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <assert.h>
#include <siri/version.h>
#include <stdio.h>
#include <stdlib.h>

int siri_version_cmp(const char * version_a, const char * version_b)
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
    assert(0);
    return 0;
}
