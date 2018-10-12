/*
 * argparse.c - Module for parsing command line arguments.
 */
#include <argparse/argparse.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <xstr/xstr.h>
#include <string.h>
#include <assert.h>

#define HELP_WIDTH 80           /* try to fit help within this screen width */
#define ARGPARSE_ERR_SIZE 1024  /* buffer size for building err msg */
#define ARGPARSE_HELP_SIZE 255  /* buffer size for building help */

/* static function definitions */
static void print_usage(argparse_parser_t * parser, const char * bname);
static void print_help(argparse_parser_t * parser, const char * bname);
static void print_error(
        argparse_parser_t * parser,
        char * err_msg,
        const char * bname);
static int process_arg(
        argparse_parser_t * parser,
        argparse_args_t ** arg,
        int * argn,
        int argc,
        char *argv[]);
static uint16_t get_argument(
        argparse_args_t ** target,
        argparse_parser_t * parser,
        char * argument);
static bool match_choice(char * choices, char * choice);
static void fill_defaults(argparse_parser_t * parser);
static void quit(argparse_parser_t * parser, int rc);


void argparse_init(argparse_parser_t * parser)
{
    /* set initial show help to false */
    parser->show_help = false;

    /* create help argument */
    parser->help.name = "help";
    parser->help.shortcut = 'h';
    parser->help.help = "show this help message and exit";
    parser->help.action = ARGPARSE_STORE_TRUE;
    parser->help.pt_value_int32_t = &parser->show_help;

    /* add help argument to parser */
    parser->args = malloc(sizeof(argparse_args_t));
    parser->args->argument = &parser->help;
    parser->args->next = NULL;
}

void argparse_free(argparse_parser_t * parser)
{
    argparse_args_t * current = parser->args;
    argparse_args_t * next;

    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }
}

void argparse_add_argument(
        argparse_parser_t * parser,
        argparse_argument_t * argument)
{
    argparse_args_t * current = parser->args;

    assert(current != NULL);

    while (current->next != NULL)
        current = current->next;
    current->next = malloc(sizeof(argparse_args_t));
    current->next->argument = argument;
    current->next->next = NULL;
}

void argparse_parse(argparse_parser_t *parser, int argc, char *argv[])
{
    int argn;
    int rc;
    char buffer[ARGPARSE_ERR_SIZE];
    const char * bname = basename(argv[0]);
    argparse_args_t * current = NULL;
    for (argn = 1; argn < argc; argn++)
    {
        rc = process_arg(parser, &current, &argn, argc, argv);
        switch (rc)
        {
        case ARGPARSE_ERR_MISSING_VALUE:
            if  (current->argument->shortcut)
                snprintf(buffer,
                        ARGPARSE_ERR_SIZE,
                        "argument -%c/--%s: expected one argument",
                        current->argument->shortcut,
                        current->argument->name);
            else
                snprintf(buffer,
                        ARGPARSE_ERR_SIZE,
                        "argument --%s: expected one argument",
                        current->argument->name);
            break;
        case ARGPARSE_ERR_UNRECOGNIZED_ARGUMENT:
            snprintf(buffer,
                    ARGPARSE_ERR_SIZE,
                    "unrecognized argument: %s",
                    argv[argn]);
            break;
        case ARGPARSE_ERR_AMBIGUOUS_OPTION:
            snprintf(buffer,
                    ARGPARSE_ERR_SIZE,
                    "ambiguous option: %s",
                    argv[argn]);
            break;
        case ARGPARSE_ERR_INVALID_CHOICE:
            if  (current->argument->shortcut)
                snprintf(buffer,
                        ARGPARSE_ERR_SIZE,
                        "argument -%c/--%s: invalid choice: '%s'",
                        current->argument->shortcut,
                        current->argument->name,
                        argv[argn]);
            else
                snprintf(buffer,
                        ARGPARSE_ERR_SIZE,
                        "argument --%s: invalid choice: '%s'",
                        current->argument->name,
                        argv[argn]);
            break;
        case ARGPARSE_ERR_ARGUMENT_TOO_LONG:
            snprintf(buffer,
                    ARGPARSE_ERR_SIZE,
                    "argument value exceeds character limit: %s",
                    current->argument->name);
            break;
        default:
            *buffer = 0;
        }
        if (rc)
        {
            print_error(parser, buffer, bname);
            quit(parser, EXIT_FAILURE);
        }
    }

    /* when help is requested, print help and exit */
    if (parser->show_help)
    {
        print_help(parser, bname);
        quit(parser, EXIT_SUCCESS);
    }

    /* fill missing arguments with defaults */
    fill_defaults(parser);

    /* free parser */
    argparse_free(parser);
}

static void quit(argparse_parser_t * parser, int rc)
{
    argparse_free(parser);
    exit(rc);
}

static void fill_defaults(argparse_parser_t * parser)
{
    argparse_args_t * current;
    for (current = parser->args; current != NULL; current = current->next)
    {
        switch (current->argument->action)
        {
        case ARGPARSE_STORE_TRUE:
        case ARGPARSE_STORE_FALSE:
        case ARGPARSE_STORE_INT:
            if (current->argument->pt_value_int32_t == 0)
                *current->argument->pt_value_int32_t =
                        current->argument->default_int32_t;
            continue;
        case ARGPARSE_STORE_STRING:
        case ARGPARSE_STORE_STR_CHOICE:
            if (!strlen(current->argument->str_value))
                strcpy(
                    current->argument->str_value,
                    current->argument->str_default);
            continue;
        }
    }
}

static void print_error(
        argparse_parser_t * parser,
        char * err_msg,
        const char * bname)
{
    print_usage(parser, bname);
    printf("%s: error: %s\n", bname, err_msg);
}

static void print_usage(argparse_parser_t * parser, const char * bname)
{
    char buffer[ARGPARSE_HELP_SIZE];
    char uname[ARGPARSE_MAX_LEN_ARG];
    size_t line_size;
    size_t ident;
    argparse_args_t * current;

    snprintf(buffer, ARGPARSE_HELP_SIZE, "usage: %s ", bname);
    line_size = ident = strlen(buffer);
    printf("%s", buffer);
    for (current = parser->args; current != NULL; current = current->next)
    {
        *buffer = 0;
        switch (current->argument->action)
        {
        case ARGPARSE_STORE_TRUE:
        case ARGPARSE_STORE_FALSE:
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[-%c] ",
                        current->argument->shortcut);
            else
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[--%s] ",
                        current->argument->name);
            break;
        case ARGPARSE_STORE_STRING:
        case ARGPARSE_STORE_INT:
            strcpy(uname, current->argument->name);
            xstr_replace_char(uname, '-', '_');
            xstr_upper_case(uname);
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[-%c %s] ",
                        current->argument->shortcut,
                        uname);
            else
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[--%s %s] ",
                        current->argument->name,
                        uname);
            break;
        case ARGPARSE_STORE_STR_CHOICE:
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[-%c {%s}] ",
                        current->argument->shortcut,
                        current->argument->choices);
            else
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        "[--%s {%s}] ",
                        current->argument->name,
                        current->argument->choices);
            break;
        }

        line_size += strlen(buffer);
        if (line_size > HELP_WIDTH)
        {
            printf("\n%*c%s", (int) ident, ' ', buffer);
            line_size = ident + strlen(buffer);
        }
        else
            printf("%s", buffer);
    }
    printf("\n");
}

static void print_help(argparse_parser_t * parser, const char * bname)
{
    char buffer[ARGPARSE_HELP_SIZE];
    char uname[ARGPARSE_MAX_LEN_ARG];
    argparse_args_t * current;
    size_t line_size;

    print_usage(parser, bname);
    printf("\noptional arguments:\n");

    current = parser->args;
    for (;current != NULL; current = current->next)
    {
        *buffer = 0;
        switch (current->argument->action)
        {
        case ARGPARSE_STORE_TRUE:
        case ARGPARSE_STORE_FALSE:
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        " -%c,",
                        current->argument->shortcut);
            snprintf(buffer, ARGPARSE_HELP_SIZE,
                    "%s --%s",
                    buffer,
                    current->argument->name);
            break;
        case ARGPARSE_STORE_STRING:
        case ARGPARSE_STORE_INT:
            strcpy(uname, current->argument->name);
            xstr_replace_char(uname, '-', '_');
            xstr_upper_case(uname);
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        " -%c %s,",
                        current->argument->shortcut,
                        uname);
            snprintf(buffer, ARGPARSE_HELP_SIZE,
                    "%s --%s %s",
                    buffer,
                    current->argument->name,
                    uname);
            break;
        case ARGPARSE_STORE_STR_CHOICE:
            if (current->argument->shortcut)
                snprintf(buffer, ARGPARSE_HELP_SIZE,
                        " -%c {%s},",
                        current->argument->shortcut,
                        current->argument->choices);
            snprintf(buffer, ARGPARSE_HELP_SIZE,
                    "%s --%s {%s}",
                    buffer,
                    current->argument->name,
                    current->argument->choices);
            break;
        }
        line_size = strlen(buffer);
        if (line_size > 24)
            printf("%s\n%*c", buffer, 24, ' ');
        else
            printf("%-*s", 24, buffer);

        printf("%s\n", current->argument->help);
    }
}

static uint16_t get_argument(
        argparse_args_t ** target,
        argparse_parser_t * parser,
        char * argument)
{
    char buffer[ARGPARSE_MAX_LEN_ARG];
    uint16_t nfound = 0;
    argparse_args_t * current;
    for (current = parser->args; current != NULL; current = current->next)
    {
        if (current->argument->shortcut)
        {
            sprintf(buffer, "-%c", current->argument->shortcut);
            if (strncmp(argument, buffer, strlen(argument)) == 0)
            {
                nfound++;
                *target = current;
                continue;
            }
        }
        sprintf(buffer, "--%s", current->argument->name);
        if (strncmp(argument, buffer, strlen(argument)) == 0)
        {
            nfound++;
            *target = current;
        }
    }
    return nfound;
}

static int process_arg(
        argparse_parser_t * parser,
        argparse_args_t ** arg,
        int * argn,
        int argc,
        char * argv[])
{
    char buffer[ARGPARSE_MAX_LEN_ARG];
    uint16_t num_matches;

    /* get arg and get the number of matches. (should be exactly one match) */
    num_matches = get_argument(&(*arg), parser, argv[*argn]);

    /* check if we have exactly one match */
    if (num_matches == 0)
        return ARGPARSE_ERR_UNRECOGNIZED_ARGUMENT;
    if (num_matches > 1)
        return ARGPARSE_ERR_AMBIGUOUS_OPTION;

    /* store true/false are not expecting more, they can be handled here */
    if (    (*arg)->argument->action == ARGPARSE_STORE_TRUE ||
            (*arg)->argument->action == ARGPARSE_STORE_FALSE)
    {
        *(*arg)->argument->pt_value_int32_t =
                (*arg)->argument->action == ARGPARSE_STORE_TRUE;
        return ARGPARSE_SUCCESS;
    }

    /* we expect a value an this value should not start with - */
    if (++(*argn) == argc || *argv[*argn] == '-')
        return ARGPARSE_ERR_MISSING_VALUE;

    if (strlen(argv[*argn]) >= ARGPARSE_MAX_LEN_ARG)
    {
        return ARGPARSE_ERR_ARGUMENT_TOO_LONG;
    }

    /* create a copy from the value into a buffer */
    strcpy(buffer, argv[*argn]);

    /* handle action */
    switch ((*arg)->argument->action)
    {
    case ARGPARSE_STORE_STRING:
        strcpy((*arg)->argument->str_value, buffer);
        return ARGPARSE_SUCCESS;
    case ARGPARSE_STORE_INT:
        *(*arg)->argument->pt_value_int32_t = (int32_t) atoi(buffer);
        return ARGPARSE_SUCCESS;
    case ARGPARSE_STORE_STR_CHOICE:
        if (match_choice((*arg)->argument->choices, buffer))
        {
            strcpy((*arg)->argument->str_value, buffer);
            return ARGPARSE_SUCCESS;
        }
        return ARGPARSE_ERR_INVALID_CHOICE;
    default:
        return ARGPARSE_ERR_UNHANDLED;
    }

}

static bool match_choice(char * choices, char * choice)
{
    size_t len_rest = strlen(choices);
    size_t len_choice = strlen(choice);
    bool check = true;
    char * walk = choices;

    for (; walk; len_rest--, walk++)
    {
        /* quit if not enough left to match choice */
        if (len_rest < len_choice)
            return false;

        /* when we should check, perform the check */
        if (    check &&
                strncmp(walk, choice, len_choice) == 0 &&
                (len_rest == len_choice || walk[len_choice] == ','))
            return true;

        /* set check true again when a comma is found */
        check = *walk == ',';
    }
    return false;
}
