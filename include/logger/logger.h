/*
 * logger.h - log module
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

#define LOGGER_DEBUG 10
#define LOGGER_INFO 20
#define LOGGER_WARNING 30
#define LOGGER_ERROR 40
#define LOGGER_CRITICAL 50

#define LOGGER_NUM_LEVELS 5

#define LOGGER_FLAG_COLORED 1

typedef struct logger_s
{
    struct _IO_FILE * ostream;
    int level;
    const char * level_name;
    int flags;
} logger_t;

const char * LOGGER_LEVEL_NAMES[LOGGER_NUM_LEVELS];

void logger_init(struct _IO_FILE * ostream, int log_level);
void logger_set_level(int log_level);
void log_debug(char * fmt, ...);
void log_info(char * fmt, ...);
void log_warning(char * fmt, ...);
void log_error(char * fmt, ...);
void log_critical(char * fmt, ...);

extern logger_t Logger;
