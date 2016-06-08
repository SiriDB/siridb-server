/*
 * filepointer.h - File-pointer used in combination with file-handler.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-04-2016
 *
 */
#pragma once

#include <stdio.h>
#include <inttypes.h>


typedef struct siri_fp_s
{
    FILE * fp;
    uint8_t ref;
} siri_fp_t;


siri_fp_t * siri_fp_new(void);
/* closes the file pointer, decrement reference counter and free if needed */
void siri_fp_decref(siri_fp_t * fp);
void siri_fp_close(siri_fp_t * fp);
