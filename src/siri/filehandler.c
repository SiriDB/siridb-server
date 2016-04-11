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
#include <siri/filehandler.h>
#include <stdlib.h>
#include <logger/logger.h>


siri_fh_t * siri_new_fh(uint16_t size)
{
    siri_fh_t * fh = (siri_fh_t *) malloc(sizeof(siri_fh_t));
    fh->size = size;
    fh->idx = 0;
    fh->fpointers = (siri_fp_t **) calloc(size, sizeof(siri_fp_t *));
    return fh;
}

void siri_free_fh(siri_fh_t * fh)
{
    if (fh == NULL)
        return;

    siri_fp_t ** fp;
    for (uint16_t i = 0; i < fh->size; i++)
    {
        fp = fh->fpointers + i;

        if (*fp == NULL)
            break;
        siri_decref_fp(*fp);
    }
    free(fh->fpointers);
    free(fh);
}

siri_fp_t * siri_new_fp(void)
{
    siri_fp_t * fp = (siri_fp_t *) malloc(sizeof(siri_fp_t));
    fp->fp = NULL;
    fp->ref = 1;
    return fp;
}

void siri_decref_fp(siri_fp_t * fp)
{
    if (fp->fp != NULL)
    {
        fclose(fp->fp);
        fp->fp = NULL;
    }
    if (!--fp->ref)
        free(fp);
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
        siri_decref_fp(*dest);

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

