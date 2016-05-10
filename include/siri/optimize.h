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

typedef struct siri_s siri_t;

typedef struct siri_optimize_s
{
    uv_timer_t timer;
    int status;
    struct timespec start;
    uv_work_t work;
} siri_optimize_t;

void siri_optimize_init(siri_t * siri);
void siri_optimize_destroy(void);
