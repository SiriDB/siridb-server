/*
 * fifo.c - First in, first out file buffer .
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 30-06-2016
 *
 */
#define _GNU_SOURCE
#include <siri/db/fifo.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <logger/logger.h>
#include <ctype.h>
#include <assert.h>

#define FIFO_NUMBERS 9

static int FIFO_walk_free(siridb_ffile_t * ffile, void * args);
static int FIFO_is_fifo_fn(const char * fn);
static int FIFO_init(siridb_fifo_t * fifo);
static siridb_ffile_t * FIFO_next(siridb_fifo_t * fifo, sirinet_pkg_t * pkg);

siridb_fifo_t * siridb_fifo_new(siridb_t * siridb)
{
    siridb_fifo_t * fifo = (siridb_fifo_t *) malloc(sizeof(siridb_fifo_t));

    fifo->fifos = llist_new();
    fifo->in = NULL;
    fifo->out = NULL;

    char str_uuid[37];
    uuid_unparse_lower(siridb->replica->uuid, str_uuid);

    if (asprintf(&fifo->path, "%s.%s/", siridb->dbpath, str_uuid) < 0)
    {
        return NULL;
    }

    if (FIFO_init(fifo))
    {
        siridb_fifo_free(fifo);
        return NULL;
    }

    fifo->max_id = (fifo->fifos->len) ?
            ((siridb_ffile_t *) fifo->fifos->last)->id : -1;

    fifo->in = FIFO_next(fifo, NULL);
    llist_append(fifo->fifos, fifo->in);

    /* we have at least one fifo in the list */
    fifo->out = llist_shift(fifo->fifos);

    if (fifo->out->fp == NULL)
    {
        fopen(fifo->out->fn, "r+");
    }

    return fifo;
}

int siridb_fifo_append(siridb_fifo_t * fifo, sirinet_pkg_t * pkg)
{
    switch(siridb_ffile_append(fifo->in, pkg))
    {
    case FFILE_NO_FREE_SPACE:
        if (fifo->in != fifo->out)
        {
            fclose(fifo->in);
        }
        fifo->in = FIFO_next(fifo, pkg);
        if (!fifo->out->next_size)
        {
            /* when the out fifo has no next size, we want to use the new
             * in fifo also as the out fifo.
             */
            siridb_ffile_unlink(fifo->out);
            fifo->out = fifo->in;
        }
        else
        {
            llist_append(fifo->fifos, fifo->in);
        }
        break;
    case FFILE_SUCCESS:
        break;
    case FFILE_ERROR:
        return -1;
    }
    return 0;
}

sirinet_pkg_t * siridb_fifo_pop(siridb_fifo_t * fifo)
{
    sirinet_pkg_t * pkg = siridb_ffile_pop(fifo->out);
    if (!fifo->out->next_size && fifo->out != fifo->in)
    {
        siridb_ffile_unlink(fifo->out);
        fifo->out = llist_shift(fifo->fifos);
    }
    return pkg;
}

int siridb_fifo_close(siridb_fifo_t * fifo)
{
#ifdef DEBUG
    assert (fifo->in->fp != NULL);
#endif
    int rc = 0;

    /* close the 'in' fifo */
    rc += fclose(fifo->in->fp);
    fifo->in->fp = NULL;

    /* if 'out' is not the same as 'in', we also need to close 'out' */
    if (fifo->out->fp != NULL)
    {
        rc += fclose(fifo->out->fp);
        fifo->out->fp = NULL;
    }

    /* return 0 if successful or a negative value in case of errors */
    return rc;
}

int siridb_fifo_open(siridb_fifo_t * fifo)
{
#ifdef DEBUG
    assert (fifo->in->fp == NULL);
#endif

    /* open fifo 'in' */
    fifo->in->fp = fopen(fifo->in->fn, "r+");

    /* if 'out' is not the same as 'in', we also need to open 'out' */
    if (fifo->out->fp == NULL)
    {
        fifo->out->fp = fopen(fifo->out->fn, "r+");
    }

    return (fifo->in->fp != NULL && fifo->out->fp != NULL) ? 0 : -1;
}

void siridb_fifo_free(siridb_fifo_t * fifo)
{
    /* we only need to free fifo->out because fido->in is either in the
     * list or the same as fifo->out. (fifo->out is never in the list)
     */
    siridb_ffile_free(fifo->out);

    llist_free_cb(fifo->fifos, (llist_cb_t) FIFO_walk_free, NULL);
    free(fifo->path);
    free(fifo);
}

static siridb_ffile_t * FIFO_next(siridb_fifo_t * fifo, sirinet_pkg_t * pkg)
{
    char * fn;
    fifo->max_id++;
    if (asprintf(
            &fn,
            "%s%0*ld.fifo",
            fifo->path,
            FIFO_NUMBERS,
            fifo->max_id) < 0)
    {
        return NULL; /* error occurred */
    }

    /* fn will be freed when ffile is destroyed */
    return siridb_ffile_new(fifo->max_id, fn, pkg);
}

static int FIFO_walk_free(siridb_ffile_t * ffile, void * args)
{
    siridb_ffile_free(ffile);
    return 1;
}

static int FIFO_init(siridb_fifo_t * fifo)
{
    struct stat st = {0};
    int rc = 0;

    siridb_ffile_t * ffile;

    if (stat(fifo->path, &st) == -1)
    {
        log_warning(
                "Fifo directory not found, creating directory '%s'.",
                fifo->path);
        if (mkdir(fifo->path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.", fifo->path);
            return -1;
        }
    }
    else
    {
        struct dirent ** fifo_list;
        char * fn;
        int total = scandir(fifo->path, &fifo_list, NULL, alphasort);

        if (total < 0)
        {
            /* no need to free fifo_list when total < 0 */
            log_error("Cannot read fifo directory '%s'.", fifo->path);
            return -1;
        }

        for (int n = 0; n < total; n++)
        {
            if (FIFO_is_fifo_fn(fifo_list[n]->d_name))
            {
                if (asprintf(&fn, "%s%s", fifo->path, fifo_list[n]->d_name) < 0)
                {
                    rc = -1;
                    log_critical("Allocation failed");
                }
                else
                {
                    uint64_t id = strtoull(fifo_list[n]->d_name, NULL, 10);
                    ffile = siridb_ffile_new(id, fn, NULL);
                    if (ffile != NULL)
                    {
                        llist_append(fifo->fifos, ffile);
                    }
                    free(fn);
                }
            }
            free(fifo_list[n]);
        }
        free(fifo_list);
    }
    return rc;
}

static int FIFO_is_fifo_fn(const char * fn)
{
    int i = 0;
    while (*fn && isdigit(*fn))
    {
        fn++;
        i++;
    }
    return (i == FIFO_NUMBERS) ? (strcmp(fn, ".fifo") == 0) : 0;
}
