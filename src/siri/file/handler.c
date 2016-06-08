/*
 * filehandler.c - Filehandler for shard files.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-04-2016
 *
 */
#include <siri/file/handler.h>
#include <stdlib.h>
#include <logger/logger.h>


siri_fh_t * siri_fh_new(uint16_t size)
{
    siri_fh_t * fh = (siri_fh_t *) malloc(sizeof(siri_fh_t));
    fh->size = size;
    fh->idx = 0;
    fh->fpointers = (siri_fp_t **) calloc(size, sizeof(siri_fp_t *));
    return fh;
}

void siri_fh_free(siri_fh_t * fh)
{
    if (fh == NULL)
        return;

    siri_fp_t ** fp;
    for (uint16_t i = 0; i < fh->size; i++)
    {
        fp = fh->fpointers + i;

        if (*fp == NULL)
            break;
        log_debug("Decref file handler as pos %d", i);
        siri_fp_decref(*fp);
    }
    free(fh->fpointers);
    free(fh);
}

int siri_fopen(
        siri_fh_t * fh,
        siri_fp_t * fp,
        const char * fn,
        const char * modes)
{
    siri_fp_t ** dest = fh->fpointers + fh->idx;

    /* close and possible free file pointer at next position */
    if (*dest)
        siri_fp_decref(*dest);

    /* assign file pointer */
    *dest = fp;

    /* increment reference counter (must be done even if open fails) */
    fp->ref++;

    if ((fp->fp = fopen(fn, modes)) == NULL)
    {
        log_critical("Cannot open file: '%s' using mode '%s'", fn, modes);
        return -1;
    }

    /* set file handler pointer to next position */
    fh->idx = (fh->idx + 1) % fh->size;

    return 0;
}


