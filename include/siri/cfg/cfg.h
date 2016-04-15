#pragma once

#include <inttypes.h>
#include <limits.h>

#define SIRI_CFG_MAX_LEN_ADDRESS 256

typedef struct siri_cfg_s
{
    char listen_client_address[SIRI_CFG_MAX_LEN_ADDRESS];
    uint16_t listen_client_port;
    char listen_backend_address[SIRI_CFG_MAX_LEN_ADDRESS];
    uint16_t listen_backend_port;
    char default_db_path[PATH_MAX];
    uint16_t max_open_files;
} siri_cfg_t;

void siri_cfg_init(void);


siri_cfg_t siri_cfg;
