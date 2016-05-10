#include <siri/args/args.h>
#include <argparse/argparse.h>
#include <siri/version.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEFAULT_LOG_FILE_MAX_SIZE 50000000
#define DEFAULT_LOG_FILE_NUM_BACKUPS 6
#define DEFAULT_LOG_LEVEL "debug"

static siri_args_t siri_args = {
        .version=0,
        .debug=0,
        .noroot=0,
        .config="",
        .log_level="",
        .log_file_prefix="",
        .log_colorized=0,
        .log_file_max_size=0,
        .log_file_num_backups=0,
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
#ifdef DEBUG
            "/home/joente/workspace/siridbc/testdir/siridb.conf",
#else
            "/etc/siridb/siridb.conf",                  /* default string   */
#endif
            siri_args.config,                           /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t debug = {
            "debug",                                    /* name             */
            'd',                                        /* shortcut         */
        "start SiriDB in debug mode",                   /* help             */
            ARGPARSE_STORE_TRUE,                        /* action           */
            0,                                          /* default int32_t  */
            &siri_args.debug,                           /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t noroot = {
            "noroot",                                   /* name             */
            'n',                                        /* shortcut         */
            "allow running SiriDB as a user without "   /* help             */
            "root access",
            ARGPARSE_STORE_TRUE,                        /* action           */
            0,                                          /* default int32_t  */
            &siri_args.noroot,                          /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
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
            "set the log level",                        /* help             */
            ARGPARSE_STORE_STR_CHOICE,                  /* action           */
            0,                                          /* default int32_t  */
            NULL,                                       /* value pt_int32_t */
            DEFAULT_LOG_LEVEL,                          /* default string   */
            siri_args.log_level,                        /* value string     */
            "debug,info,warning,error"                  /* choices          */
    };

    argparse_argument_t log_file_max_size = {
            "log-file-max-size",                        /* name             */
            0,                                          /* shortcut         */
            "max size of log files before rollover "    /* help             */
            "(--log-file-prefix must be set)",
            ARGPARSE_STORE_INT,                         /* action           */
            DEFAULT_LOG_FILE_MAX_SIZE,                  /* default int32_t  */
            &siri_args.log_file_max_size,               /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t log_file_num_backups = {
            "log-file-num-backups",                     /* name             */
            0,                                          /* shortcut         */
            "number of log files to keep "     /* help             */
            "(--log-file-prefix must be set)",
            ARGPARSE_STORE_INT,                         /* action           */
            DEFAULT_LOG_FILE_NUM_BACKUPS,               /* default int32_t  */
            &siri_args.log_file_num_backups,            /* value pt_int32_t */
            NULL,                                       /* default string   */
            NULL,                                       /* value string     */
            NULL                                        /* choices          */
    };

    argparse_argument_t log_file_prefix = {
            "log-file-prefix",                          /* name             */
            0,                                          /* shortcut         */
            "path prefix for log files "                /* help             */
            "(when not output to the console)",
            ARGPARSE_STORE_STRING,                      /* action           */
            0,                                          /* default int32_t  */
            NULL,                                       /* value pt_int32_t */
            "",                                         /* default string   */
            siri_args.log_file_prefix,                  /* value string     */
            NULL                                        /* choices          */
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
    argparse_add_argument(&parser, &debug);
    argparse_add_argument(&parser, &noroot);
    argparse_add_argument(&parser, &version);
    argparse_add_argument(&parser, &log_level);
    argparse_add_argument(&parser, &log_file_max_size);
    argparse_add_argument(&parser, &log_file_num_backups);
    argparse_add_argument(&parser, &log_file_prefix);
    argparse_add_argument(&parser, &log_colorized);

    /* this will parse and free the parser from memory */
    argparse_parse(&parser, argc, argv);

    if (siri_args.version)
    {
        printf(
                "SiriDB Server %s\n"
                "Maintainer: %s\n"
                "Home-page: %s\n",
                SDB_VERSION,
                SDB_MAINTAINER,
                SDB_HOME_PAGE);
        exit(EXIT_SUCCESS);
    }
}
