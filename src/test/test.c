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
#include <siri/db/pool.h>

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

static int test_qpack1(void)
{
    test_start("Testing qpack (1)");

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


static int test_cleri1(void)
{
    test_start("Testing cleri (1)");
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

static int test_ctree1(void)
{
    test_start("Testing ctree (1)");
    ct_node_t * ct = ct_new();
    assert (ct_add(ct, "Iris", "is gewoon Iris") == CT_OK);
    assert (ct_add(ct, "Iris", "Iris?") == CT_EXISTS);

    ct_free(ct);

    return test_end(TEST_OK);
}
static int test_imap32_1(void)
{
    test_start("Testing imap32 (1)");
    imap32_t * imap = imap32_new();

    imap32_add(imap, 1, "Iriske");
    imap32_add(imap, 1234567, "Sasientje");
    assert (strcmp(imap32_get(imap, 1), "Iriske") == 0);
    assert (strcmp(imap32_pop(imap, 1), "Iriske") == 0);
    assert (imap32_get(imap, 1) == NULL);
    assert (strcmp(imap32_pop(imap, 1234567), "Sasientje") == 0);
    imap32_free(imap);
    return test_end(TEST_OK);
}

static int test_gen_pool_lookup_1(void)
{
    test_start("Testing test_gen_pool_lookup_1 (1)");

    siridb_lookup_t * lookup = siridb_gen_lookup(4);
    uint16_t match[30] = {
            0, 1, 0, 2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0,
            2, 3, 1, 0, 3, 3, 2, 2, 1, 0, 1, 0, 2, 3, 1};

    for (int i = 0; i < 30; i++)
        assert(match[i] == (*lookup)[i]);

    free(lookup);
    return test_end(TEST_OK);
}

int run_tests(int flags)
{
    srand(time(NULL));
    int rc = 0;
    rc += test_qpack1();
    rc += test_cleri1();
    rc += test_ctree1();
    rc += test_imap32_1();
    rc += test_gen_pool_lookup_1();

    if (rc == 0)
        printf("\nSuccesfully performed all tests!\n\n");
    else
        printf("\nERROR, %d tests have failed!\n\n", rc);

    return rc;
}
