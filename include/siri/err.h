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

#define ERR_CLOSE_ENFORCED -3
#define ERR_CLOSE_TIMEOUT_REACHED -2
#define ERR_STARTUP -1

#ifdef DEBUG
#define ERR_ALLOC                                           \
log_critical("Memory allocation error at: %s:%d (%s)",      \
        __FILE__, __LINE__, __func__);                      \
raise(SIGSEGV);                                             \
if (!siri_err) siri_err = SIGSEGV;
#define ERR_FILE                                            \
log_critical("Critical file error at: %s:%d (%s)",          \
        __FILE__, __LINE__, __func__);                      \
raise(SIGABRT);                                             \
if (!siri_err) siri_err = SIGABRT;
#define ERR_C                                       \
log_critical("Critical error at: %s:%d (%s)",       \
        __FILE__, __LINE__, __func__);              \
raise(SIGABRT);                                     \
if (!siri_err) siri_err = SIGABRT;
#else
#define ERR_ALLOC                                   \
log_critical("Memory allocation error occurred");   \
raise(SIGSEGV);                                     \
if (!siri_err) siri_err = SIGSEGV;
#define ERR_FILE                                                            \
log_critical("Critical file error occurred (possibly the disk is full?)");  \
raise(SIGABRT);                                                             \
if (!siri_err) siri_err = SIGABRT;
#define ERR_C                               \
raise(SIGABRT);                             \
if (!siri_err) siri_err = SIGABRT;
#endif


