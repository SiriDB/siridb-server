/*
 * ffile.h - FIFO file.
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
#include <siri/net/pkg.h>

typedef enum
{
    FFILE_NO_FREE_SPACE=-2,
    FFILE_ERROR,
    FFILE_SUCCESS
} siridb_ffile_result_t;

typedef struct siridb_ffile_s
{
    uint64_t id;
    char * fn;
    uint32_t free_space;
    uint32_t next_size;  // must be uint32_t (4 bytes)
#ifdef DEBUG
    int pop_commit;
#endif
    FILE * fp;
} siridb_ffile_t;

/* fn must be set using malloc and will be freed with either
 * siridb_ffile_free or siridb_ffile_unlink.
 */
siridb_ffile_t * siridb_ffile_new(
        uint64_t id,
        char * fn,
        sirinet_pkg_t * pkg);

void siridb_ffile_open(siridb_ffile_t * ffile);
void siridb_ffile_free(siridb_ffile_t * ffile);
void siridb_ffile_unlink(siridb_ffile_t * ffile);
sirinet_pkg_t * siridb_ffile_pop(siridb_ffile_t * ffile);

/* returns 0 if successful, -1 in case of an error */
int siridb_ffile_pop_commit(siridb_ffile_t * ffile);

siridb_ffile_result_t siridb_ffile_append(
        siridb_ffile_t * ffile,
        sirinet_pkg_t * pkg);
