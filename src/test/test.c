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
#include <qpack/qpack.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <motd/motd.h>
#include <time.h>
#include <stdlib.h>
#include <cleri/parser.h>
#include <siri/grammar/grammar.h>
#include <cleri/grammar.h>
#include <assert.h>
#include <ctree/ctree.h>
#include <timeit/timeit.h>
#include <imap32/imap32.h>
#include <imap64/imap64.h>
#include <siri/db/pool.h>
#include <siri/db/points.h>

#define TEST_OK 0
#define TEST_FAILED 1

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
    return status;
}

static int test_qpack(void)
{
    test_start("Testing qpack");

    qp_packer_t * packer = qp_new_packer(32);
    qp_map_open(packer);
    qp_add_raw(packer, "data", 4);
    qp_array_open(packer);

    qp_add_map2(packer);
    qp_add_raw(packer, "name", 4);
    qp_add_raw(packer, "time_precision", 14);
    qp_add_raw(packer, "value", 5);
    qp_add_raw(packer, "s", 1);

    qp_add_map2(packer);
    qp_add_raw(packer, "name", 4);
    qp_add_raw(packer, "version", 7);
    qp_add_raw(packer, "value", 5);
    qp_add_raw(packer, "2.0.0", 5);

    qp_unpacker_t * unpacker = qp_new_unpacker(packer->buffer, packer->len);

    qp_free_unpacker(unpacker);
    qp_free_packer(packer);

    return test_end(TEST_OK);
}


static int test_cleri(void)
{
    test_start("Testing cleri");
    cleri_parse_result_t * pr;
    cleri_grammar_t * grammar = compile_grammar();

    /* should not break on full grammar */
    pr = cleri_parse(grammar,
            "select mean(1h + 1m) from \"series-001\", \"series-002\", "
            "\"series-003\" between 1360152000 and 1360152000 + 1d merge as "
            "\"series\" using mean(1)");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_free_parse_result(pr);

    /* should not break on empty grammar */
    pr = cleri_parse(grammar, "");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_free_parse_result(pr);

    /* should not break on single word grammar */
    pr = cleri_parse(grammar, "now");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_free_parse_result(pr);

    /* should not break on wrong grammar */
    pr = cleri_parse(grammar, "count serious?");

    /* is_valid should be false (0) */
    assert(pr->is_valid == 0);

    /* pos should be 6 */
    assert(pr->pos == 6);

    cleri_free_parse_result(pr);

    /* simple sum */
    pr = cleri_parse(grammar, "21 % 2");

    /* is_valid should be true (1) */
    assert(pr->is_valid == 1);

    cleri_free_parse_result(pr);

    /* free grammar */
    cleri_free_grammar(grammar);

    return test_end(TEST_OK);
}

static int test_ctree(void)
{
    test_start("Testing ctree");
    ct_node_t * ct = ct_new();
    assert (ct_add(ct, "Iris", "is gewoon Iris") == CT_OK);
    assert (ct_add(ct, "Iris", "Iris?") == CT_EXISTS);

    ct_free(ct);

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

    siridb_lookup_t * lookup = siridb_gen_lookup(4);
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

    siridb_points_t * points = siridb_new_points(5);
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

    siridb_free_points(points);

    return test_end(TEST_OK);
}

int run_tests(int flags)
{
    srand(time(NULL));
    int rc = 0;
    rc += test_qpack();
    rc += test_cleri();
    rc += test_ctree();
    rc += test_imap32();
    rc += test_imap64();
    rc += test_gen_pool_lookup();
    rc += test_points();

    if (rc == 0)
        printf("\nSuccesfully performed all tests!\n\n");
    else
        printf("\nERROR, %d tests have failed!\n\n", rc);

    return rc;
}
