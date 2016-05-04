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

#include <inttypes.h>
#include <siri/filepointer.h>

typedef struct siri_fh_s
{
    uint16_t size;
    uint16_t idx;
    siri_fp_t ** fpointers;
} siri_fh_t;

siri_fh_t * siri_fh_new(uint16_t size);

void siri_fh_free(siri_fh_t * fh);

int siri_fopen(
        siri_fh_t * fh,
        siri_fp_t * fp,
        const char * fn,
        const char * modes);

