/*
 * lookup.h - Find and assign to which pool series belong.
 */
#ifndef SIRIDB_LOOKUP_H_
#define SIRIDB_LOOKUP_H_

#include <inttypes.h>
#include <stddef.h>

#define SIRIDB_LOOKUP_SZ 8192

typedef uint_fast16_t siridb_lookup_t[SIRIDB_LOOKUP_SZ];

uint16_t siridb_lookup_sn(siridb_lookup_t * lookup, const char * sn);
uint16_t siridb_lookup_sn_raw(
        siridb_lookup_t * lookup,
        const char * sn,
        size_t len);
siridb_lookup_t * siridb_lookup_new(uint_fast16_t num_pools);
void siridb_lookup_free(siridb_lookup_t * lookup);

#endif  /* SIRIDB_LOOKUP_H_ */
