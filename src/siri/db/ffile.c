/*
 * ffile.c - FIFO file.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 30-06-2016
 *
 */
#include <siri/db/ffile.h>
#include <unistd.h>
#include <stdlib.h>
#include <logger/logger.h>
#include <assert.h>

#define FFILE_DEFAULT_SIZE 104857600  // 100 MB

siridb_ffile_t * siridb_ffile_new(
        uint64_t id,
        char * fn,
        sirinet_pkg_t * pkg)
{
    siridb_ffile_t * ffile = (siridb_ffile_t *) malloc(sizeof(siridb_ffile_t));

    ffile->id = id;
    ffile->fn = fn;

    if (access(fn, R_OK) == -1)
    {
        ffile->fp = fopen(fn, "w+");
        ffile->next_size = 0;
        if (pkg == NULL)
        {
            ffile->free_space = FFILE_DEFAULT_SIZE;
        }
        else
        {
            size_t size = pkg->len + PKG_HEADER_SIZE + 2*sizeof(uint32_t);
            ffile->free_space = (size > FFILE_DEFAULT_SIZE) ?
                    size : FFILE_DEFAULT_SIZE;
            siridb_ffile_append(ffile, pkg);
        }
    }
    else
    {
        ffile->fp = fopen(fn, "r+");
        fseek(ffile->fp, 0, SEEK_END);

        if (ftell(ffile->fp) >= sizeof(uint32_t))
        {
            ffile->free_space = 0;
            fseek(ffile->fp, -sizeof(uint32_t), SEEK_END);
            fread(&ffile->next_size, sizeof(uint32_t), 1, ffile->fp);
            fclose(ffile->fp);
            ffile->fp = NULL;
        }
        else
        {
            log_warning("Empty fifo found, we will remove file: '%s'", fn);
            siridb_ffile_unlink(ffile);
            ffile = NULL;
        }
    }
    return ffile;
}

siridb_ffile_result_t siridb_ffile_append(
        siridb_ffile_t * ffile,
        sirinet_pkg_t * pkg)
{
#ifdef DEBUG
    assert (ffile->fp != NULL);
#endif

    uint32_t size = pkg->len + PKG_HEADER_SIZE;

    if (ffile->free_space < size + 2 * sizeof(uint32_t))
    {
        ffile->free_space = 0;
        return FFILE_NO_FREE_SPACE;
    }

    if (!ffile->next_size)
    {
        ffile->next_size = size;
    }
    ffile->free_space -= size + sizeof(uint32_t);

    if (    fseek(ffile->fp, ffile->free_space, SEEK_SET) ||
            fwrite((unsigned char *) pkg, size, 1, ffile->fp) != 1 ||
            fwrite(&size, sizeof(uint32_t), 1, ffile->fp) ||
            fflush(ffile->fp))
    {
        return FFILE_ERROR;
    }

    return FFILE_SUCCESS;
}

sirinet_pkg_t * siridb_ffile_pop(siridb_ffile_t * ffile)
{
#ifdef DEBUG
    assert (ffile->next_size && ffile->fp != NULL);
    assert (!ffile->pop_commit);
    ffile->pop_commit = 1;
#endif

    if (fseek(ffile->fp, -ffile->next_size - sizeof(uint32_t), SEEK_END))
    {
        return NULL;
    }

    unsigned char * data = (unsigned char *) malloc(sizeof(ffile->next_size));

    if (data == NULL)
    {
        return NULL;
    }

    if (fread(data, ffile->next_size, 1, ffile->fp) != 1)
    {
        free(data);
        return NULL;
    }

    return (sirinet_pkg_t *) data;
}

int siridb_ffile_pop_commit(siridb_ffile_t * ffile)
{
#ifdef DEBUG
    assert (ffile->next_size && ffile->fp != NULL);
    assert (ffile->pop_commit);
    ffile->pop_commit = 0;
#endif
    long int pos;

    if (fseek(ffile->fp, -ffile->next_size - 2 * sizeof(uint32_t), SEEK_END))
    {
        return -1;
    }

    if (fread(&ffile->next_size, sizeof(uint32_t), 1, ffile->fp) != 1)
    {
        return -1;
    }

    pos = ftell(ffile->fp);

    if (pos < 0)
    {
        return -1;
    }

    if (ftruncate(ffile->fp, ftell(ffile->fp)))
    {
        return -1;
    }

    return 0;
}

void siridb_ffile_open(siridb_ffile_t * ffile)
{
    /* check ffile->fp != NULL to see if the file open was successful */
    ffile->fp = fopen(ffile->fn, "r+");
}

void siridb_ffile_unlink(siridb_ffile_t * ffile)
{
    if (ffile->fp != NULL && fclose(ffile->fp))
    {
        log_critical("Cannot close '%s', (disk full?)", ffile->fn);
    }
    if (unlink(ffile->fn))
    {
        log_critical("Cannot remove fifo file: '%s'", ffile->fn);
    }
    free(ffile->fn);
    free(ffile);
}

void siridb_ffile_free(siridb_ffile_t * ffile)
{
    if (ffile->fp != NULL && fclose(ffile->fp))
    {
        log_critical("Cannot close '%s', (disk full?)", ffile->fn);
    }
    free(ffile->fn);
    free(ffile);
}
