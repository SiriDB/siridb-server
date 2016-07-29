/*
 * lookup.c - SiriDB Pool lookup.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 29-07-2016
 *
 */
#include <siri/db/lookup.h>
#include <siri/err.h>
#include <stdlib.h>

static void LOOKUP_make(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup);

/*
 * Returns a pool id based on a terminated string.
 */
uint16_t siridb_lookup_sn(siridb_lookup_t * lookup, const char * sn)
{
    uint32_t n = 0;
    for (; *sn; sn++)
    {
        n += *sn;
    }
    return (*lookup)[n % SIRIDB_LOOKUP_SZ];
}

/*
 * Returns a pool id based on a raw string.
 */
uint16_t siridb_lookup_sn_raw(
        siridb_lookup_t * lookup,
        const char * sn,
        size_t len)
{
    uint32_t n = 0;
    while (len--)
    {
        n += sn[len];
    }
    return (*lookup)[n % SIRIDB_LOOKUP_SZ];
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_lookup_t * siridb_lookup_new(uint_fast16_t num_pools)
{
    siridb_lookup_t * lookup =
            (siridb_lookup_t *) calloc(1, sizeof(siridb_lookup_t));
    if (lookup == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        LOOKUP_make(1, num_pools, lookup);
    }
    return lookup;
}

/*
 * Destroy lookup.
 */
inline void siridb_lookup_free(siridb_lookup_t * lookup)
{
    free(lookup);
}

/*
 * Algorithm to create pools lookup array.
 */
static void LOOKUP_make(
        uint_fast16_t n,
        uint_fast16_t num_pools,
        siridb_lookup_t * lookup)
{
    if (n == num_pools)
    {
        return;
    }

    uint_fast16_t i;
    uint_fast16_t m;
    uint_fast16_t counters[n];
    for (i = 0; i < n; i++)
    {
        counters[i] = i;
    }

    m = n + 1;
    for (i = 0; i < SIRIDB_LOOKUP_SZ; i++)
    {
        counters[(*lookup)[i]]++;
        if (counters[(*lookup)[i]] % m == 0)
        {
            (*lookup)[i] = n;
        }
    }
    LOOKUP_make(m, num_pools, lookup);
}
