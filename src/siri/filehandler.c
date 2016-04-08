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
#include <siri/siri.h>
#include <imap64/imap64.h>

siri_fh_t * siri_new_fh(uint16_t size)
{
    siri_fh_t * fh = (siri_fh_t *) malloc(sizeof(siri_fh_t));
    fh->size = size;
    fh->len = 0;
    fh->pt = 0;
    fh->fpointers = (FILE **) calloc(size, sizeof(FILE *));
    return fh;
}

void siri_free_fh(siri_fh_t * fh)
{
    free(fh->fpointers);
    free(fh);
}

int siri_fopen(FILE ** fp)
{
    if ((siri.fh->fpointers + siri.fh->pt) == NULL)
    {

    }
    return 0;
}
