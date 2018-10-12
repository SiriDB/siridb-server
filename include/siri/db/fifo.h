/*
 * fifo.h - First in, first out file buffer.
 */
#ifndef SIRIDB_FIFO_H_
#define SIRIDB_FIFO_H_

typedef struct siridb_fifo_s siridb_fifo_t;

#include <stddef.h>
#include <siri/db/db.h>
#include <llist/llist.h>
#include <siri/db/ffile.h>


siridb_fifo_t * siridb_fifo_new(siridb_t * siridb);
void siridb_fifo_free(siridb_fifo_t * fifo);
size_t siridb_fifo_size(siridb_fifo_t * fifo);
int siridb_fifo_append(siridb_fifo_t * fifo, sirinet_pkg_t * pkg);
sirinet_pkg_t * siridb_fifo_pop(siridb_fifo_t * fifo);
int siridb_fifo_commit(siridb_fifo_t * fifo);
int siridb_fifo_commit_err(siridb_fifo_t * fifo);
int siridb_fifo_close(siridb_fifo_t * fifo);
int siridb_fifo_open(siridb_fifo_t * fifo);

/*
 * Value is greater than 0 when the fifo has data or 0 when empty.
 * Use this to check if the fifo has data. (must be done before calling pop)
 */
#define siridb_fifo_has_data(fifo) fifo->out->next_size


/*
 * Returns 1 if the fifo buffer is open or 0 if closed.
 */
#define siridb_fifo_is_open(fifo) (fifo->in->fp != NULL)

struct siridb_fifo_s
{
    char * path;
    llist_t * fifos;
    siridb_ffile_t * in;
    siridb_ffile_t * out;
    ssize_t max_id;     /*  max_id can be -1        */
};

#endif  /* SIRIDB_FIFO_H_ */
