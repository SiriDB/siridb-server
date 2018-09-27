#include "../test.h"
#include <slist/slist.h>

const unsigned int num_entries = 14;
char * entries[] = {
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
    "entry-last"
};

int main()
{
    test_start("slist");

    /* slist_append_safe */
    {
        slist_t * slist = slist_new(0);
        _assert (slist->len == 0);
        _assert (slist->size == 0);

        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (slist_append_safe(&slist, entries[i]) == 0);
        }

        /* slist_copy */
        {
            slist_t * slistcp = slist_copy(slist);
            unsigned int i;
            for (i = 0; i < num_entries; i++)
            {
                _assert (slistcp->data[i] == entries[i]);
            }
            slist_free(slistcp);
        }

        _assert (slist->len == num_entries);
        slist_free(slist);
    }

    /* slist_append */
    {
        slist_t * slist = slist_new(num_entries);
        _assert (slist->len == 0);
        _assert (slist->size == num_entries);

        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            slist_append(slist, entries[i]);
        }

        _assert (slist->len == num_entries);

        /* slist_pop */
        for (i = num_entries; i-- > 0;)
        {
            _assert (slist_pop(slist) == entries[i]);
        }

        slist_free(slist);
    }


    return test_end();
}
