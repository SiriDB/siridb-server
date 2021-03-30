/*
 * siri/evars.c
 */
#include <stdbool.h>
#include <siri/evars.h>
#include <siri/net/tcp.h>

static void evars__bool(const char * evar, uint8_t * b)
{
    char * u8str = getenv(evar);
    if (!u8str)
        return;

    *b = (bool) strtoul(u8str, NULL, 10);
}

static void evars__u16(const char * evar, uint16_t * u16)
{
    char * u16str = getenv(evar);
    if (!u16str)
        return;

    *u16 = (uint16_t) strtoul(u16str, NULL, 10);
}

static void evars__u16_mm(
        const char * evar,
        uint16_t * u16,
        uint16_t mi,
        uint16_t ma)
{
    uint16_t val;
    char * u16str = getenv(evar);
    if (!u16str)
        return;

    val = (uint16_t) strtoul(u16str, NULL, 10);
    if (val < mi || val > ma)
        return;

    *u16 = val;
}

static void evars__u32_mm(
        const char * evar,
        uint32_t * u32,
        uint32_t mi,
        uint32_t ma)
{
    uint32_t val;
    char * u32str = getenv(evar);
    if (!u32str)
        return;

    val = (uint32_t) strtoull(u32str, NULL, 10);
    if (val < mi || val > ma)
        return;

    *u32 = val;
}

static void evars__to_strn(const char * evar, char * s, size_t n)
{
    char * str = getenv(evar);
    if (!str || strlen(str) >= n)
        return;

    strncpy(s, str, n);
}

static void evars__ip_support(const char * evar, uint8_t * ip_support)
{
    char * str = getenv(evar);
    if (!str)
        return;

    if (strcasecmp(str, "ALL") == 0)
    {
        *ip_support = IP_SUPPORT_ALL;
    }
    else if (strcasecmp(str, "IPV4ONLY") == 0)
    {
        *ip_support = IP_SUPPORT_IPV4ONLY;
    }
    else if (strcasecmp(str, "IPV6ONLY") == 0)
    {
        *ip_support = IP_SUPPORT_IPV6ONLY;
    }
}

static void evars__to_addr(const char * evar, char ** addr)
{
    struct in_addr sa;
    struct in6_addr sa6;

    char * str = getenv(evar);
    if (!str || (
            !inet_pton(AF_INET, str, &sa) &&
            !inet_pton(AF_INET6, str, &sa6)))
        return;

    str = strdup(str);
    if (!str)
        return;

    free(*addr);
    *addr = str;
}

static void evars__to_addr_port(
        const char * evar,
        char * addr,
        uint16_t * port)
{
    char * str = getenv(evar);
    if (!str)
        return;

    (void) sirinet_extract_addr_port(str, addr, port);
}


void siri_evars_parse(siri_t * siri)
{
    evars__u16(
            "SIRIDB_LISTEN_CLIENT_PORT",
            &siri->cfg->listen_client_port);
    evars__u16(
            "SIRIDB_HTTP_STATUS_PORT",
            &siri->cfg->http_status_port);
    evars__u16(
            "SIRIDB_HTTP_API_PORT",
            &siri->cfg->http_api_port);
    evars__u16(
            "SIRIDB_MAX_OPEN_FILES",
            &siri->cfg->max_open_files);
    evars__bool(
            "SIRIDB_ENABLE_PIPE_SUPPORT",
            &siri->cfg->pipe_support);
    evars__bool(
            "SIRIDB_ENABLE_SHARD_COMPRESSION",
            &siri->cfg->shard_compression);
    evars__bool(
            "SIRIDB_ENABLE_SHARD_AUTO_DURATION",
            &siri->cfg->shard_auto_duration);
    evars__bool(
            "SIRIDB_IGNORE_BROKEN_DATA",
            &siri->cfg->ignore_broken_data);
    evars__to_strn(
            "SIRIDB_DB_PATH",
            siri->cfg->db_path,
            sizeof(siri->cfg->db_path));
    /* Read old environment variable for backwards compatibility */
    evars__to_strn(
            "SIRIDB_DEFAULT_DB_PATH",
            siri->cfg->db_path,
            sizeof(siri->cfg->db_path));
    evars__u32_mm(
            "SIRIDB_BUFFER_SYNC_INTERVAL",
            &siri->cfg->buffer_sync_interval,
            0, 300000);
    evars__u16_mm(
            "SIRIDB_HEARTBEAT_INTERVAL",
            &siri->cfg->heartbeat_interval,
            3, 300);
    evars__u32_mm(
            "SIRIDB_OPTIMIZING_INTERVAL",
            &siri->cfg->optimize_interval,
            0, 2419200);
    evars__ip_support(
            "SIRIDB_IP_SUPPORT",
            &siri->cfg->ip_support);
    evars__to_addr(
            "SIRIDB_BIND_CLIENT_ADDRESS",
            &siri->cfg->bind_client_addr);
    evars__to_addr(
            "SIRIDB_BIND_SERVER_ADDRESS",
            &siri->cfg->bind_backend_addr);
    evars__to_strn(
            "SIRIDB_PIPE_CLIENT_NAME",
            siri->cfg->pipe_client_name,
            sizeof(siri->cfg->pipe_client_name)-2);
    evars__to_addr_port(
            "SIRIDB_SERVER_NAME",
            siri->cfg->server_address,
            &siri->cfg->listen_backend_port);
}
