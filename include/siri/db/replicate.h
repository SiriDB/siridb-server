/*
 * replicate.h - Replicate SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 11-07-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/db/db.h>

typedef struct siridb_s siridb_t;

typedef enum
{
    REPLICATE_IDLE,
    REPLICATE_RUNNING,
    REPLICATE_PAUSED,
    REPLICATE_STOPPING,
    REPLICATE_CLOSED
} siridb_replicate_status_t;


typedef struct siridb_replicate_s
{
    siridb_replicate_status_t status;
    uv_timer_t * timer;
    FILE * init_fp;
    char * init_fn;  /* NULL when not initializing */
    int init_fd;
    uint32_t * series_id;
} siridb_replicate_t;

int siridb_replicate_init(siridb_t * siridb, int isnew);
int siridb_replicate_create(siridb_t * siridb);
void siridb_replicate_destroy(siridb_t * siridb);
int siridb_replicate_finish_init(siridb_replicate_t * replicate);
void siridb_replicate_start(siridb_replicate_t * replicate);
void siridb_replicate_close(siridb_replicate_t * replicate);
void siridb_replicate_pause(siridb_replicate_t * replicate);
void siridb_replicate_continue(siridb_replicate_t * replicate);


#define siridb_replicate_is_idle(replicate) (replicate->status == REPLICATE_IDLE)
