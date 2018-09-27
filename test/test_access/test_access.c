#include "../test.h"
#include <siri/db/access.h>


int main()
{
    test_start("access");

    char buffer[SIRIDB_ACCESS_STR_MAX];
    uint32_t access_bit = 0;

    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "no access") == 0);

    access_bit |= SIRIDB_ACCESS_SHOW;
    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "show") == 0);

    access_bit |= SIRIDB_ACCESS_SELECT;
    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "select and show") == 0);

    access_bit |= SIRIDB_ACCESS_LIST;
    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "list, select and show") == 0);

    access_bit |= SIRIDB_ACCESS_PROFILE_WRITE;
    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "write") == 0);

    access_bit &= ~SIRIDB_ACCESS_INSERT;
    siridb_access_to_str(buffer, access_bit);
    _assert (strcmp(buffer, "read and create") == 0);

    _assert (siridb_access_from_strn("read", 4) == (SIRIDB_ACCESS_PROFILE_READ));
    _assert (siridb_access_from_strn("list", 4) == SIRIDB_ACCESS_LIST);

    return test_end();
}