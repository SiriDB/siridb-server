/*
 * test.c - testing functions for SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-03-2016
 *
 */
#include <test/test.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <qpack/qpack.h>
#include <motd/motd.h>
#include <cleri/cleri.h>
#include <ctree/ctree.h>
#include <timeit/timeit.h>
#include <imap/imap.h>
#include <iso8601/iso8601.h>
#include <expr/expr.h>
#include <siri/grammar/grammar.h>
#include <siri/grammar/gramp.h>
#include <siri/db/aggregate.h>
#include <siri/db/db.h>
#include <siri/db/pools.h>
#include <siri/db/points.h>
#include <siri/db/access.h>
#include <siri/version.h>
#include <siri/db/lookup.h>
#include <strextra/strextra.h>

#define TEST_OK 1
#define TEST_FAILED -1

#define TEST_MSG_OK     "....OK"
#define TEST_MSG_FAILED "FAILED"

const char * padding =
        "........................................"
        "........................................";

static void test_start(char * test_name)
{
    int padlen = 80 - strlen(test_name);
    printf("%s%*.*s", test_name, padlen, padlen, padding);
}

static int test_end(int status)
{
    printf("%s\n", (status == TEST_OK) ? TEST_MSG_OK : TEST_MSG_FAILED);
    assert (status == TEST_OK);
    return status;
}

static siridb_points_t * prepare_points(void)
{
    siridb_points_t * points = siridb_points_new(10, TP_INT);
    uint64_t timestamps[10] =   {3, 6, 7, 10, 11, 13, 14, 15, 25, 27};
    int64_t values[10] =        {1, 3, 0, 2,  4,  8,  3,  5,  6,  3};
    qp_via_t val;
    int i;

    siridb_init_aggregates();

    for (i = 0; i < 10; i++)
    {
        val.int64 = values[i];
        siridb_points_add_point(points, &timestamps[i], &val);
    }

    return points;
}

static int test_qpack(void)
{
    test_start("Testing qpack");

    qp_packer_t * packer = qp_packer_new(32);
    qp_unpacker_t unpacker;
    qp_obj_t qpi;
    int64_t i;

    qp_add_type(packer, QP_MAP_OPEN);
    qp_add_raw(packer, (const unsigned char *) "data", 4);
    qp_add_type(packer, QP_ARRAY_OPEN);

    qp_add_type(packer, QP_MAP2);
    qp_add_raw(packer, (const unsigned char *) "name", 4);
    qp_add_raw(packer, (const unsigned char *) "time_precision", 14);
    qp_add_raw(packer, (const unsigned char *) "value", 5);
    qp_add_raw(packer, (const unsigned char *) "s", 1);

    qp_add_type(packer, QP_MAP2);
    qp_add_raw(packer, (const unsigned char *) "name", 4);
    qp_add_raw(packer, (const unsigned char *) "version", 7);
    qp_add_raw(packer, (const unsigned char *) "value", 5);
    qp_add_raw(packer, (const unsigned char *) "2.0.0", 5);

    qp_packer_free(packer);

    packer = qp_packer_new(512);

    qp_add_type(packer, QP_ARRAY_OPEN);
    for (i =-100; i < 100; i++)
    {
        qp_add_int8(packer, i);
    }
    qp_add_type(packer, QP_ARRAY_CLOSE);

    qp_unpacker_init(&unpacker, packer->buffer, packer->len);

    assert (qp_is_array(qp_next(&unpacker, NULL)));
    for (i =-100; i < 100; i++)
    {
        assert (qp_is_int(qp_next(&unpacker, &qpi)));
        assert (qpi.via.int64 == i);
    }

    qp_packer_free(packer);

    return test_end(TEST_OK);
}


static int test_cleri(void)
{
    test_start("Testing cleri");
    cleri_parse_t * pr;
    cleri_grammar_t * grammar = compile_grammar();

    /* should not break on full grammar */
    pr = cleri_parse(grammar,
            "select mean(1h + 1m) from \"series-001\", \"series-002\", "
            "\"series-003\" between 1360152000 and 1360152000 + 1d merge as "
            "\"series\" using mean(1)");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parse_free(pr);

    /* should not break on empty grammar */
    pr = cleri_parse(grammar, "");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parse_free(pr);

    /* should not break on single word grammar */
    pr = cleri_parse(grammar, "now");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parse_free(pr);

    /* should not break on wrong grammar */
    pr = cleri_parse(grammar, "count serious?");

    /* is_valid should be false (0) */
    assert(pr->is_valid == 0);

    /* pos should be 6 */
    assert(pr->pos == 6);

    cleri_parse_free(pr);

    /* simple sum */
    pr = cleri_parse(grammar, "21 % 2");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parse_free(pr);

    /* free grammar */
    cleri_grammar_free(grammar);

    return test_end(TEST_OK);
}

static int test_ctree(void)
{
    test_start("Testing ctree");
    ct_t * ct = ct_new();

    // first add should be successful
    assert (ct_add(ct, "Iris", "is gewoon Iris") == CT_OK);
    assert (ct_add(ct, "Iris1", "is gewoon Iris1") == CT_OK);
    assert (ct_add(ct, "Iris2", "is gewoon Iris2") == CT_OK);

    // key exists
    assert (ct_add(ct, "Iris", "Iris?") == CT_EXISTS);

    // len should be 3 by now
    assert (ct->len == 3);
    assert (strcmp(ct_getn(ct, "Iris1!", 5), "is gewoon Iris1") == 0);
    assert (strcmp(ct_get(ct, "Iris"), "is gewoon Iris") == 0);
    assert (strcmp(ct_pop(ct, "Iris"), "is gewoon Iris") == 0);
    assert (strcmp(ct_pop(ct, "Iris1"), "is gewoon Iris1") == 0);
    assert (strcmp(ct_get(ct, "Iris2"), "is gewoon Iris2") == 0);
    assert (ct->len == 1);

    /* This will hit all steps in dec node */
    assert (ct_add(ct, "I", "Een korte naam") == CT_OK);
    assert (ct_add(ct, "t", "Een andere naam") == CT_OK);
    assert (strcmp(ct_pop(ct, "Iris2"), "is gewoon Iris2") == 0);

    /* This will hit the pop merge...*/
    assert (ct_add(ct, "Iris", "Hoi Iris") == CT_OK);
    assert (strcmp(ct_pop(ct, "I"), "Een korte naam") == 0);
    assert (ct_add(ct, "t", "Iris?") == CT_EXISTS);

    /* Makes sure all possible return values for CT_add are hit */
    assert (ct_add(ct, "IrIs", "Hoi IrIs") == CT_OK);
    assert (ct_add(ct, "Ir", "Hoi Ir") == CT_OK);

    /* Hits a recursive CT_add */
    assert (ct_add(ct, "Iri!!", "Hoi Ir!") == CT_OK);


    ct_free(ct, NULL);

    return test_end(TEST_OK);
}

static int test__imap_cb(char * data, char * cmp)
{
    assert (strcmp(data, cmp) == 0);
    return 1;
}

static int test_imap(void)
{
    test_start("Testing imap");
    imap_t * imap = imap_new();

    imap_set(imap, 14, "Sasientje");
    imap_set(imap, 20130602, "Iriske");
    assert (imap_set(imap, 726, "Jip") == 1);
    assert (imap_set(imap, 726, "Job") == 0);
    assert (imap_add(imap, 726, "Jap") == -2);
    assert (imap_add(imap, 2011, "Tijs") == 0);
    imap_set(imap, 0, "Joente");

    assert (imap->len == 5);
    assert (strcmp(imap_get(imap, 14), "Sasientje") == 0);
    assert (strcmp(imap_get(imap, 20130602), "Iriske") == 0);
    assert (imap_get(imap, 123) == NULL);

    assert (strcmp(imap_pop(imap, 726), "Job") == 0);
    assert (imap_pop(imap, 726) == NULL);
    assert (strcmp(imap_pop(imap, 2011), "Tijs") == 0);
    assert (imap->len == 3);

    imap_free(imap, NULL);

    imap = imap_new();
    imap_set(imap, 42, "Sasientje");
    assert (imap_walk(imap, (imap_cb) &test__imap_cb, "Sasientje") == 1);
    size_t n = 43;
    imap_walkn(imap, &n, (imap_cb) &test__imap_cb, "Sasientje");
    assert (n == 42);
    n = 1;

    imap_set(imap, 3, "Iriske");
    imap_walkn(imap, &n, (imap_cb) &test__imap_cb, "Iriske");
    assert (strcmp(imap_pop(imap, 3), "Iriske") == 0);

    imap_free(imap, NULL);

    return test_end(TEST_OK);
}

static int test__imap_decref_cb(char * series)
{
    ((slist_object_t *) series)->ref--;
    return 1;
}

static int test__imap_id_count_cb(
        char * series,
        void * data __attribute__((unused)))
{
    return ((siridb_series_t *) series)->id;
}

static siridb_series_t series_a = {
    .ref=0,
    .id=11
};
static siridb_series_t series_b = {
    .ref=0,
    .id=987
};
static siridb_series_t series_c = {
    .ref=0,
    .id=219
};
static siridb_series_t series_d = {
    .ref=0,
    .id=9
};
static siridb_series_t series_e = {
    .ref=0,
    .id=988
};

static imap_t * imap_dst;
static imap_t * imap_tmp;


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

static int test_imap_union(void)
{
    test_start("Testing imap union");

    test__imap_setup();

    imap_union_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    assert (imap_dst->len == 5);
    assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_a.id +
            series_b.id +
            series_c.id +
            series_d.id +
            series_e.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    assert (series_a.ref == 0);
    assert (series_b.ref == 0);
    assert (series_c.ref == 0);
    assert (series_d.ref == 0);
    assert (series_e.ref == 0);

    return test_end(TEST_OK);
}

static int test_imap_intersection(void)
{
    test_start("Testing imap intersection");

    test__imap_setup();

    imap_intersection_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    assert (imap_dst->len == 2);
    assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_b.id +
            series_c.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    assert (series_a.ref == 0);
    assert (series_b.ref == 0);
    assert (series_c.ref == 0);
    assert (series_d.ref == 0);
    assert (series_e.ref == 0);

    return test_end(TEST_OK);
}

static int test_imap_difference(void)
{
    test_start("Testing imap difference");

    test__imap_setup();

    imap_difference_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    assert (imap_dst->len == 1);
    assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) series_a.id);

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    assert (series_a.ref == 0);
    assert (series_b.ref == 0);
    assert (series_c.ref == 0);
    assert (series_d.ref == 0);
    assert (series_e.ref == 0);

    return test_end(TEST_OK);
}

static int test_imap_symmetric_difference(void)
{
    test_start("Testing imap symmetric_difference");

    test__imap_setup();

    imap_symmetric_difference_ref(
            imap_dst,
            imap_tmp,
            (imap_free_cb) test__imap_decref_cb);

    assert (imap_dst->len == 3);
    assert (imap_walk(
                imap_dst,
                (imap_cb) &test__imap_id_count_cb,
                NULL) == (int) (
            series_a.id +
            series_d.id +
            series_e.id));

    imap_free(imap_dst, (imap_free_cb) test__imap_decref_cb);
    assert (series_a.ref == 0);
    assert (series_b.ref == 0);
    assert (series_c.ref == 0);
    assert (series_d.ref == 0);
    assert (series_e.ref == 0);

    return test_end(TEST_OK);
}

static int test_gen_pool_lookup(void)
{
    test_start("Testing test_gen_pool_lookup");

    siridb_lookup_t * lookup = siridb_lookup_new(4);
    uint16_t match[30] = {
            0, 1, 0, 2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0,
            2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0, 2, 3, 1};

    for (int i = 0; i < 30; i++)
        assert(match[i] == (*lookup)[i]);

    free(lookup);
    return test_end(TEST_OK);
}

static int test_points(void)
{
    test_start("Testing points");

    siridb_points_t * points = siridb_points_new(5, TP_INT);
    siridb_point_t * point;
    qp_via_t val;
    uint64_t timestamps[5] = {4, 6, 3, 5, 7};
    int64_t values[5] = {1, 3, 0, 2, 4};

    for (int i = 0; i < 5; i++)
    {
        val.int64 = values[i];
        siridb_points_add_point(points, &timestamps[i], &val);
    }

    for (size_t i = 0; i < points->len; i++)
    {
        point = points->data + i;
        assert (i == (size_t) point->val.int64);
    }

    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_count(void)
{
    test_start("Testing aggregation count");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_COUNT;
    aggr.group_by = 6;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 6 && result->data->val.int64 == 2);
    assert ((result->data + 3)->ts == 30 &&
            (result->data + 3)->val.int64 == 2);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_max(void)
{
    test_start("Testing aggregation max");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MAX;
    aggr.group_by = 10;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 3);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 10 && result->data->val.int64 == 3);
    assert ((result->data + 2)->ts == 30 &&
            (result->data + 2)->val.int64 == 6);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_mean(void)
{
    test_start("Testing aggregation mean");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEAN;
    aggr.group_by = 4;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 5);
    assert (result->tp == TP_DOUBLE);
    assert (result->data->ts == 4 && result->data->val.real == 1.0);
    assert ((result->data + 4)->ts == 28 &&
            (result->data + 4)->val.real == 4.5);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_median(void)
{
    test_start("Testing aggregation median");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == TP_DOUBLE);
    assert (result->data->ts == 7 && result->data->val.real == 1.0);
    assert ((result->data + 1)->ts == 14 &&
            (result->data + 1)->val.real == 3.5);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_median_high(void)
{
    test_start("Testing aggregation median_high");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN_HIGH;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 7 && result->data->val.int64 == 1);
    assert ((result->data + 1)->ts == 14 &&
            (result->data + 1)->val.int64 == 4);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_median_low(void)
{
    test_start("Testing aggregation median_low");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MEDIAN_LOW;
    aggr.group_by = 7;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 7 && result->data->val.int64 == 1);
    assert ((result->data + 1)->ts == 14 &&
            (result->data + 1)->val.int64 == 3);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_min(void)
{
    test_start("Testing aggregation min");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_MIN;
    aggr.group_by = 2;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 9);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 4 && result->data->val.int64 == 1);
    assert ((result->data + 5)->ts == 14 &&
            (result->data + 5)->val.int64 == 3);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_pvariance(void)
{
    test_start("Testing pvariance");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_PVARIANCE;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 5);
    assert (result->tp == TP_DOUBLE);
    assert (result->data->ts == 5 && result->data->val.real == 0.0);
    assert ((result->data + 2)->ts == 15 &&
            (result->data + 2)->val.real == 3.5);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_sum(void)
{
    test_start("Testing aggregation sum");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_SUM;
    aggr.group_by = 5;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 5);
    assert (result->tp == TP_INT);
    assert (result->data->ts == 5 && result->data->val.int64 == 1);
    assert ((result->data + 2)->ts == 15 &&
            (result->data + 2)->val.int64 == 20);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_aggr_variance(void)
{
    test_start("Testing variance");

    siridb_aggr_t aggr;
    siridb_points_t * result;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];
    siridb_points_t * points = prepare_points();

    aggr.gid = CLERI_GID_F_VARIANCE;
    aggr.group_by = 6;
    aggr.limit = 0;
    aggr.offset = 0;

    result = siridb_aggregate_run(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == TP_DOUBLE);
    assert (result->data->ts == 6 && result->data->val.real == 2.0);
    assert ((result->data + 1)->ts == 12 &&
            (result->data + 1)->val.real == 4.0);

    siridb_points_free(result);
    siridb_points_free(points);

    return test_end(TEST_OK);
}

static int test_iso8601(void)
{
    test_start("Testing iso8601");

    iso8601_tz_t amsterdam = iso8601_tz("Europe/Amsterdam");
    assert(amsterdam > 0);

    iso8601_tz_t utc = iso8601_tz("UTC");
    assert(utc > 0);

    /* Some extra time zone test for case sensitivity etc. */
    assert(iso8601_tz("NAIVE") == 0);
    assert(iso8601_tz("europe/KIEV") > 0);
    assert(iso8601_tz("Ams") < 0);

    /* Test parsing a year with time-zone information */
    assert(iso8601_parse_date("2013", amsterdam) == 1356994800);

    /* Customer offset should be preferred over UTC */
    assert(iso8601_parse_date("2013+01", utc) == 1356994800);

    /* UTC should be preferred over given time-zone (Amsterdam) */
    assert(iso8601_parse_date("2013Z", amsterdam) == 1356998400);

    /* Test with minute precision */
    assert(iso8601_parse_date("2013-02-06T13:01:12", utc) == 1360155672);

    /* Test short written */
    assert(iso8601_parse_date("2013-2-6 13:1:12", utc) == 1360155672);

    /* Test summer time */
    assert(iso8601_parse_date("2016-04-21", amsterdam) == 1461189600);

    /* Test error */
    assert(iso8601_parse_date("2016 04 21", amsterdam) == -1);

    /* Test last day in year */
    assert(iso8601_parse_date("2013-12-31", utc) == 1388448000);

    return test_end(TEST_OK);
}

static int test_expr(void)
{
    test_start("Testing expressions");
    int64_t result;

    assert(expr_parse(&result, "5+37") == 0 && result == 42);
    assert(expr_parse(&result, "2+2*20") == 0 && result == 42);
    assert(expr_parse(&result, "(2+4)*7") == 0 && result == 42);
    assert(expr_parse(&result, "16%10*7") == 0 && result == 42);
    assert(expr_parse(&result, "7*14%56") == 0 && result == 42);
    assert(expr_parse(&result, "21/3*6") == 0 && result == 42);
    assert(expr_parse(&result, "22/3*6") == 0 && result == 42);

    /* division by zero is not a good idea */
    assert(expr_parse(&result, "42/(2-2)") == EXPR_DIVISION_BY_ZERO);

    /* module by zero is not a good idea either */
    assert(expr_parse(&result, "42%(2-2)") == EXPR_MODULO_BY_ZERO);

    return test_end(TEST_OK);
}

static int test_access(void)
{
    test_start("Testing access");
    char buffer[SIRIDB_ACCESS_STR_MAX];
    uint32_t access_bit = 0;

    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "no access") == 0);

    access_bit |= SIRIDB_ACCESS_SHOW;
    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "show") == 0);

    access_bit |= SIRIDB_ACCESS_SELECT;
    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "select and show") == 0);

    access_bit |= SIRIDB_ACCESS_LIST;
    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "list, select and show") == 0);

    access_bit |= SIRIDB_ACCESS_PROFILE_WRITE;
    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "write") == 0);

    access_bit &= ~SIRIDB_ACCESS_INSERT;
    siridb_access_to_str(buffer, access_bit);
    assert (strcmp(buffer, "read and create") == 0);

    assert (siridb_access_from_strn("read", 4) == (SIRIDB_ACCESS_PROFILE_READ));
    assert (siridb_access_from_strn("list", 4) == SIRIDB_ACCESS_LIST);

    return test_end(TEST_OK);
}

static int test_version(void)
{
    test_start("Testing version");

    assert (siri_version_cmp("1.0.0", "2.0.0") < 0);
    assert (siri_version_cmp("2.0.0", "1.0.0") > 0);
    assert (siri_version_cmp("2.2.0", "2.32.0") < 0);
    assert (siri_version_cmp("2.32.0", "2.2.0") > 0);
    assert (siri_version_cmp("2.0.5", "2.0.22") < 0);
    assert (siri_version_cmp("2.0.22", "2.0.5") > 0);
    assert (siri_version_cmp("2.0", "2.0.0") < 0);
    assert (siri_version_cmp("2.0.2", "2.0") > 0);
    assert (siri_version_cmp("a", "") > 0);
    assert (siri_version_cmp("", "b") < 0);
    assert (siri_version_cmp("", "") == 0);
    assert (siri_version_cmp("2.0.0", "2.0.0") == 0);

    return test_end(TEST_OK);
}

int test_strx_to_double(void)
{
    test_start("Testing strx_to_double");

    assert (strx_to_double("0.5", 3) == 0.5);
    assert (strx_to_double("0.55", 3) == 0.5);
    assert (strx_to_double("123.456", 7) == 123.456);
    assert (strx_to_double("123", 3) == 123);
    assert (strx_to_double("123.", 4) == 123);
    assert (strx_to_double("123456.", 3) == 123);
    return test_end(TEST_OK);
}

int run_tests(void)
{
    timeit_t start;
    timeit_start(&start);
    int rc = 0;
    rc += test_qpack();
    rc += test_cleri();
    rc += test_ctree();
    rc += test_imap();
    rc += test_imap_union();
    rc += test_imap_intersection();
    rc += test_imap_difference();
    rc += test_imap_symmetric_difference();
    rc += test_gen_pool_lookup();
    rc += test_points();
    rc += test_aggr_count();
    rc += test_aggr_max();
    rc += test_aggr_mean();
    rc += test_aggr_median();
    rc += test_aggr_median_high();
    rc += test_aggr_median_low();
    rc += test_aggr_min();
    rc += test_aggr_pvariance();
    rc += test_aggr_sum();
    rc += test_aggr_variance();
    rc += test_iso8601();
    rc += test_expr();
    rc += test_access();
    rc += test_version();
    rc += test_strx_to_double();

    printf("\nSuccessfully performed %d tests in %.3f milliseconds!\n\n",
            rc, timeit_stop(&start));

    return 0;
}
