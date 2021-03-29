/*
 * cfg.h - Read the global SiriDB configuration file. (usually siridb.conf)
 */
#include <cfgparser/cfgparser.h>
#include <inttypes.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/cfg/cfg.h>
#include <stdio.h>
#include <stdlib.h>
#include <xstr/xstr.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <siri/net/tcp.h>

static siri_cfg_t siri_cfg = {
        .http_status_port=0,    /* 0=disabled, 1-16535=enabled */
        .http_api_port=0,       /* 0=disabled, 1-16535=enabled */
        .listen_client_port=9000,
        .listen_backend_port=9010,
        .bind_client_addr=NULL,
        .bind_backend_addr=NULL,
        .heartbeat_interval=30,
        .max_open_files=DEFAULT_OPEN_FILES_LIMIT,
        .optimize_interval=3600,
        .ip_support=IP_SUPPORT_ALL,
        .shard_compression=0,
        .shard_auto_duration=0,
        .server_address="localhost",
        .db_path="",
        .pipe_support=0,
        .pipe_client_name="siridb_client.sock",
        .buffer_sync_interval=0,
        .ignore_broken_data=0
};

static void SIRI_CFG_read_uint(
        cfgparser_t * cfgparser,
        const char * option_name,
        int min,
        int max,
        uint32_t * value);
static void SIRI_CFG_read_address_port(
        cfgparser_t * cfgparser,
        const char * option_name,
        char * address_pt,
        uint16_t * port_pt);
static void SIRI_CFG_read_addr(
        cfgparser_t * cfgparser,
        const char * option_name,
        char ** dest);
static void SIRI_CFG_read_pipe_client_name(cfgparser_t * cfgparser);
static void SIRI_CFG_read_db_path(cfgparser_t * cfgparser);
static void SIRI_CFG_read_max_open_files(cfgparser_t * cfgparser);
static void SIRI_CFG_read_ip_support(cfgparser_t * cfgparser);
static void SIRI_CFG_read_shard_compression(cfgparser_t * cfgparser);
static void SIRI_CFG_read_shard_auto_duration(cfgparser_t * cfgparser);
static void SIRI_CFG_read_pipe_support(cfgparser_t * cfgparser);
static void SIRI_CFG_ignore_broken_data(cfgparser_t * cfgparser);

void siri_cfg_init(siri_t * siri)
{
    /* Read the application configuration file. */
    cfgparser_t * cfgparser = cfgparser_new();
    cfgparser_return_t rc;
    uint32_t tmp;
    siri->cfg = &siri_cfg;
    rc = cfgparser_read(cfgparser, siri->args->config);
    if (rc != CFGPARSER_SUCCESS)
    {
        /* we could choose to continue with defaults but this is probably
         * not what users want so lets quit.
         */
        log_debug(
                "Not using configuration file '%s' (%s)",
                siri->args->config,
                cfgparser_errmsg(rc));
        cfgparser_free(cfgparser);
        return;
    }

    SIRI_CFG_read_address_port(
            cfgparser,
            "server_name",
            siri_cfg.server_address,
            &siri_cfg.listen_backend_port);

    tmp = siri_cfg.listen_client_port;

    SIRI_CFG_read_uint(
            cfgparser,
            "listen_client_port",
            1,
            65535,
            &tmp);
    siri_cfg.listen_client_port = (uint16_t) tmp;

    SIRI_CFG_read_uint(
            cfgparser,
            "optimize_interval",
            0,
            2419200,  /* 4 weeks */
            &siri_cfg.optimize_interval);

    tmp = siri_cfg.heartbeat_interval;
    SIRI_CFG_read_uint(
            cfgparser,
            "heartbeat_interval",
            3,
            300,
            &tmp);
    siri_cfg.heartbeat_interval = (uint16_t) tmp;

    tmp = siri_cfg.http_status_port;
    SIRI_CFG_read_uint(
            cfgparser,
            "http_status_port",
            0,
            65535,
            &tmp);
    siri_cfg.http_status_port = (uint16_t) tmp;

    tmp = siri_cfg.http_api_port;
    SIRI_CFG_read_uint(
            cfgparser,
            "http_api_port",
            0,
            65535,
            &tmp);
    siri_cfg.http_api_port = (uint16_t) tmp;

    SIRI_CFG_read_db_path(cfgparser);
    SIRI_CFG_read_max_open_files(cfgparser);
    SIRI_CFG_read_ip_support(cfgparser);
    SIRI_CFG_read_shard_compression(cfgparser);
    SIRI_CFG_read_shard_auto_duration(cfgparser);

    SIRI_CFG_read_addr(
            cfgparser,
            "bind_client_address",
            &siri_cfg.bind_client_addr);

    SIRI_CFG_read_addr(
            cfgparser,
            "bind_server_address",
            &siri_cfg.bind_backend_addr);

    SIRI_CFG_read_pipe_support(cfgparser);

    if (siri_cfg.pipe_support)
    {
        SIRI_CFG_read_pipe_client_name(cfgparser);
    }

    tmp = siri_cfg.buffer_sync_interval;
    SIRI_CFG_read_uint(
            cfgparser,
            "buffer_sync_interval",
            0,
            300000,
            &tmp);
    siri_cfg.buffer_sync_interval = (uint32_t) tmp;

    SIRI_CFG_ignore_broken_data(cfgparser);

    cfgparser_free(cfgparser);
}

void siri_cfg_destroy(siri_t * siri)
{
    free(siri->cfg->bind_backend_addr);
    free(siri->cfg->bind_client_addr);
}

static void SIRI_CFG_read_uint(
        cfgparser_t * cfgparser,
        const char * option_name,
        int min,
        int max,
        uint32_t * value)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                option_name);

    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing '%s' in '%s' (%s). ",
                option_name,
                siri.args->config,
                cfgparser_errmsg(rc));
        return;
    }

    if (option->tp != CFGPARSER_TP_INTEGER)
    {
        log_warning(
                "Error reading '%s' in '%s': %s.",
                option_name,
                siri.args->config,
                "error: expecting an integer value");
        return;
    }

    if (option->val->integer < min || option->val->integer > max)
    {
        log_warning(
                "Error reading '%s' in '%s': "
                "error: value should be between %d and %d but got %d.",
                option_name,
                siri.args->config,
                min,
                max,
                option->val->integer);
        return;
    }
    *value = option->val->integer;
}

static void SIRI_CFG_read_ip_support(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "ip_support");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing 'ip_support' in '%s' (%s).",
                siri.args->config,
                cfgparser_errmsg(rc));
    }
    else if (option->tp != CFGPARSER_TP_STRING)
    {
        log_warning(
                "Error reading 'ip_support' in '%s': %s.",
                siri.args->config,
                "error: expecting a string value");
    }
    else
    {
        if (strcmp(option->val->string, "ALL") == 0)
        {
            siri_cfg.ip_support = IP_SUPPORT_ALL;
        }
        else if (strcmp(option->val->string, "IPV4ONLY") == 0)
        {
            siri_cfg.ip_support = IP_SUPPORT_IPV4ONLY;
        }
        else if (strcmp(option->val->string, "IPV6ONLY") == 0)
        {
            siri_cfg.ip_support = IP_SUPPORT_IPV6ONLY;
        }
        else
        {
            log_warning(
                    "Error reading 'ip_support' in '%s': "
                    "error: expecting ALL, IPV4ONLY or IPV6ONLY but got '%s'.",
                    siri.args->config,
                    option->val->string);
        }
    }
}

static void SIRI_CFG_read_shard_compression(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "enable_shard_compression");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing 'enable_shard_compression' in '%s' (%s).",
                siri.args->config,
                cfgparser_errmsg(rc));
    }
    else if (option->tp != CFGPARSER_TP_INTEGER || option->val->integer > 1)
    {
        log_warning(
                "Error reading 'enable_shard_compression' in '%s': %s.",
                siri.args->config,
                "error: expecting 0 or 1");
    }
    else if (option->val->integer == 1)
    {
        siri_cfg.shard_compression = 1;
    }
}

static void SIRI_CFG_read_shard_auto_duration(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "enable_shard_auto_duration");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing 'enable_shard_auto_duration' in '%s' (%s).",
                siri.args->config,
                cfgparser_errmsg(rc));
    }
    else if (option->tp != CFGPARSER_TP_INTEGER || option->val->integer > 1)
    {
        log_warning(
                "Error reading 'enable_shard_auto_duration' in '%s': %s.",
                siri.args->config,
                "error: expecting 0 or 1");
    }
    else if (option->val->integer == 1)
    {
        siri_cfg.shard_auto_duration = 1;
    }
}

static void SIRI_CFG_read_pipe_support(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "enable_pipe_support");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing 'enable_pipe_support' in '%s' (%s).",
                siri.args->config,
                cfgparser_errmsg(rc));
    }
    else if (option->tp != CFGPARSER_TP_INTEGER || option->val->integer > 1)
    {
        log_warning(
                "Error reading 'enable_pipe_support' in '%s': %s.",
                siri.args->config,
                "error: expecting 0 or 1");
    }
    else if (option->val->integer == 1)
    {
        siri_cfg.pipe_support = 1;
    }
}

static void SIRI_CFG_ignore_broken_data(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
        &option,
        cfgparser,
        "siridb",
        "ignore_broken_data");
    if (rc != CFGPARSER_SUCCESS)
    {
        return; // optional config option
    }
    else if (option->tp != CFGPARSER_TP_INTEGER || option->val->integer > 1)
    {
        log_warning(
            "Error reading 'ignore_broken_data' in '%s': %s.",
            siri.args->config,
            "error: expecting 0 or 1");
    }
    else if (option->val->integer == 1)
    {
        siri_cfg.ignore_broken_data = 1;
    }
}

static void SIRI_CFG_read_addr(
        cfgparser_t * cfgparser,
        const char * option_name,
        char ** dest)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    struct in_addr sa;
    struct in6_addr sa6;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                option_name);
    if (rc != CFGPARSER_SUCCESS)
    {
        return;
    }
    if (option->tp != CFGPARSER_TP_STRING)
    {
        log_error(
                "Error reading '%s' in '%s': %s. ",
                option_name,
                siri.args->config,
                "error: expecting a string value");
        return;
    }
    if (!inet_pton(AF_INET, option->val->string, &sa) &&
        !inet_pton(AF_INET6, option->val->string, &sa6))
    {
        log_error(
                "Error reading '%s' in '%s': %s. ",
                option_name,
                siri.args->config,
                "error: expecting a valid IPv4 or IPv6 address");
        return;
    }
    *dest = strdup(option->val->string);
    if (!(*dest))
    {
        log_error("Error allocating memory for address.");
    }
}

static void SIRI_CFG_read_pipe_client_name(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    size_t len;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "pipe_client_name");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Missing 'pipe_client_name' in '%s' (%s).",
                siri.args->config,
                cfgparser_errmsg(rc));
        return;
    }

    if (option->tp != CFGPARSER_TP_STRING)
    {
        log_warning(
                "Error reading 'pipe_client_name' in '%s': %s.",
                siri.args->config,
                "error: expecting a string value");
        return;
    }

    len = strlen(option->val->string);
    if (len > XPATH_MAX-2)
    {
        log_warning(
                "Pipe client name exceeds %d characters, please "
                "check your configuration file: %s.",
                XPATH_MAX-2,
                siri.args->config);
        return;
    }

    if (len == 0)
    {
        log_warning(
                "Pipe client should not be an empty string, please "
                "check your configuration file: %s.",
                siri.args->config);
        return;
    }

    strcpy(siri_cfg.pipe_client_name, option->val->string);
}

static void SIRI_CFG_read_db_path(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "db_path");

    if (rc != CFGPARSER_SUCCESS)
    {
        /* Fall-back using the old configuration name */
        rc = cfgparser_get_option(
                    &option,
                    cfgparser,
                    "siridb",
                    "default_db_path");

        if (rc != CFGPARSER_SUCCESS)
        {
            return;
        }
    }

    if (option->tp != CFGPARSER_TP_STRING)
    {
        log_warning(
                "Error reading 'db_path' in '%s': %s.",
                siri.args->config,
                "error: expecting a string value");
        return;
    }

    if (strlen(option->val->string) >= XPATH_MAX)
    {
        log_warning(
                "Error reading 'db_path' in '%s': %s.",
                siri.args->config,
                "error: path too long");
        return;
    }

    strncpy(siri_cfg.db_path, option->val->string, XPATH_MAX);
}

static void SIRI_CFG_read_max_open_files(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;

    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "max_open_files");
    if (rc != CFGPARSER_SUCCESS || option->tp != CFGPARSER_TP_INTEGER)
    {
        return;
    }

    siri_cfg.max_open_files = (uint16_t) option->val->integer;
}

/*
 * Note that address_pt must have a size of at least SIRI_CFG_MAX_LEN_ADDRESS.
 */
static void SIRI_CFG_read_address_port(
        cfgparser_t * cfgparser,
        const char * option_name,
        char * address_pt,
        uint16_t * port_pt)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;

    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                option_name);
    if (rc != CFGPARSER_SUCCESS)
    {
        return;
    }

    if (option->tp != CFGPARSER_TP_STRING)
    {
        log_critical(
                "Error reading '%s' in '%s': %s. ",
                option_name,
                siri.args->config,
                "error: expecting a string value");
        exit(EXIT_FAILURE);
        return;
    }

    if (sirinet_extract_addr_port(option->val->string, address_pt, port_pt))
    {
        exit(EXIT_FAILURE);
        return;
    }
}
