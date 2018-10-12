/*
 * replicate.h - Replicate SiriDB.
 */
#ifndef SIRIDB_REPLICATE_H_
#define SIRIDB_REPLICATE_H_

typedef enum
{
    REPLICATE_IDLE,
    REPLICATE_RUNNING,
    REPLICATE_PAUSED,
    REPLICATE_STOPPING,
    REPLICATE_CLOSED
} siridb_replicate_status_t;

typedef struct siridb_replicate_s siridb_replicate_t;

#include <uv.h>
#include <siri/db/db.h>
#include <siri/db/initsync.h>
#include <siri/net/pkg.h>

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

struct siridb_replicate_s
{
    siridb_replicate_status_t status;
    uv_timer_t * timer;
    siridb_initsync_t * initsync;
};

#endif  /* SIRIDB_REPLICATE_H_ */
