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
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/db/fifo.h>
#include <siri/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

static int FIFO_walk_free(siridb_ffile_t * ffile, void * args);
static int FIFO_init(siridb_fifo_t * fifo);

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 *
 * Make sure siridb->replica is not NULL since this function needs its UUID.
 */
siridb_fifo_t * siridb_fifo_new(siridb_t * siridb)
{
    siridb_fifo_t * fifo = (siridb_fifo_t *) malloc(sizeof(siridb_fifo_t));

    if (fifo == NULL)
    {
        ERR_ALLOC
        return NULL;
    }

    fifo->fifos = llist_new();

    if (fifo->fifos == NULL)
    {
        free(fifo);
        return NULL;  /* signal is raised */
    }

    fifo->in = NULL;
    fifo->out = NULL;

    char str_uuid[37];
    uuid_unparse_lower(siridb->replica->uuid, str_uuid);

    if (asprintf(&fifo->path, "%s.%s/", siridb->dbpath, str_uuid) < 0)
    {
        ERR_ALLOC
        siridb_fifo_free(fifo);
        return NULL;
    }

    if (FIFO_init(fifo))
    {
        siridb_fifo_free(fifo);
        return NULL;
    }

    fifo->max_id = (fifo->fifos->len) ?
            ((siridb_ffile_t *) fifo->fifos->last->data)->id : -1;

    fifo->in = siridb_ffile_new(++fifo->max_id, fifo->path, NULL);
    if (fifo->in == NULL)
    {
        ERR_FILE
        siridb_fifo_free(fifo);
        return NULL;
    }

    if (llist_append(fifo->fifos, fifo->in))
    {
        siridb_fifo_free(fifo);
        return NULL;  /* signal is raised */
    }

    /* we have at least one fifo in the list */
    fifo->out = (siridb_ffile_t *) llist_shift(fifo->fifos);

#ifdef DEBUG
    assert (fifo->out != NULL);
#endif

    if (fifo->out->fp == NULL)
    {
        siridb_ffile_open(fifo->out, "r+");
        if (fifo->out->fp == NULL)
        {
            ERR_FILE
            log_critical("Cannot open file: '%s'", fifo->out->fn);
            siridb_fifo_free(fifo);
            return NULL;
        }
    }

    return fifo;
}

/*
 * Returns 0 if successful or anything else if not.
 * (signal is set in case of an error)
 */
int siridb_fifo_append(siridb_fifo_t * fifo, sirinet_pkg_t * pkg)
{
    switch(siridb_ffile_append(fifo->in, pkg))
    {
    case FFILE_NO_FREE_SPACE:
        if (fifo->in != fifo->out)
        {
        	if (fclose(fifo->in->fp))
			{
				ERR_FILE
			}
        	fifo->in->fp = NULL;
        }

        fifo->in = siridb_ffile_new(++fifo->max_id, fifo->path, pkg);

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
        ERR_FILE
        break;
    }
    return siri_err;
}

/*
 * returns a package created with malloc or NULL when an error has occurred.
 * (signal is set in case of a malloc error, not in case of a file error)
 *
 * warning:
 *      be sure to check the fifo using siridb_fifo_has_data() and
 *      siridb_fifo_is_open() before calling this function.
 */
sirinet_pkg_t * siridb_fifo_pop(siridb_fifo_t * fifo)
{
    sirinet_pkg_t * pkg = siridb_ffile_pop(fifo->out);
    if (pkg == NULL && !siri_err)
    {
        /*
         * In case siri_err is not set, we can try to recover by commiting an
         * error. We should not do this in case of malloc errors.
         */
        siridb_fifo_commit_err(fifo);
    }
    return pkg;
}

/*
 * returns 0 if successful or another value in case of errors.
 * (signal can be set when result is not 0)
 */
int siridb_fifo_commit(siridb_fifo_t * fifo)
{
    if (siridb_ffile_pop_commit(fifo->out))
    {
        log_error("Error occurred when shrinking file: '%s' ",
                fifo->out->fn);
        if (fifo->out != fifo->in)
        {
            log_warning(
                    "We try to recover from this error by removing this fifo.");
            fifo->out->next_size = 0;
        }
        else
        {
            ERR_FILE
            log_critical(
                    "The current fifo buffer is corrupt and we currently "
                    "cannot recover from this error. "
                    "(a reboot might remove the corrupt fifo)");
        }
    }

    if (!fifo->out->next_size && fifo->out != fifo->in)
    {
        siridb_ffile_unlink(fifo->out);
        fifo->out = (siridb_ffile_t *) llist_shift(fifo->fifos);

        /* fifo->out->fp can be open in case it is equal to fifo->in */
        if (fifo->out->fp == NULL)
        {
            siridb_ffile_open(fifo->out, "r+");
            if (fifo->out->fp == NULL)
            {
                ERR_FILE
                log_critical("Cannot open file: '%s'", fifo->out->fn);
            }
        }
    }

#ifdef DEBUG
    assert (fifo->out != NULL);
#endif

    return siri_err;
}

/*
 * returns 0 if successful or another value in case of errors.
 * (signal can be set when result is not 0)
 */
int siridb_fifo_commit_err(siridb_fifo_t * fifo)
{
    log_error(
            "Handling the last package from the fifo buffer has failed, "
            "commit the pop anyway");
    return siridb_fifo_commit(fifo);
}

/*
 * returns 0 if successful or a negative value in case of errors
 */
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

/*
 * returns 0 if successful or a -1 in case of errors
 */
int siridb_fifo_open(siridb_fifo_t * fifo)
{
#ifdef DEBUG
    assert (fifo->in->fp == NULL);
#endif

    /* open fifo 'in' */
    siridb_ffile_open(fifo->in, "r+");

    /* if 'out' is not the same as 'in', we also need to open 'out' */
    if (fifo->out->fp == NULL)
    {
        siridb_ffile_open(fifo->out, "r+");
    }

    return (fifo->in->fp != NULL && fifo->out->fp != NULL) ? 0 : -1;
}

/*
 * destroy the fifo and close open files.
 * (signal is set if a file close has failed)
 */
void siridb_fifo_free(siridb_fifo_t * fifo)
{
    /* we only need to free fifo->out because fido->in is either in the
     * list or the same as fifo->out. (fifo->out is never in the list)
     */
    siridb_ffile_free(fifo->out);

    llist_free_cb(fifo->fifos, (llist_cb) FIFO_walk_free, NULL);
    free(fifo->path);
    free(fifo);
}

/*
 * returns 1 and a signal can be set if a file close has failed
 */
static int FIFO_walk_free(siridb_ffile_t * ffile, void * args)
{
    siridb_ffile_free(ffile);
    return 1;
}

/*
 * returns 0 when successful or any other value when not.
 * (in case of an error a signal is set too)
 */
static int FIFO_init(siridb_fifo_t * fifo)
{
    struct stat st = {0};

    siridb_ffile_t * ffile;

    if (stat(fifo->path, &st) == -1)
    {
        log_warning(
                "Fifo directory not found, creating directory '%s'.",
                fifo->path);
        if (mkdir(fifo->path, 0700) == -1)
        {
            log_critical("Cannot create directory '%s'.", fifo->path);
            ERR_C
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
            log_critical("Cannot read fifo directory '%s'.", fifo->path);
            ERR_C
        }

        for (int n = 0; n < total; n++)
        {
            if (siridb_ffile_check_fn(fifo_list[n]->d_name))
            {
                if (asprintf(&fn, "%s%s", fifo->path, fifo_list[n]->d_name) < 0)
                {
                    ERR_ALLOC
                }
                else
                {
                    uint64_t id = strtoull(fifo_list[n]->d_name, NULL, 10);
                    ffile = siridb_ffile_new(id, fifo->path, NULL);
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
    return siri_err;
}


