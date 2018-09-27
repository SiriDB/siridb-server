
#include "../test.h"
#include <siri/db/lookup.h>


int main()
{
    test_start("lookup");

    unsigned int i;
    siridb_lookup_t * lookup;
    uint16_t match[30] = {
            0, 1, 0, 2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0,
            2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0, 2, 3, 1};

    lookup = siridb_lookup_new(4);
    _assert (lookup);

    for (i = 0; i < 30; i++)
    {
        _assert( match[i] == (*lookup)[i] );
    }

    free(lookup);
    return test_end();
}
