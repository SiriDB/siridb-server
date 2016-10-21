#pragma once

#include <inttypes.h>
#include <limits.h>
#include <siri/siri.h>

typedef struct siri_s siri_t;

#define SIRI_CFG_MAX_LEN_ADDRESS 256

typedef struct siri_cfg_s
{
    uint16_t listen_client_port;
    uint16_t listen_backend_port;
    uint16_t heartbeat_interval;
    uint16_t max_open_files;
    uint32_t optimize_interval;
    uint8_t ip_support;
    char server_address[SIRI_CFG_MAX_LEN_ADDRESS];
    char default_db_path[PATH_MAX];
} siri_cfg_t;

void siri_cfg_init(siri_t * siri);
