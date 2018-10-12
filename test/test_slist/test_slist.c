#include "../test.h"
#include <vec/vec.h>

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
    test_start("vec");

    /* vec_append_safe */
    {
        vec_t * vec = vec_new(0);
        _assert (vec->len == 0);
        _assert (vec->size == 0);

        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            _assert (vec_append_safe(&vec, entries[i]) == 0);
        }

        /* vec_copy */
        {
            vec_t * veccp = vec_copy(vec);
            unsigned int i;
            for (i = 0; i < num_entries; i++)
            {
                _assert (veccp->data[i] == entries[i]);
            }
            vec_free(veccp);
        }

        _assert (vec->len == num_entries);
        vec_free(vec);
    }

    /* vec_append */
    {
        vec_t * vec = vec_new(num_entries);
        _assert (vec->len == 0);
        _assert (vec->size == num_entries);

        unsigned int i;
        for (i = 0; i < num_entries; i++)
        {
            vec_append(vec, entries[i]);
        }

        _assert (vec->len == num_entries);

        /* vec_pop */
        for (i = num_entries; i-- > 0;)
        {
            _assert (vec_pop(vec) == entries[i]);
        }

        vec_free(vec);
    }


    return test_end();
}
