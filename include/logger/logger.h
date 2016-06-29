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
const char * logger_level_name(int log_level);

void log__debug(char * fmt, ...);
void log__info(char * fmt, ...);
void log__warning(char * fmt, ...);
void log__error(char * fmt, ...);
void log__critical(char * fmt, ...);

extern logger_t Logger;

#define log_debug(fmt, ...)                 \
    if (Logger.level <= LOGGER_DEBUG)       \
        log__debug(fmt, ##__VA_ARGS__)      \

#define log_info(fmt, ...)                  \
    if (Logger.level <= LOGGER_INFO)        \
        log__info(fmt, ##__VA_ARGS__)       \

#define log_warning(fmt, ...)               \
    if (Logger.level <= LOGGER_WARNING)     \
        log__warning(fmt, ##__VA_ARGS__)    \

#define log_error(fmt, ...)                 \
    if (Logger.level <= LOGGER_ERROR)       \
        log__error(fmt, ##__VA_ARGS__)      \

#define log_critical(fmt, ...)              \
    if (Logger.level <= LOGGER_CRITICAL)    \
        log__critical(fmt, ##__VA_ARGS__)   \

#define LOGC(fmt, ...) \
    fprintf(Logger.ostream, "%s:%d ", __FILE__, __LINE__); \
    log_critical(fmt, ##__VA_ARGS__)

