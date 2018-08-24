/*
 * args.h - SiriDB Arguments.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 */
#ifndef SIRI_ARGS_H_
#define SIRI_ARGS_H_

typedef struct siri_args_s siri_args_t;

#include <inttypes.h>
#include <argparse/argparse.h>
#include <siri/siri.h>

/* arguments are configured and parsed here */
void siri_args_parse(siri_t * siri, int argc, char *argv[]);

struct siri_args_s
{
    /* true/false props */
    int32_t version;
    int32_t log_colorized;

    /* string props */
    char config[ARGPARSE_MAX_LEN_ARG];
    char log_level[ARGPARSE_MAX_LEN_ARG];
};

#endif  /* SIRI_ARGS_H_ */
