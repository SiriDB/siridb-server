/*
 * pointer.c - File pointer used in combination with file handler.
 */
#include <logger/logger.h>
#include <siri/err.h>
#include <siri/file/pointer.h>
#include <stdlib.h>

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siri_fp_t * siri_fp_new(void)
{
    siri_fp_t * fp = (siri_fp_t *) malloc(sizeof(siri_fp_t));
    if (fp == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        fp->fp = NULL;
        fp->ref = 1;
    }
    return fp;
}

/*
 * This function will always close the file when open but only frees the
 * object from memory when reference count 0 is reached.
 *
 * When an error occurs while closing the file, a SIGNAL is raised.
 */
void siri_fp_decref(siri_fp_t * fp)
{
    if (fp->fp != NULL)
    {
        if (fclose(fp->fp))
        {
            ERR_FILE
        }
        fp->fp = NULL;
    }
    if (!--fp->ref)
    {
        free(fp);
    }
}

/*
 * This will close a file and a SIGNAL is raised is an error occurs.
 */
void siri_fp_close(siri_fp_t * fp)
{
    if (fp->fp != NULL)
    {
        if (fclose(fp->fp))
        {
            ERR_FILE
        }
        fp->fp = NULL;
    }
}
