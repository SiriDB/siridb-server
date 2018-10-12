/*
 * main.c - SiriDB.
 *
 * author/maintainer : Jeroen van der Heijden <jeroen@transceptor.technology>
 * contributors      : https://github.com/SiriDB/siridb-server/contributors
 * home page         : https://siridb.net
 * copyright         : 2018, Transceptor Technology
 *
 */
#include <locale.h>
#include <logger/logger.h>
#include <siri/args/args.h>
#include <siri/db/presuf.h>
#include <siri/err.h>
#include <siri/help/help.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int main(int argc, char * argv[])
{
    /* set local to LC_ALL */
    (void) setlocale(LC_ALL, "");

    /* initialize random */
    srand(time(NULL));

    /* set threadpool size to 8 (default=4) */
    putenv("UV_THREADPOOL_SIZE=8");

    /* set default timezone to UTC */
    putenv("TZ=:UTC");
    tzset();

    /* parse arguments first since this can exit the program */
    siri_args_parse(&siri, argc, argv);

    /* setup logger, this must be done before logging the first line */
    siri_setup_logger();

    /* initialize points dictionary */
    siridb_points_init();

    /* start server */
    log_info("Starting SiriDB Server (version: %s)", SIRIDB_VERSION);

    /* initialize SiriDB mutex (used for the siridb_list) */
    if (uv_mutex_init(&siri.siridb_mutex))
    {
        exit(1);
    }

    /* read siridb main application configuration */
    siri_cfg_init(&siri);

    /* start SiriDB. (this start the event loop etc.) */
    if (siri_start() && !siri_err)
    {
        siri_err = ERR_STARTUP;
    }

    /* free siridb */
    siri_free();

    /* destroy SiriDB mutex */
    uv_mutex_destroy(&siri.siridb_mutex);

    /* cleanup prefix-suffix allocation */
    siridb_presuf_cleanup();

    /* cleanup help */
    siri_help_free();

    log_info("Bye! (%d)\n", siri_err);

    return siri_err;
}
