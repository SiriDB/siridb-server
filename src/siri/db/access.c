/*
 * access.c - Access constants and functions.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-03-2016
 *
 */
#include <siri/db/access.h>
#include <string.h>
#include <stdio.h>

#define ACCESS_SIZE 16

static const siridb_access_repr_t access_map[ACCESS_SIZE] = {

        /* access profiles, biggest numbers must be fist */
        {.repr="full", .access_bit=SIRIDB_ACCESS_PROFILE_FULL},
        {.repr="modify", .access_bit=SIRIDB_ACCESS_PROFILE_MODIFY},
        {.repr="write", .access_bit=SIRIDB_ACCESS_PROFILE_WRITE},
        {.repr="read", .access_bit=SIRIDB_ACCESS_PROFILE_READ},

        /* access bits, order here is not important */
        {.repr="select", .access_bit=SIRIDB_ACCESS_SELECT},
        {.repr="show", .access_bit=SIRIDB_ACCESS_SHOW},
        {.repr="list", .access_bit=SIRIDB_ACCESS_LIST},
        {.repr="create", .access_bit=SIRIDB_ACCESS_CREATE},
        {.repr="insert", .access_bit=SIRIDB_ACCESS_INSERT},
        {.repr="drop", .access_bit=SIRIDB_ACCESS_DROP},
        {.repr="count", .access_bit=SIRIDB_ACCESS_COUNT},
        {.repr="grant", .access_bit=SIRIDB_ACCESS_GRANT},
        {.repr="revoke", .access_bit=SIRIDB_ACCESS_REVOKE},
        {.repr="alter", .access_bit=SIRIDB_ACCESS_ALTER},
        {.repr="pause", .access_bit=SIRIDB_ACCESS_PAUSE},
        {.repr="continue", .access_bit=SIRIDB_ACCESS_CONTINUE},
};

/*
 * Returns access bit by string.
 */
uint32_t siridb_access_from_strn(const char * str, size_t n)
{
    for (int i = 0; i < ACCESS_SIZE; i++)
    {
        if (strncmp(access_map[i].repr, str, n) == 0)
        {
            return access_map[i].access_bit;
        }
    }
    return 0;
}

/*
 * Returns a access bit flag from a Cleri children object.
 */
uint32_t siridb_access_from_children(cleri_children_t * children)
{
    uint32_t access_bit = 0;

    while (children != NULL)
    {
        access_bit |= siridb_access_from_strn(
                children->node->str,
                children->node->len);
        if (children->next == NULL)
            break;
        children = children->next->next;
    }

    return access_bit;
}

/*
 * Make sure 'str' is a pointer to a string which can hold at least
 * SIRIDB_ACCESS_STR_MAX.
 */
void siridb_access_to_str(char * str, uint32_t access_bit)
{
    char * pt = str;

    for (int i = 0; i < ACCESS_SIZE && access_bit; i++)
    {
        if ((access_bit & access_map[i].access_bit) == access_map[i].access_bit)
        {
            access_bit -= access_map[i].access_bit;
            pt += (pt == str) ? sprintf(pt, "%s", access_map[i].repr) :
                    (access_bit) ? sprintf(pt, ", %s", access_map[i].repr) :
                            sprintf(pt, " and %s", access_map[i].repr);
        }
    }
    if (pt == str)
    {
        sprintf(pt, "no access");
    }
}
