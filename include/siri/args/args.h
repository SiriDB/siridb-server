#pragma once

#include <inttypes.h>
#include <argparse/argparse.h>
#include <siri/siri.h>

typedef struct siri_s siri_t;

typedef struct siri_args_s
{
    /* true/false props */
    int32_t version;
    int32_t log_colorized;

    /* string props */
    char config[ARGPARSE_MAX_LEN_ARG];
    char log_level[ARGPARSE_MAX_LEN_ARG];
} siri_args_t;

/* arguments are configured and parsed here */
void siri_args_parse(siri_t * siri, int argc, char *argv[]);
