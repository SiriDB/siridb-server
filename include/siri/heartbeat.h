/*
 * heartbeat.c - Heart-beat task SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * There is one and only one heart-beat task thread running for SiriDB. For
 * this reason we do not need to parse data but we should only take care for
 * locks while writing data.
 *
 * changes
 *  - initial version, 17-06-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/siri.h>

#define SIRI_HEARTBEAT_PENDING 0
#define SIRI_HEARTBEAT_RUNNING 1
#define SIRI_HEARTBEAT_CANCELLED 2

typedef struct siri_s siri_t;

typedef struct siri_heartbeat_s
{
    uv_timer_t timer;
    int status;
    struct timespec start;
    uv_work_t work;
} siri_heartbeat_t;

void siri_heartbeat_init(siri_t * siri);
void siri_heartbeat_cancel(void);
