#include "../test.h"
#include <ctree/ctree.h>

static const unsigned int num_entries = 14;
static char * entries[] = {
    "Zero",
    "First entry",
    "Second entry",
    "Third entry",
    "Fourth entry",
    "Fifth entry",
    "Sixth entry",
    "Seventh entry",
    "8",
    "9",
    "entry 10",
    "entry 11",
    "entry 12",
    "entry-last",
};

int main()
{
    test_start("ctree");

    ct_t * ctree = ct_new();

    /* test adding values */
    {
        unsigned int i;
        _assert (ctree->len == 0);
        for (i = 0; i < num_entries; i++)
        {
            _assert (ct_add(ctree, entries[i], entries[i]) == 0);
        }
    }

    /* test is the length is correct */
    {
        _assert (ctree->len == num_entries);
    }

    /* test adding duplicated values */
    {
        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (ct_add(ctree, entries[i], entries[i]) == CT_EXISTS);
        }
    }

    /* test adding empty value */
    {
        _assert (ct_add(ctree, "", entries[0]) == CT_EXISTS);
    }

    /* test get */
    {
        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (ct_get(ctree, entries[i]) == entries[i]);
        }
    }

    /* test getn */
    {
        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (ct_getn(
                ctree,
                entries[i],
                strlen(entries[i])) == entries[i]);
        }
    }

    /* test pop value */
    {
        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (ct_pop(ctree, entries[i]) == entries[i]);
        }
    }

    /* test is the length is correct */
    {
        _assert (ctree->len == 0);
    }

    ct_free(ctree, NULL);

    return test_end();
}
