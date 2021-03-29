/*
 * cfg.h - Read the global SiriDB configuration file. (usually siridb.conf)
 */
#ifndef SIRI_CFG_H_
#define SIRI_CFG_H_

typedef struct siri_cfg_s siri_cfg_t;

#define SIRI_CFG_MAX_LEN_ADDRESS 256

/* do not use more than x percent for the max limit for open sharding files */
#define RLIMIT_PERC_FOR_SHARDING 0.5

#define MAX_OPEN_FILES_LIMIT 32768
#define MIN_OPEN_FILES_LIMIT 3
#define DEFAULT_OPEN_FILES_LIMIT MAX_OPEN_FILES_LIMIT

#include <inttypes.h>
#include <limits.h>
#include <siri/siri.h>
#include <xpath/xpath.h>

void siri_cfg_init(siri_t * siri);
void siri_cfg_destroy(siri_t * siri);

struct siri_cfg_s
{
    uint32_t optimize_interval;
    uint32_t buffer_sync_interval;

    uint16_t listen_client_port;
    uint16_t listen_backend_port;
    uint16_t heartbeat_interval;
    uint16_t max_open_files;

    uint16_t http_status_port;
    uint16_t http_api_port;
    uint8_t pipe_support;
    uint8_t ip_support;
    uint8_t shard_compression;
    uint8_t shard_auto_duration;

    char * bind_client_addr;
    char * bind_backend_addr;
    char server_address[SIRI_CFG_MAX_LEN_ADDRESS];
    char db_path[XPATH_MAX];
    char pipe_client_name[XPATH_MAX];

    uint8_t ignore_broken_data;
};

#endif  /* SIRI_CFG_H_ */
