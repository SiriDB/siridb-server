/*
 * cfg.h - Read the global SiriDB configuration file. (usually siridb.conf)
 */
#ifndef SIRI_CFG_H_
#define SIRI_CFG_H_

typedef struct siri_cfg_s siri_cfg_t;

#define SIRI_CFG_MAX_LEN_ADDRESS 256

#include <inttypes.h>
#include <limits.h>
#include <siri/siri.h>
#include <xpath/xpath.h>

void siri_cfg_init(siri_t * siri);
void siri_cfg_destroy(siri_t * siri);

struct siri_cfg_s
{
    uint16_t listen_client_port;
    uint16_t listen_backend_port;
    char * bind_client_addr;
    char * bind_backend_addr;
    uint16_t heartbeat_interval;
    uint16_t max_open_files;
    uint32_t optimize_interval;
    uint8_t ip_support;
    uint8_t shard_compression;
    char server_address[SIRI_CFG_MAX_LEN_ADDRESS];
    char default_db_path[XPATH_MAX];
    uint8_t pipe_support;
    char pipe_client_name[XPATH_MAX];
    uint32_t buffer_sync_interval;
};

#endif  /* SIRI_CFG_H_ */
