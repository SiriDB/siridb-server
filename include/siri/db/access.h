/*
 * access.h - Access constants and functions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-03-2016
 *
 */
#pragma once

#include <cleri/node.h>


#define SIRIDB_ACCESS_SELECT    1
#define SIRIDB_ACCESS_SHOW      2
#define SIRIDB_ACCESS_LIST      4
#define SIRIDB_ACCESS_CREATE    8
#define SIRIDB_ACCESS_INSERT    16
#define SIRIDB_ACCESS_DROP      32
#define SIRIDB_ACCESS_COUNT     64
#define SIRIDB_ACCESS_GRANT     128
#define SIRIDB_ACCESS_REVOKE    256
#define SIRIDB_ACCESS_ALTER     512
#define SIRIDB_ACCESS_PAUSE     1024
#define SIRIDB_ACCESS_CONTINUE  2048

/* this is a save size since access string cannot contain double items */
#define SIRIDB_ACCESS_STR_MAX 128

#define SIRIDB_ACCESS_PROFILE_READ      \
    SIRIDB_ACCESS_SELECT |              \
    SIRIDB_ACCESS_SHOW |                \
    SIRIDB_ACCESS_LIST |                \
    SIRIDB_ACCESS_COUNT

#define SIRIDB_ACCESS_PROFILE_WRITE     \
    SIRIDB_ACCESS_PROFILE_READ |        \
    SIRIDB_ACCESS_INSERT |              \
    SIRIDB_ACCESS_CREATE

#define SIRIDB_ACCESS_PROFILE_MODIFY    \
    SIRIDB_ACCESS_PROFILE_WRITE |       \
    SIRIDB_ACCESS_DROP |                \
    SIRIDB_ACCESS_ALTER

#define SIRIDB_ACCESS_PROFILE_FULL      \
    SIRIDB_ACCESS_PROFILE_MODIFY |      \
    SIRIDB_ACCESS_GRANT |               \
    SIRIDB_ACCESS_REVOKE |              \
    SIRIDB_ACCESS_PAUSE |               \
    SIRIDB_ACCESS_CONTINUE

typedef uint32_t siridb_access_t;

typedef struct siridb_access_repr_s
{
    const char * repr;
    siridb_access_t access_bit;
} siridb_access_repr_t;


siridb_access_t siridb_access_from_strn(const char * str, size_t n);

/* children must be children from a cleri_list where values are comma
 * separated. The children node must be valid and contain at least one access
 * string.
 */
siridb_access_t siridb_access_from_children(cleri_children_t * children);

void siridb_access_to_str(char * str, siridb_access_t access_bit);

