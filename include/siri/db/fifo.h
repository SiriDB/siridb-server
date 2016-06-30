/*
 * fifo.h - First in, first out file buffer.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 30-06-2016
 *
 */
#pragma once
#include <siri/db/db.h>
#include <llist/llist.h>
#include <siri/db/ffile.h>

typedef struct siridb_fifo_s
{
    char * path;
    llist_t * fifos;
    siridb_ffile_t * in;
    siridb_ffile_t * out;
    ssize_t max_id;  // max_id can be -1
} siridb_fifo_t;

siridb_fifo_t * siridb_fifo_new(siridb_t * siridb);

/*
 * destroy the fifo and close open files.
 * (signal is set if a file close has failed)
 */
void siridb_fifo_free(siridb_fifo_t * fifo);

/*
 * returns a package created with malloc or NULL when an error has occured.
 * (signal is set when return value is NULL)
 */
sirinet_pkg_t * siridb_fifo_pop(siridb_fifo_t * fifo);

/*
 * returns 0 if successful or another value in case of errors.
 * (signal can be set when result is not 0)
 */
int siridb_fifo_commit(siridb_fifo_t * fifo);

/* returns 0 if successful or a negative value in case of errors */
int siridb_fifo_close(siridb_fifo_t * fifo);

/* returns 0 if successful or a -1 in case of errors */
int siridb_fifo_open(siridb_fifo_t * fifo);
