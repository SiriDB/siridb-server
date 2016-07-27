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
#include <cleri/parser.h>
#include <cleri/grammar.h>
#include <ctree/ctree.h>
#include <timeit/timeit.h>
#include <imap32/imap32.h>
#include <imap64/imap64.h>
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
    siridb_points_t * points = siridb_points_new(10, SIRIDB_POINTS_TP_INT);
    uint64_t timestamps[10] =   {3, 6, 7, 10, 11, 13, 14, 15, 25, 27};
    int64_t values[10] =        {1, 3, 0, 2,  4,  8,  3,  5,  6,  3};
    qp_via_t val;

    siridb_init_aggregates();

    for (int i = 0; i < 10; i++)
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
    qp_add_type(packer, QP_MAP_OPEN);
    qp_add_raw(packer, "data", 4);
    qp_add_type(packer, QP_ARRAY_OPEN);

    qp_add_type(packer, QP_MAP2);
    qp_add_raw(packer, "name", 4);
    qp_add_raw(packer, "time_precision", 14);
    qp_add_raw(packer, "value", 5);
    qp_add_raw(packer, "s", 1);

    qp_add_type(packer, QP_MAP2);
    qp_add_raw(packer, "name", 4);
    qp_add_raw(packer, "version", 7);
    qp_add_raw(packer, "value", 5);
    qp_add_raw(packer, "2.0.0", 5);

    qp_unpacker_t * unpacker = qp_unpacker_new(packer->buffer, packer->len);

    qp_unpacker_free(unpacker);
    qp_packer_free(packer);

    return test_end(TEST_OK);
}


static int test_cleri(void)
{
    test_start("Testing cleri");
    cleri_parser_t * pr;
    cleri_grammar_t * grammar = compile_grammar();

    /* should not break on full grammar */
    pr = cleri_parser_new(grammar,
            "select mean(1h + 1m) from \"series-001\", \"series-002\", "
            "\"series-003\" between 1360152000 and 1360152000 + 1d merge as "
            "\"series\" using mean(1)");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parser_free(pr);

    /* should not break on empty grammar */
    pr = cleri_parser_new(grammar, "");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parser_free(pr);

    /* should not break on single word grammar */
    pr = cleri_parser_new(grammar, "now");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parser_free(pr);

    /* should not break on wrong grammar */
    pr = cleri_parser_new(grammar, "count serious?");

    /* is_valid should be false (0) */
    assert(pr->is_valid == 0);

    /* pos should be 6 */
    assert(pr->pos == 6);

    cleri_parser_free(pr);

    /* simple sum */
    pr = cleri_parser_new(grammar, "21 % 2");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_parser_free(pr);

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

    // get_sure should return the corrent result
    assert (strcmp(*ct_get_sure(ct, "Iris"), "is gewoon Iris") == 0);

    // get_sure with new key should create a CT_EMPTY
    assert (ct_is_empty(*ct_get_sure(ct, "Sasha")));

    // len should be 2 by now
    assert (ct->len == 4);
    assert (ct_get(ct, "Dummy") == NULL);
    assert (strcmp(ct_get(ct, "Iris"), "is gewoon Iris") == 0);
    assert (strcmp(ct_pop(ct, "Iris"), "is gewoon Iris") == 0);
    assert (strcmp(ct_pop(ct, "Iris1"), "is gewoon Iris1") == 0);
    assert (ct_pop(ct, "Sasha") == CT_EMPTY);
    assert (strcmp(ct_get(ct, "Iris2"), "is gewoon Iris2") == 0);
    assert (ct->len == 1);


    ct_free(ct, NULL);

    return test_end(TEST_OK);
}

static int test_imap32(void)
{
    test_start("Testing imap32");
    imap32_t * imap = imap32_new();

    imap32_add(imap, 1234567, "Sasientje");
    assert (imap->offset == 18);
    assert (imap->size == 1);
    imap32_add(imap, 234567, "Iriske");
    assert (imap->offset == 3);
    assert (imap->size == 16);
    assert (strcmp(imap32_get(imap, 1234567), "Sasientje") == 0);
    assert (strcmp(imap32_get(imap, 234567), "Iriske") == 0);
    assert (strcmp(imap32_pop(imap, 234567), "Iriske") == 0);
    assert (imap32_get(imap, 234567) == NULL);
    assert (strcmp(imap32_pop(imap, 1234567), "Sasientje") == 0);

    imap32_add(imap, 123456, "Sasientje");
    imap32_add(imap, 834567, "Iriske");
    assert (strcmp(imap32_pop(imap, 834567), "Iriske") == 0);
    assert (strcmp(imap32_pop(imap, 123456), "Sasientje") == 0);

    imap32_free(imap);
    return test_end(TEST_OK);
}

static int test_imap64(void)
{
    test_start("Testing imap64");
    imap64_t * imap = imap64_new();

    imap64_add(imap, 16, "Sasientje");
    imap64_add(imap, 1676, "Juul");
    imap64_add(imap, 0, "Iriske");
    assert (imap->len == 3);
    assert (strcmp(imap64_get(imap, 16), "Sasientje") == 0);
    assert (strcmp(imap64_get(imap, 0), "Iriske") == 0);
    assert (strcmp(imap64_pop(imap, 0), "Iriske") == 0);
    assert (imap64_get(imap, 0) == NULL);
    assert (strcmp(imap64_pop(imap, 16), "Sasientje") == 0);
    assert (imap->len == 1);

    imap64_add(imap, 451246163432574, "Sasientje");
    imap64_add(imap, 23456, "Iriske");
    assert (strcmp(imap64_pop(imap, 23456), "Iriske") == 0);
    assert (strcmp(imap64_pop(imap, 451246163432574), "Sasientje") == 0);

    imap64_free(imap);
    return test_end(TEST_OK);
}

static int test_gen_pool_lookup(void)
{
    test_start("Testing test_gen_pool_lookup");

    siridb_lookup_t * lookup = siridb_pools_gen_lookup(4);
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

    siridb_points_t * points = siridb_points_new(5, SIRIDB_POINTS_TP_INT);
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
        assert (i == point->val.int64);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_COUNT - KW_OFFSET];
    aggr.group_by = 6;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MAX - KW_OFFSET];
    aggr.group_by = 10;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 3);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MEAN - KW_OFFSET];
    aggr.group_by = 4;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 5);
    assert (result->tp == SIRIDB_POINTS_TP_DOUBLE);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MEDIAN - KW_OFFSET];
    aggr.group_by = 7;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == SIRIDB_POINTS_TP_DOUBLE);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MEDIAN_HIGH - KW_OFFSET];
    aggr.group_by = 7;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MEDIAN_LOW - KW_OFFSET];
    aggr.group_by = 7;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 4);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
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

    aggr.cb = siridb_aggregates[CLERI_GID_K_MIN - KW_OFFSET];
    aggr.group_by = 2;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 9);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
    assert (result->data->ts == 4 && result->data->val.int64 == 1);
    assert ((result->data + 5)->ts == 14 &&
            (result->data + 5)->val.int64 == 3);

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

    aggr.cb = siridb_aggregates[CLERI_GID_K_SUM - KW_OFFSET];
    aggr.group_by = 5;

    result = siridb_aggregate(points, &aggr, err_msg);

    assert (result != NULL);
    assert (result->len == 5);
    assert (result->tp == SIRIDB_POINTS_TP_INT);
    assert (result->data->ts == 5 && result->data->val.int64 == 1);
    assert ((result->data + 2)->ts == 15 &&
            (result->data + 2)->val.int64 == 20);

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
    siridb_access_t access_bit = 0;

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
    assert (strcmp(buffer, "select, show and list") == 0);

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

int run_tests(int flags)
{
    timeit_t start;
    timeit_start(&start);
    int rc = 0;
    rc += test_qpack();
    rc += test_cleri();
    rc += test_ctree();
    rc += test_imap32();
    rc += test_imap64();
    rc += test_gen_pool_lookup();
    rc += test_points();
    rc += test_aggr_count();
    rc += test_aggr_max();
    rc += test_aggr_mean();
    rc += test_aggr_median();
    rc += test_aggr_median_high();
    rc += test_aggr_median_low();
    rc += test_aggr_min();
    rc += test_aggr_sum();
    rc += test_iso8601();
    rc += test_expr();
    rc += test_access();
    rc += test_version();

    printf("\nSuccesfully performed %d tests in %.3f milliseconds!\n\n",
            rc, timeit_stop(&start));

    return 0;
}
