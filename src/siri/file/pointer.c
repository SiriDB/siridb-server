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

#include <siri/file/pointer.h>
#include <stdlib.h>
#include <logger/logger.h>

#define FCLOSE      \
    fclose(fp->fp); \
    fp->fp = NULL;

siri_fp_t * siri_fp_new(void)
{
    siri_fp_t * fp = (siri_fp_t *) malloc(sizeof(siri_fp_t));
    fp->fp = NULL;
    fp->ref = 1;
    return fp;
}

void siri_fp_decref(siri_fp_t * fp)
{
    if (fp->fp != NULL)
    {
        FCLOSE
    }
    if (!--fp->ref)
    {
        log_debug("Free File Pointer");
        free(fp);
    }

}

void siri_fp_close(siri_fp_t * fp)
{
    FCLOSE
}
