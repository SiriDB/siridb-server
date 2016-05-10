/*
 * clserver.h - TCP server for serving client requests.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/siri.h>

typedef struct siri_s siri_t;

int sirinet_clserver_init(siri_t * siri);



