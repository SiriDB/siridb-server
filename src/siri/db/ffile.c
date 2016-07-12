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
 *
 */
#define _GNU_SOURCE
#include <siri/db/ffile.h>
#include <unistd.h>
#include <stdlib.h>
#include <logger/logger.h>
#include <assert.h>
#include <siri/err.h>
#include <ctype.h>
#include <string.h>

#define FFILE_DEFAULT_SIZE 104857600  // 100 MB
#define FFILE_NUMBERS 9  // how much numbers are used to generate the file.

/*
 * returns NULL in case an error has occurred and a signal is set if this
 * was a critical error.
 */
siridb_ffile_t * siridb_ffile_new(
        uint64_t id,
        const char * path,
        sirinet_pkg_t * pkg)
{
    siridb_ffile_t * ffile = (siridb_ffile_t *) malloc(sizeof(siridb_ffile_t));

    if (ffile == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    if (asprintf(&ffile->fn, "%s%0*lu.fifo", path, FFILE_NUMBERS, id) < 0)
    {
        ERR_ALLOC
        free(ffile);
        return NULL;
    }

    ffile->id = id;
    ffile->next_size = 0;

    if (access(ffile->fn, R_OK) == -1)
    {
        /* create a new fifo file */
        ffile->fp = fopen(ffile->fn, "w+");

        if (ffile->fp == NULL)
        {
            ERR_FILE
            siridb_ffile_free(ffile);
            return NULL;
        }

        if (pkg == NULL)
        {
            ffile->free_space = FFILE_DEFAULT_SIZE;
        }
        else
        {
            /* we need 4 extra zeros (uint32_t) at the start of a fifo file */
            size_t size = pkg->len + PKG_HEADER_SIZE + 2 * sizeof(uint32_t);

            /* set free space to a value is will always fit */
            ffile->free_space = (size > FFILE_DEFAULT_SIZE) ?
                    size : FFILE_DEFAULT_SIZE;

            /* because we has enough free space, this should always work */
            if (siridb_ffile_append(ffile, pkg) != FFILE_SUCCESS)
            {
                ERR_FILE;
                siridb_ffile_free(ffile);
                return NULL;
            }
        }
    }
    else
    {
        /* open existing fifo file */
        ffile->fp = fopen(ffile->fn, "r+");

        if (ffile->fp == NULL || fseek(ffile->fp, 0, SEEK_END))
        {
            ERR_FILE
            siridb_ffile_free(ffile);
            return NULL;
        }

        if (ftell(ffile->fp) >= sizeof(uint32_t))
        {
            ffile->free_space = 0;
            if (    fseek(ffile->fp, -(long int) sizeof(uint32_t), SEEK_END) ||
                    fread(  &ffile->next_size,
                            sizeof(uint32_t),
                            1,
                            ffile->fp) != 1 ||
                    fclose(ffile->fp))
            {
                ERR_FILE
                siridb_ffile_free(ffile);
                return NULL;
            }
            ffile->fp = NULL;
        }

        if (!ffile->next_size)
        {
            log_warning("Empty fifo found, removing file: '%s'", ffile->fn);
            /*
             * signal can be set in unlink, otherwise it's just an empty
             * fifo and can be removed.
             */
            siridb_ffile_unlink(ffile);
            ffile = NULL;
        }
    }
    return ffile;
}

/*
 * returns a result code, usually FFILE_ERROR is critical but signal
 * will never be set by this function.
 */
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
            fwrite(&size, sizeof(uint32_t), 1, ffile->fp) != 1 ||
            fflush(ffile->fp))
    {
        return FFILE_ERROR;
    }

    return FFILE_SUCCESS;
}

/*
 * returns 1 (true) if the file name is valid and 0 (false) if not
 */
int siridb_ffile_check_fn(const char * fn)
{
    int i = 0;
    while (*fn && isdigit(*fn))
    {
        fn++;
        i++;
    }
    return (i == FFILE_NUMBERS) ? (strcmp(fn, ".fifo") == 0) : 0;
}

/*
 * returns a package object or NULL in case of an error.
 *
 * warning: be sure to check 'next_size' before calling this function.
 */
sirinet_pkg_t * siridb_ffile_pop(siridb_ffile_t * ffile)
{
#ifdef DEBUG
    assert (ffile->next_size);
    assert (ffile->fp != NULL);
#endif

    if (fseek(
            ffile->fp,
            -(long int) (ffile->next_size + sizeof(uint32_t)),
            SEEK_END))
    {
        log_critical("Seek error in '%s'", ffile->fn);
        return NULL;
    }
    sirinet_pkg_t * pkg = (sirinet_pkg_t *) malloc(ffile->next_size);

    if (pkg == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    if (fread(pkg, ffile->next_size, 1, ffile->fp) != 1)
    {
        log_critical(
                "Error while reading %lu bytes from '%s'",
                ffile->next_size,
                ffile->fn);
        free(pkg);
        return NULL;
    }

    return pkg;
}

/*
 * returns 0 if successful, -1 in case of an error
 */
int siridb_ffile_pop_commit(siridb_ffile_t * ffile)
{
#ifdef DEBUG
    assert (ffile->next_size && ffile->fp != NULL);
#endif

    long int pos;
    int n;

    return (fseek(
                ffile->fp,
                -(long int) ffile->next_size - 2 * sizeof(uint32_t),
                SEEK_END) ||
            fread(&ffile->next_size, sizeof(uint32_t), 1, ffile->fp) != 1 ||
            (pos = ftell(ffile->fp)) < 0 ||
            (n = fileno(ffile->fp)) == -1 ||
            ftruncate(n, pos)) ?
                    -1 : 0;
}


/*
 * signal can be set in case of file errors
 */
void siridb_ffile_unlink(siridb_ffile_t * ffile)
{
    if (ffile->fp != NULL && fclose(ffile->fp))
    {
        ERR_FILE
    }
    if (unlink(ffile->fn))
    {
        ERR_FILE
        log_critical("Cannot remove fifo file: '%s'", ffile->fn);
    }
    free(ffile->fn);
    free(ffile);
}

/*
 * signal can be set in case of file errors
 */
void siridb_ffile_free(siridb_ffile_t * ffile)
{
    if (ffile->fp != NULL && fclose(ffile->fp))
    {
        ERR_FILE
    }
    free(ffile->fn);
    free(ffile);
}
