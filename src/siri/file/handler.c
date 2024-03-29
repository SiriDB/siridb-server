/*
 * handler.c - File handler for shard files.
 */
#include <logger/logger.h>
#include <siri/err.h>
#include <siri/file/handler.h>
#include <stdlib.h>

siri_fh_t * siri_fh_new(uint16_t size)
{
    siri_fh_t * fh = malloc(sizeof(siri_fh_t));
    if (fh == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        fh->size = size;
        fh->idx = 0;
        fh->fpointers = calloc(size, sizeof(siri_fp_t *));
        if (fh->fpointers == NULL)
        {
            ERR_ALLOC
            free(fh);
            fh = NULL;
        }
        uv_mutex_init(&fh->lock_);
    }
    return fh;
}

/*
 * Close file handler. (closes all open files in the file handler)
 * In case closing an open file fails, a SIGNAL will be raised.
 */
void siri_fh_close(siri_fh_t * fh)
{
    siri_fp_t ** fp;
    uint16_t i;

    if (fh == NULL)
    {
        return;
    }

    for (i = 0; i < fh->size; i++)
    {
        fp = fh->fpointers + i;

        if (*fp == NULL)
        {
            break;
        }
        siri_fp_decref(*fp);
    }
}

/*
 * Destroy file handler. (closes all open files in the file handler)
 * In case closing an open file fails, a SIGNAL will be raised.
 */
void siri_fh_free(siri_fh_t * fh)
{
    free(fh->fpointers);
    uv_mutex_destroy(&fh->lock_);
    free(fh);
}


/*
 * Returns 0 if successful or -1 in case of an error.
 */
int siri_fopen(
        siri_fh_t * fh,
        siri_fp_t * fp,
        const char * fn,
        const char * modes)
{
    siri_fp_t ** dest;
    uv_mutex_lock(&fh->lock_);
    dest = fh->fpointers + fh->idx;

    /* close and possible free file pointer at next position */
    if (*dest != NULL)
    {
        siri_fp_decref(*dest);
    }

    /* assign file pointer */
    *dest = fp;

    /* increment reference counter (must be done even if open fails) */
    fp->ref++;

    if ((fp->fp = fopen(fn, modes)) == NULL)
    {
        log_critical("Cannot open file: '%s' using mode '%s'", fn, modes);
        uv_mutex_unlock(&fh->lock_);
        return -1;
    }

    /* set file handler pointer to next position */
    fh->idx = (fh->idx + 1) % fh->size;
    uv_mutex_unlock(&fh->lock_);
    return 0;
}


