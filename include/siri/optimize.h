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
#include <stdbool.h>

typedef struct siri_optimize_s
{
    uv_timer_t timer;
    bool running;
    struct timespec start;
    uv_work_t work;
} siri_optimize_t;

void siri_optimize_init(void);
void siri_optimize_destroy(void);
