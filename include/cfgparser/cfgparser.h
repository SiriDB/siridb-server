/*
 * cfgparser.h - module for reading (and later writing) to INI style files.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#pragma once

#include <inttypes.h>

typedef enum {
    CFGPARSER_SUCCESS,
    CFGPARSER_ERR_READING_FILE,
    CFGPARSER_ERR_SESSION_NOT_OPEN,
    CFGPARSER_ERR_MISSING_EQUAL_SIGN,
    CFGPARSER_ERR_OPTION_ALREADY_DEFINED,
    CFGPARSER_ERR_SECTION_NOT_FOUND,
    CFGPARSER_ERR_OPTION_NOT_FOUND
} cfgparser_return_t;


typedef enum {
    CFGPARSER_TP_INTEGER,
    CFGPARSER_TP_STRING,
    CFGPARSER_TP_REAL,
} cfgparser_tp_t;

typedef union
{
    int32_t integer;
    double real;
    char * string;
} cfgparser_u;

typedef struct cfgparser_option_s
{
    char * name;
    cfgparser_tp_t tp;
    cfgparser_u * val;
    cfgparser_u * def;
    struct cfgparser_option_s * next;
} cfgparser_option_t;

typedef struct cfgparser_section_s
{
    char * name;
    cfgparser_option_t * options;
    struct cfgparser_section_s * next;
} cfgparser_section_t;

typedef struct cfgparser_s
{
    cfgparser_section_t * sections;
} cfgparser_t;


cfgparser_return_t cfgparser_read(cfgparser_t * cfgparser, const char * fn);

cfgparser_t * cfgparser_new(void);

void cfgparser_free(cfgparser_t * cfgparser);

cfgparser_section_t * cfgparser_section(
        cfgparser_t * cfgparser,
        const char * name);

cfgparser_option_t * cfgparser_string_option(
        cfgparser_section_t * section,
        const char * name,
        const char * val,
        const char * def);

cfgparser_option_t * cfgparser_integer_option(
        cfgparser_section_t * section,
        const char * name,
        int32_t val,
        int32_t def);

cfgparser_option_t * cfgparser_real_option(
        cfgparser_section_t * section,
        const char * name,
        double val,
        double def);

const char * cfgparser_errmsg(cfgparser_return_t err);

cfgparser_return_t cfgparser_get_section(
        cfgparser_section_t ** section,
        cfgparser_t * cfgparser,
        const char * section_name);

cfgparser_return_t cfgparser_get_option(
        cfgparser_option_t ** option,
        cfgparser_t * cfgparser,
        const char * section_name,
        const char * option_name);

