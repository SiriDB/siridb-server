#pragma once

#include <inttypes.h>
#include <argparse/argparse.h>

typedef struct siri_args_s
{
    /* true/false props */
    int32_t version;
    int32_t debug;
    int32_t noroot;
    int32_t log_colorized;

    /* string props */
    char config[255];
    char log_level[255];
    char log_file_prefix[255];

    /* integer props */
    int32_t log_file_max_size;
    int32_t log_file_num_backups;

} siri_args_t;

/* arguments are configured and parsed here */
void siri_args_parse(int argc, char *argv[]);

/* here we keep the arguments result */
extern siri_args_t siri_args;
