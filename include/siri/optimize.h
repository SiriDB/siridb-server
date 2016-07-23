/*
 * optimize.h - Optimize task SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-05-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/siri.h>

#define SIRI_OPTIMIZE_PENDING 0
#define SIRI_OPTIMIZE_RUNNING 1
#define SIRI_OPTIMIZE_CANCELLED 2
#define SIRI_OPTIMIZE_PAUSED 3

typedef struct siri_s siri_t;

typedef struct siri_optimize_s
{
    uv_timer_t timer;
    int status;
    time_t start;
    uv_work_t work;
    uint16_t pause;
} siri_optimize_t;

void siri_optimize_init(siri_t * siri);
void siri_optimize_stop(siri_t * siri);
void siri_optimize_pause(siri_optimize_t * optimize);
void siri_optimize_continue(siri_optimize_t * optimize);
