/*
 * error.h - SiriDB Error.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 30-06-2016
 *
 */
#pragma once

#include <logger/logger.h>
#include <signal.h>

/* value should be 0, any other value indicates a critical error has occurred */
extern int siri_err;


#ifdef DEBUG
#define ERR_ALLOC                                           \
log_critical("Memory allocation error at: %s:%d (%s)",      \
        __FILE__, __LINE__, __func__);                      \
raise(SIGSEGV);                                             \
siri_err = SIGSEGV;
#define ERR_FILE                                            \
log_critical("Critical file error at: %s:%d (%s)",          \
        __FILE__, __LINE__, __func__);                      \
raise(SIGABRT);                                             \
siri_err = SIGABRT;
#define ERR_C                                       \
log_critical("Critical error at: %s:%d (%s)",       \
        __FILE__, __LINE__, __func__);              \
raise(SIGABRT);                                     \
siri_err = SIGABRT;
#else
#define ERR_ALLOC                                   \
log_critical("Memory allocation error occurred");   \
raise(SIGSEGV);                                     \
siri_err = SIGSEGV;
#define ERR_FILE                                                            \
log_critical("Critical file error occurred (possible the disk is full)");   \
raise(SIGABRT);                                                             \
siri_err = SIGABRT;
#define ERR_C                               \
raise(SIGABRT);                             \
siri_err = SIGABRT;
#endif


