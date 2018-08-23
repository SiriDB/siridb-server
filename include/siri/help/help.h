/*
 * help.h - Help for SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 23-09-2016
 *
 */
#ifndef SIRI_HELP_H_
#define SIRI_HELP_H_

#include <siri/grammar/gramp.h>

const char * siri_help_get(
        uint16_t gid,
        const char * help_name,
        char * err_msg);

void siri_help_free(void);

#endif  /* SIRI_HELP_H_ */
