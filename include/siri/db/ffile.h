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
    uint32_t next_size;  /* must be uint32_t (4 bytes)  */
    FILE * fp;
    int fd;
    long int size;
} siridb_ffile_t;

void siridb_ffile_open(siridb_ffile_t * ffile, const char * opentype);
siridb_ffile_t * siridb_ffile_new(
        uint64_t id,
        const char * path,
        sirinet_pkg_t * pkg);
int siridb_ffile_check_fn(const char * fn);
void siridb_ffile_free(siridb_ffile_t * ffile);
void siridb_ffile_unlink(siridb_ffile_t * ffile);
sirinet_pkg_t * siridb_ffile_pop(siridb_ffile_t * ffile);
int siridb_ffile_pop_commit(siridb_ffile_t * ffile);
siridb_ffile_result_t siridb_ffile_append(
        siridb_ffile_t * ffile,
        sirinet_pkg_t * pkg);
