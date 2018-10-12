#include "../test.h"
#include <inttypes.h>
#include <imap/imap.h>

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

typedef struct
{
    uint32_t ref;
    uint32_t id;
} test_series_t;

static test_series_t series_a = {
    .ref=0,
    .id=11
};
static test_series_t series_b = {
    .ref=0,
    .id=987
};
static test_series_t series_c = {
    .ref=0,
    .id=219
};
static test_series_t series_d = {
    .ref=0,
    .id=9
};
static test_series_t series_e = {
    .ref=0,
    .id=988
};

static imap_t * imap_dst;
static imap_t * imap_tmp;

static int test__imap_decref_cb(char * series)
{
    ((vec_object_t *) series)->ref--;
    return 1;
}

static int test__imap_id_count_cb(
        char * series,
        void * data __attribute__((unused)))
{
    return ((test_series_t *) series)->id;
}

static void test__imap_setup(void)
{
    imap_dst = imap_new();
    imap_tmp = imap_new();

    imap_set(imap_dst, series_a.id, &series_a);
    series_a.ref++;

    imap_set(imap_dst, series_b.id, &series_b);
    series_b.ref++;

    imap_set(imap_dst, series_c.id, &series_c);
    series_c.ref++;

    imap_set(imap_tmp, series_b.id, &series_b);
    series_b.ref++;

    imap_set(imap_tmp, series_c.id, &series_c);
    series_c.ref++;

    imap_set(imap_tmp, series_d.id, &series_d);
    series_d.ref++;

    imap_set(imap_tmp, series_e.id, &series_e);
    series_e.ref++;
}

static int test_imap_add_set_get_pod(void)
{
    test_start("imap (add, set, get, pop)");

    imap_t * imap = imap_new();

    /* test is the length is correct */
    _assert (imap->len == 0);

    /* test imap_add */
    {
        unsigned int i;
        for (i=0; i < num_entries; i++)
        {
            _assert (imap_add(imap, i, entries[i]) == 0);
        }
        _assert (imap->len == num_entries);
    }

    /* test imap_add duplicates */
    {
        unsigned int i;
        for (i=0; i < num_entries; i++)
        {
            _assert (imap_add(imap, i, "duplicate") != 0);
        }
    }

    /* test imap_get */
    {
        unsigned int i;
        for (i=0; i < num_entries; i++)
        {
            _assert (imap_get(imap, i) == entries[i]);
        }
    }

    /* test imap_set */
    {
        unsigned int i;
        for (i=0; i < num_entries; i++)
        {
            unsigned int j = i * 2;
            _assert (imap_set(
                imap, j, entries[i]) == (int) (j < num_entries ? 0 : 1));
        }

        for (i=0; i < num_entries; i++)
        {
            unsigned int j = i * 2;
            _assert (imap_get(imap, j) == entries[i]);
        }
    }

    /* test imap_pop */
    {
        unsigned int i;
        for (i=0; i < num_entries; i++)
        {
            unsigned int j = i * 2;
            _assert (imap_pop(imap, j) == entries[i]);
        }

        for (i=1; i < num_entries; i+=2)
        {
            _assert (imap_pop(imap, i) == entries[i]);
        }

        _assert (imap->len == 0);
    }

    imap_free(imap, NULL);

    return test_end();
}

static int test_imap_union()
{
    test_start("imap (union)");

    test__imap_setup();

    imap_union_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    _assert (imap_dst->len == 5);
    _assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_a.id +
            series_b.id +
            series_c.id +
            series_d.id +
            series_e.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);

    _assert (series_a.ref == 0);
    _assert (series_b.ref == 0);
    _assert (series_c.ref == 0);
    _assert (series_d.ref == 0);
    _assert (series_e.ref == 0);

    return test_end();
}

static int test_imap_intersection(void)
{
    test_start("imap (intersection)");

    test__imap_setup();

    imap_intersection_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    _assert (imap_dst->len == 2);
    _assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_b.id +
            series_c.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    _assert (series_a.ref == 0);
    _assert (series_b.ref == 0);
    _assert (series_c.ref == 0);
    _assert (series_d.ref == 0);
    _assert (series_e.ref == 0);

    return test_end();
}

static int test_imap_difference(void)
{
    test_start("imap (difference)");

    test__imap_setup();

    imap_difference_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    _assert (imap_dst->len == 1);
    _assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) series_a.id);

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    _assert (series_a.ref == 0);
    _assert (series_b.ref == 0);
    _assert (series_c.ref == 0);
    _assert (series_d.ref == 0);
    _assert (series_e.ref == 0);

    return test_end();
}

static int test_imap_symmetric_difference(void)
{
    test_start("imap (symmetric_difference)");

    test__imap_setup();

    imap_symmetric_difference_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    _assert (imap_dst->len == 3);
    _assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_a.id +
            series_d.id +
            series_e.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    _assert (series_a.ref == 0);
    _assert (series_b.ref == 0);
    _assert (series_c.ref == 0);
    _assert (series_d.ref == 0);
    _assert (series_e.ref == 0);

    return test_end();
}

int main()
{
    return (
        test_imap_add_set_get_pod() ||
        test_imap_union() ||
        test_imap_intersection() ||
        test_imap_difference() ||
        test_imap_symmetric_difference() ||
        0
    );
}
