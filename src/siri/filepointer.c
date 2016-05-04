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

#include <siri/filepointer.h>
#include <stdlib.h>
#include <logger/logger.h>


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
        fclose(fp->fp);
        fp->fp = NULL;
    }
    if (!--fp->ref)
        free(fp);
}
