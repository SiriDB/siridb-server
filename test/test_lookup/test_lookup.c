
#include "../test.h"
#include <siri/db/lookup.h>
#include <stdbool.h>


/* to at least 42 pools we devide series within 20% off from ideal */
#define NPOOLS 42

static double percentage_unequal = 0.2;
static unsigned int countersa[NPOOLS];
static unsigned int countersb[NPOOLS];

static void init_counters(void)
{
    unsigned int i;
    for (i = 0; i < NPOOLS; ++i)
    {
        countersa[i] = 0;
        countersb[i] = 0;
    }
}

int main()
{
    test_start("lookup");

    unsigned int i;
    siridb_lookup_t * lookup;
    uint16_t match[30] = {
        0, 1, 0, 2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0,
        2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0, 2, 3, 1
    };

    /* make sure the lookup zie has not changed */
    _assert(SIRIDB_LOOKUP_SZ == 8192);

    {
        unsigned int num_pools;
        for (num_pools = 1; num_pools < NPOOLS; ++num_pools)
        {
            unsigned int n, p, ideal, lower, upper;
            ideal = SIRIDB_LOOKUP_SZ / (num_pools * 2);
            lower = ideal - (percentage_unequal * ideal);
            upper = ideal + (percentage_unequal * ideal);
            init_counters();
            lookup = siridb_lookup_new(num_pools);
            _assert (lookup);
            for (n = 0; n < SIRIDB_LOOKUP_SZ; ++n)
            {
                /* check for SIRIDB_SERIES_IS_SERVER_ONE flag */
                if ((bool) ((n / 11) % 2))
                {
                    countersa[(*lookup)[n]]++;
                }
                else
                {
                    countersb[(*lookup)[n]]++;
                }
            }
            for (p = 0; p < num_pools; ++p)
            {
                _assert (countersa[p] >= lower && countersa[p] <= upper);
                _assert (countersb[p] >= lower && countersb[p] <= upper);
            }
            free(lookup);
        }
    }

    lookup = siridb_lookup_new(4);
    _assert (lookup);

    for (i = 0; i < 30; i++)
    {
        _assert( match[i] == (*lookup)[i] );
    }

    free(lookup);
    return test_end();
}
