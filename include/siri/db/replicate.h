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
#include <siri/db/initsync.h>
#include <siri/net/pkg.h>

typedef struct siridb_initsync_s siridb_initsync_t;
typedef struct siridb_s siridb_t;
typedef struct sirinet_pkg_s sirinet_pkg_t;

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
    siridb_initsync_t * initsync;
} siridb_replicate_t;

int siridb_replicate_init(siridb_t * siridb, siridb_initsync_t * initsync);
void siridb_replicate_free(siridb_replicate_t ** replicate);
int siridb_replicate_finish_init(siridb_replicate_t * replicate);
void siridb_replicate_start(siridb_replicate_t * replicate);
void siridb_replicate_close(siridb_replicate_t * replicate);
void siridb_replicate_pause(siridb_replicate_t * replicate);
void siridb_replicate_continue(siridb_replicate_t * replicate);
int siridb_replicate_pkg(siridb_t * siridb, sirinet_pkg_t * pkg);
sirinet_pkg_t * siridb_replicate_pkg_filter(
        siridb_t * siridb,
        unsigned char * data,
        size_t len,
        int flags);

#define siridb_replicate_is_idle(replicate) (replicate->status == REPLICATE_IDLE)
