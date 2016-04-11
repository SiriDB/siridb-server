/*
 * filehandler.h - Filehandler for shard files.
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

typedef struct siri_fh_s
{
    uint16_t size;
    uint16_t idx;
    siri_fp_t ** fpointers;
} siri_fh_t;

siri_fh_t * siri_new_fh(uint16_t size);
void siri_free_fh(siri_fh_t * fh);

siri_fp_t * siri_new_fp(void);
/* closes the file pointer, decrement reference counter and free if needed */
void siri_decref_fp(siri_fp_t * fp);

int siri_fopen(
        siri_fh_t * fh,
        siri_fp_t * fp,
        const char * fn,
        const char * modes);

