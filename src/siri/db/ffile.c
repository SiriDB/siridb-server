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
#include <assert.h>
#include <ctype.h>
#include <logger/logger.h>
#include <siri/db/ffile.h>
#include <siri/err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FFILE_DEFAULT_SIZE 104857600  // 100 MB
#define FFILE_NUMBERS 9  // how much numbers are used to generate the file.

/*
 * Open the fifo file. (set both the file pointer and file descriptor
 * In case of and error, fifo->fp is set to NULL
 */
void siridb_ffile_open(siridb_ffile_t * ffile, const char * opentype)
{
    ffile->fp = fopen(ffile->fn, opentype);
    if (ffile->fp != NULL)
    {
        ffile->fd = fileno(ffile->fp);
        if (ffile->fd == -1)
        {
        	log_critical("Error reading file descriptor: '%s'", ffile->fn);
            fclose(ffile->fp);
            ffile->fp = NULL;
        }
    }
}

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

    if (asprintf(&ffile->fn, "%s%0*llu.fifo",
    		path,
			FFILE_NUMBERS,
			(unsigned long long) id) < 0)
    {
        ERR_ALLOC
        free(ffile);
        return NULL;
    }

    ffile->id = id;
    ffile->next_size = 0;

    siridb_ffile_open(ffile, "r+");

    if (ffile->fp == NULL)
    {
        /* create a new fifo file */
        siridb_ffile_open(ffile, "w+");

        if (ffile->fp == NULL)
        {
            ERR_FILE
            siridb_ffile_free(ffile);
            return NULL;
        }

        if (pkg == NULL)
        {
            ffile->size = ffile->free_space = FFILE_DEFAULT_SIZE;
        }
        else
        {
            /* we need 4 extra zeros (uint32_t) at the start of a fifo file */
            size_t size = pkg->len + PKG_HEADER_SIZE + 2 * sizeof(uint32_t);

            /* set free space to a value is will always fit */
            ffile->size = ffile->free_space = (size > FFILE_DEFAULT_SIZE) ?
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
        /* reading existing fifo file */

        if (fseeko(ffile->fp, 0, SEEK_END))
        {
            ERR_FILE
            siridb_ffile_free(ffile);
            return NULL;
        }

        if ((ffile->size = ftello(ffile->fp)) >= sizeof(uint32_t))
        {
            ffile->free_space = 0;
            if (    fseeko(ffile->fp, -(long int) sizeof(uint32_t), SEEK_END) ||
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
            log_debug("Empty fifo found, removing: '%s'", ffile->fn);
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

    if (    fseeko(ffile->fp, (off_t) ffile->free_space, SEEK_SET) ||
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
    if (fseeko(
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

    if (    ffile->next_size < PKG_HEADER_SIZE ||
            pkg->len != ffile->next_size - PKG_HEADER_SIZE)
    {
        log_critical(
                "Corrupt package in fifo: '%s' ", ffile->fn);
        free(pkg);
        return NULL;
    }

    return pkg;
}

/*
 * returns 0 if successful, -1 in case of an error
 *
 * Warning: ffile->size might be incorrect if an error occurred
 */
int siridb_ffile_pop_commit(siridb_ffile_t * ffile)
{
#ifdef DEBUG
    assert (ffile->next_size && ffile->fp != NULL);
#endif

    ffile->size -= ffile->next_size + sizeof(uint32_t);

    return (fseeko(
                ffile->fp,
                ffile->size - sizeof(uint32_t),
                SEEK_SET) ||
            fread(&ffile->next_size, sizeof(uint32_t), 1, ffile->fp) != 1 ||
            ftruncate(ffile->fd, ffile->size)) ?
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
