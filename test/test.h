#ifndef SIRIDB_TEST_H_
#define SIRIDB_TEST_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#define TEST_OK 0
#define TEST_FAILED 1
#define TEST_MSG_OK     "....\x1B[32mOK\x1B[0m"
#define TEST_MSG_FAILED "\x1B[31mFAILED\x1B[0m"

static struct timeval start;
static struct timeval end;

static int status = TEST_OK;
static int count = 0;

const char * padding =
        ".............................."
        "..............................";

static void test_start(char * test_name)
{
    count = 0;
    int padlen = 60 - strlen(test_name);
    printf("Testing %s%*.*s", test_name, padlen, padlen, padding);
    gettimeofday(&start, 0);
}

static int test_end(void)
{
    gettimeofday(&end, 0);
    float t = (end.tv_sec - start.tv_sec) * 1000.0f +
            (end.tv_usec - start.tv_usec) / 1000.0f;

    printf("%s (%.3f ms)\n",
            (status == TEST_OK) ? TEST_MSG_OK : TEST_MSG_FAILED,
                    t);

    return status;
}

#define _assert(e) (void)((e)?count++:(status = TEST_FAILED) && printf("\n\x1B[33mAssertion failed (%s:%d):\x1B[0m %s\n\n", __FILE__, __LINE__, #e))

#endif  /* SIRIDB_TEST_H_ */