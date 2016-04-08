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
#include <imap64/imap64.h>

typedef struct siri_fh_s
{
    uint16_t size;
    uint16_t pt;
    imap64_t fpointers;
    uint64_t * shard_ids;
} siri_fh_t;

siri_fh_t * siri_new_fh(uint16_t size);
void siri_free_fh(siri_fh_t * fh);

