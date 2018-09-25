/*
 * test_smap.c
 *
 *  Created on: Sep 30, 2017
 *      Author: Jeroen van der Heijden <jeroen@transceptor.technology>
 */


#include "../test.h"
#include <imap/imap.h>


int main()
{
    test_start("imap");

    imap_t * imap = imap_new();

    /* test is the length is correct */
    {
        _assert (imap->len == 0);
    }

    imap_free(imap, NULL);

    return test_end();
}
