/*
 * argparse.h - Module for parsing command line arguments.
 */
#ifndef ARGPARSE_H_
#define ARGPARSE_H_

#define ARGPARSE_SUCCESS 0
#define ARGPARSE_ERR_MISSING_VALUE -1
#define ARGPARSE_ERR_UNRECOGNIZED_ARGUMENT -2
#define ARGPARSE_ERR_AMBIGUOUS_OPTION -3
#define ARGPARSE_ERR_INVALID_CHOICE -4
#define ARGPARSE_ERR_ARGUMENT_TOO_LONG -5
#define ARGPARSE_ERR_UNHANDLED -9

#define ARGPARSE_MAX_LEN_ARG 255

typedef enum {
    ARGPARSE_STORE_TRUE,
    ARGPARSE_STORE_FALSE,
    ARGPARSE_STORE_STRING,
    ARGPARSE_STORE_INT,
    ARGPARSE_STORE_STR_CHOICE
} argparse_action_t;

typedef struct argparse_args_s argparse_args_t;
typedef struct argparse_parser_s argparse_parser_t;
typedef struct argparse_argument_s argparse_argument_t;

#include <inttypes.h>

void argparse_init(argparse_parser_t * parser);
void argparse_free(argparse_parser_t * parser);
void argparse_add_argument(
        argparse_parser_t * parser,
        argparse_argument_t * argument);
void argparse_parse(argparse_parser_t *parser, int argc, char *argv[]);

struct argparse_argument_s
{
    char * name;
    char shortcut;
    char * help;
    argparse_action_t action;
    int32_t default_int32_t;
    int32_t * pt_value_int32_t;
    char * str_default;
    char * str_value;
    char * choices; /* choices must be separated by commas */
};

struct argparse_args_s
{
    argparse_argument_t * argument;
    argparse_args_t * next;
};

struct argparse_parser_s
{
    argparse_args_t * args;
    int32_t show_help;
    argparse_argument_t help;
};

#endif  /* ARGPARSE_H_ */
