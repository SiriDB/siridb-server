/*
 * access.h - Access constants and functions.
 */
#ifndef SIRIDB_ACCESS_H_
#define SIRIDB_ACCESS_H_

/* definitions of all access rights */
#define SIRIDB_ACCESS_SHOW      1
#define SIRIDB_ACCESS_COUNT     2
#define SIRIDB_ACCESS_LIST      4
#define SIRIDB_ACCESS_SELECT    8
#define SIRIDB_ACCESS_INSERT    16
#define SIRIDB_ACCESS_CREATE    32
#define SIRIDB_ACCESS_ALTER     64
#define SIRIDB_ACCESS_DROP      128
#define SIRIDB_ACCESS_GRANT     256
#define SIRIDB_ACCESS_REVOKE    512

/* this is a save size since access string cannot contain double items */
#define SIRIDB_ACCESS_STR_MAX 128

/* profile definitions */
#define SIRIDB_ACCESS_PROFILE_READ      \
    SIRIDB_ACCESS_SHOW |                \
    SIRIDB_ACCESS_COUNT |               \
    SIRIDB_ACCESS_LIST |                \
    SIRIDB_ACCESS_SELECT

#define SIRIDB_ACCESS_PROFILE_WRITE     \
    SIRIDB_ACCESS_PROFILE_READ |        \
    SIRIDB_ACCESS_INSERT |              \
    SIRIDB_ACCESS_CREATE

#define SIRIDB_ACCESS_PROFILE_MODIFY    \
    SIRIDB_ACCESS_PROFILE_WRITE |       \
    SIRIDB_ACCESS_ALTER |               \
    SIRIDB_ACCESS_DROP

#define SIRIDB_ACCESS_PROFILE_FULL      \
    SIRIDB_ACCESS_PROFILE_MODIFY |      \
    SIRIDB_ACCESS_GRANT |               \
    SIRIDB_ACCESS_REVOKE

typedef struct siridb_access_repr_s siridb_access_repr_t;

#include <cleri/cleri.h>

uint32_t siridb_access_from_strn(const char * str, size_t n);

/* children must be children from a cleri_list where values are comma
 * separated. The children node must be valid and contain at least one access
 * string.
 */
uint32_t siridb_access_from_children(cleri_children_t * children);

void siridb_access_to_str(char * str, uint32_t access_bit);

struct siridb_access_repr_s
{
    const char * repr;
    uint32_t access_bit;
};

#endif  /* SIRIDB_ACCESS_H_ */
