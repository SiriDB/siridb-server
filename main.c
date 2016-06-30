/*
 * main.c - SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <logger/logger.h>
#include <siri/args/args.h>
#include <siri/err.h>
#include <siri/siri.h>
#include <siri/version.h>

#ifdef DEBUG
#include <test/test.h>
#endif

int main(int argc, char * argv[])
{
    int rc;

    #ifdef DEBUG
    printf( "*******************************\n"
            "*     Start DEBUG Release     *\n"
            "*******************************\n");
    #endif

    /*
     * set local to LC_ALL
     * more info at: http://www.cprogramming.com/tutorial/unicode.html
     */
    setlocale(LC_ALL, "");

    /* initialize random */
    srand(time(NULL));

    /* set default timezone to UTC */
    putenv("TZ=:UTC");
    tzset();

    /* parse arguments first since this can exit the program */
    siri_args_parse(&siri, argc, argv);

    /* setup logger, this must be done before logging the first line */
    siri_setup_logger();

    #ifdef DEBUG
    /* run tests when we are in debug mode */
    rc = run_tests(0);
    if (rc)
        exit(1);
    #endif

    /* start server */
    log_info("Starting SiriDB Server (version: %s)", SIRIDB_VERSION);

    /* initialize SiriDB mutex (used for the siridb_list) */
    uv_mutex_init(&siri.siridb_mutex);

    /* read siridb main application configuration */
    siri_cfg_init(&siri);

    /* start SiriDB. (this start the event loop etc.) */
    rc = siri_start();

    /* free siridb */
    siri_free();

    /* destroy SiriDB mutex */
    uv_mutex_destroy(&siri.siridb_mutex);

    log_info("Bye!\n");

    return rc;
}
