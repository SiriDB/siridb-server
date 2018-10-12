/*
 * args.c - Parse SiriDB command line arguments.
 */
#include <siri/args/args.h>
#include <argparse/argparse.h>
#include <siri/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEFAULT_LOG_FILE_MAX_SIZE 50000000
#define DEFAULT_LOG_FILE_NUM_BACKUPS 6

#ifndef NDEBUG
#define DEFAULT_LOG_LEVEL "debug"
#else
#define DEFAULT_LOG_LEVEL "info"
#endif

static siri_args_t siri_args = {
        .version=0,
        .config="",
        .log_level="",
        .log_colorized=0,
};

void siri_args_parse(siri_t * siri, int argc, char *argv[])
{
    siri->args = &siri_args;

    argparse_parser_t parser;
    argparse_init(&parser);

    argparse_argument_t config = {
            "config",                                   /* name             */
            'c',                                        /* shortcut         */
            "define which SiriDB configuration file "   /* help             */
            "to use",
            ARGPARSE_STORE_STRING,                      /* action           */
            0,                                          /* default int32_t  */
            NULL,                                       /* value pt_int32_t */
            "/etc/siridb/siridb.conf",                  /* default string   */
            siri_args.config,                           /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t version = {
            "version",                                  /* name             */
            'v',                                        /* shortcut         */
            "show version information and exit",        /* help             */
            ARGPARSE_STORE_TRUE,                        /* action           */
            0,                                          /* default int32_t  */
            &siri_args.version,                         /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t log_level = {
            "log-level",                                /* name             */
            'l',                                        /* shortcut         */
            "set initial log level",                    /* help             */
            ARGPARSE_STORE_STR_CHOICE,                  /* action           */
            0,                                          /* default int32_t  */
            NULL,                                       /* value pt_int32_t */
            DEFAULT_LOG_LEVEL,                          /* default string   */
            siri_args.log_level,                        /* value string     */
            "debug,info,warning,error,critical"         /* choices          */
    };

    argparse_argument_t log_colorized = {
            "log-colorized",                            /* name             */
            0,                                          /* shortcut         */
            "use colorized logging",                    /* help             */
            ARGPARSE_STORE_TRUE,                        /* action           */
            0,                                          /* default int32_t  */
            &siri_args.log_colorized,                   /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
            NULL                                        /* choices          */
    };

    argparse_add_argument(&parser, &config);
    argparse_add_argument(&parser, &version);
    argparse_add_argument(&parser, &log_level);
    argparse_add_argument(&parser, &log_colorized);

    /* this will parse and free the parser from memory */
    argparse_parse(&parser, argc, argv);

    if (siri_args.version)
    {
        printf(
                "SiriDB Server %s\n"
                "Contributers: %s\n"
                "Home-page: %s\n",
                SIRIDB_VERSION,
                SIRIDB_CONTRIBUTERS,
                SIRIDB_HOME_PAGE);

        exit(EXIT_SUCCESS);
    }
}
