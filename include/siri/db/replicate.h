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

#define REPLICATE_IDLE 0
#define REPLICATE_RUNNING 1
#define REPLICATE_PAUSED 2
#define REPLICATE_STOPPING 3
#define REPLICATE_CLOSED 4

typedef struct siridb_replicate_s
{
    int status;
    uv_timer_t * timer;
} siridb_replicate_t;

int siridb_replicate_init(siridb_t * siridb);
void siridb_replicate_destroy(siridb_t * siridb);
void siridb_replicate_start(siridb_replicate_t * replicate);
void siridb_replicate_close(siridb_replicate_t * replicate);
void siridb_replicate_pause(siridb_replicate_t * replicate);
void siridb_replicate_continue(siridb_replicate_t * replicate);


#define siridb_replicate_is_idle(replicate) (replicate->status == REPLICATE_IDLE)
