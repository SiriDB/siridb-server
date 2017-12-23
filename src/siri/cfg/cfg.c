#include <cfgparser/cfgparser.h>
#include <inttypes.h>
#include <limits.h>
#include <logger/logger.h>
#include <siri/cfg/cfg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strextra/strextra.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <siri/net/socket.h>

/* do not use more than x percent for the max limit for open sharding files */
#define RLIMIT_PERC_FOR_SHARDING 0.5

#define MAX_OPEN_FILES_LIMIT 32768
#define MIN_OPEN_FILES_LIMIT 3
#define DEFAULT_OPEN_FILES_LIMIT MAX_OPEN_FILES_LIMIT

static siri_cfg_t siri_cfg = {
        .listen_client_port=9000,
        .listen_backend_port=9010,
        .bind_client_addr=NULL,
        .bind_backend_addr=NULL,
        .heartbeat_interval=30,
        .max_open_files=DEFAULT_OPEN_FILES_LIMIT,
        .optimize_interval=3600,
        .ip_support=IP_SUPPORT_ALL,
        .shard_compression=0,
        .server_address="localhost",
        .default_db_path="/var/lib/siridb/"
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
static void SIRI_CFG_read_default_db_path(cfgparser_t * cfgparser);
static void SIRI_CFG_read_max_open_files(cfgparser_t * cfgparser);
static void SIRI_CFG_read_ip_support(cfgparser_t * cfgparser);
static void SIRI_CFG_read_shard_compression(cfgparser_t * cfgparser);

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
        log_critical(
                "Could not read '%s': %s\n",
                siri->args->config,
                cfgparser_errmsg(rc));
        cfgparser_free(cfgparser);
        exit(EXIT_FAILURE);
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

    SIRI_CFG_read_default_db_path(cfgparser);
    SIRI_CFG_read_max_open_files(cfgparser);
    SIRI_CFG_read_ip_support(cfgparser);
    SIRI_CFG_read_shard_compression(cfgparser);

    SIRI_CFG_read_addr(
            cfgparser,
            "bind_client_address",
            &siri_cfg.bind_client_addr);

    SIRI_CFG_read_addr(
            cfgparser,
            "bind_server_address",
            &siri_cfg.bind_backend_addr);

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
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%u'",
                option_name,
                siri.args->config,
                cfgparser_errmsg(rc),
                *value);
    }
    else if (option->tp != CFGPARSER_TP_INTEGER)
    {
        log_warning(
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%u'",
                option_name,
                siri.args->config,
                "error: expecting an integer value",
                *value);
    }
    else if (option->val->integer < min || option->val->integer > max)
    {
        log_warning(
                "Error reading '%s' in '%s': "
                "error: value should be between %d and %d but got %d. "
                "Using default value: '%u'",
                option_name,
                siri.args->config,
                min,
                max,
                option->val->integer,
                *value);
    }
    else
    {
        *value = option->val->integer;
    }
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
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%s'",
                "ip_support",
                siri.args->config,
                cfgparser_errmsg(rc),
                sirinet_socket_ip_support_str(siri_cfg.ip_support));
    }
    else if (option->tp != CFGPARSER_TP_STRING)
    {
        log_warning(
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%s'",
                "ip_support",
                siri.args->config,
                "error: expecting a string value",
                sirinet_socket_ip_support_str(siri_cfg.ip_support));
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
                    "Error reading '%s' in '%s': "
                    "error: expecting ALL, IPV4ONLY or IPV6ONLY but got '%s'. "
                    "Using default value: '%s'",
                    "ip_support",
                    siri.args->config,
                    option->val->string,
                    sirinet_socket_ip_support_str(siri_cfg.ip_support));
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
        log_debug(
                "Missing '%s' in '%s': %s. "
                "Disable shard compression",
                "enable_shard_compression",
                siri.args->config,
                cfgparser_errmsg(rc));
    }
    else if (option->tp != CFGPARSER_TP_INTEGER || option->val->integer > 1)
    {
        log_warning(
                "Error reading '%s' in '%s': %s. "
                "Disable shard compression",
                "enable_shard_compression",
                siri.args->config,
                "error: expecting 0 or 1");
    }
    else if (option->val->integer == 1)
    {
        siri_cfg.shard_compression = 1;
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

static void SIRI_CFG_read_default_db_path(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    size_t len;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "default_db_path");
    if (rc != CFGPARSER_SUCCESS)
    {
        log_warning(
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%s'",
                "default_db_path",
                siri.args->config,
                cfgparser_errmsg(rc),
                siri_cfg.default_db_path);
    }
    else if (option->tp != CFGPARSER_TP_STRING)
    {
        log_warning(
                "Error reading '%s' in '%s': %s. "
                "Using default value: '%s'",
                "default_db_path",
                siri.args->config,
                "error: expecting a string value",
                siri_cfg.default_db_path);
    }
    else
    {
        if (strlen(option->val->string) >= SIRI_PATH_MAX -2 ||
            realpath(
                option->val->string,
                siri_cfg.default_db_path) == NULL)
        {
            log_warning(
                    "Could not resolve default database path: %s, please "
                    "check your configuration file: %s",
                    option->val->string,
                    siri.args->config);
            memset(siri_cfg.default_db_path, 0, SIRI_PATH_MAX);

            /* keep space left for a trailing slash and a terminator char */
            strncpy(siri_cfg.default_db_path,
                    option->val->string,
                    SIRI_PATH_MAX - 2);
        }

        len = strlen(siri_cfg.default_db_path);

        if (len == SIRI_PATH_MAX - 2)
        {
            log_warning(
                    "Default database path exceeds %d characters, please "
                    "check your configuration file: %s",
                    SIRI_PATH_MAX - 3,
                    siri.args->config);
        }

        /* add trailing slash (/) if its not already there */
        if (siri_cfg.default_db_path[len - 1] != '/')
        {
            siri_cfg.default_db_path[len] = '/';
        }
    }
}

static void SIRI_CFG_read_max_open_files(cfgparser_t * cfgparser)
{
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    struct rlimit rlim;
    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                "max_open_files");
    if (rc != CFGPARSER_SUCCESS || option->tp != CFGPARSER_TP_INTEGER)
    {
        log_info(
                "Using default value for max_open_files: %d",
                siri_cfg.max_open_files);
    }
    else
    {
        siri_cfg.max_open_files = (uint16_t) option->val->integer;
    }

    if (siri_cfg.max_open_files < MIN_OPEN_FILES_LIMIT ||
            siri_cfg.max_open_files > MAX_OPEN_FILES_LIMIT)
    {
        log_warning(
                "Value max_open_files must be a value between %d and %d "
                "but we found %d. Using default value instead: %d",
                MIN_OPEN_FILES_LIMIT, MAX_OPEN_FILES_LIMIT,
                siri_cfg.max_open_files, DEFAULT_OPEN_FILES_LIMIT);
        siri_cfg.max_open_files = DEFAULT_OPEN_FILES_LIMIT;
    }

    getrlimit(RLIMIT_NOFILE, &rlim);

    uint16_t min_limit = (uint16_t)
            ((double) siri_cfg.max_open_files / RLIMIT_PERC_FOR_SHARDING) -1;

    if (min_limit > (uint64_t) rlim.rlim_max)
    {
        siri_cfg.max_open_files =
                (uint16_t) ((double) rlim.rlim_max * RLIMIT_PERC_FOR_SHARDING);
        log_warning(
                "We want to set a max-open-files value which "
                "exceeds %d%% of the current hard limit.\n\nWe "
                "will use %d as max_open_files for now.\n"
                "Please increase the hard-limit using:\n"
                "ulimit -Hn %d",
                (uint8_t) (RLIMIT_PERC_FOR_SHARDING * 100),
                siri_cfg.max_open_files,
                min_limit);
        min_limit = siri_cfg.max_open_files * 2;
    }

    if (min_limit > (uint64_t) rlim.rlim_cur)
    {
        log_info(
                "Increasing soft-limit from %d to %d since we want "
                "to use only %d%% from the soft-limit for shard files",
                (uint64_t) rlim.rlim_cur,
                min_limit,
                (uint8_t) (RLIMIT_PERC_FOR_SHARDING * 100));
        rlim.rlim_cur = min_limit;
        if (setrlimit(RLIMIT_NOFILE, &rlim))
        {
            log_critical("Could not set the soft-limit to %d", min_limit);
            exit(EXIT_FAILURE);
        }
    }
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
    char * port;
    char * address;
    char hostname[SIRI_CFG_MAX_LEN_ADDRESS];
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    int test_port;

    if (gethostname(hostname, SIRI_CFG_MAX_LEN_ADDRESS))
    {
        log_debug(
                "Unable to read the systems host name. Since its only purpose "
                "is to apply this in the configuration file this might not be "
                "any problem. (using 'localhost' as fallback)");
        strcpy(hostname, "localhost");
    }

    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                option_name);
    if (rc != CFGPARSER_SUCCESS)
    {
        log_critical(
                "Error reading '%s' in '%s': %s.",
                option_name,
                siri.args->config,
                cfgparser_errmsg(rc));
        exit(EXIT_FAILURE);
    }
    else if (option->tp != CFGPARSER_TP_STRING)
    {
        log_critical(
                "Error reading '%s' in '%s': %s. ",
                option_name,
                siri.args->config,
                "error: expecting a string value");
        exit(EXIT_FAILURE);
    }
    else
    {
        if (*option->val->string == '[')
        {
            /* an IPv6 address... */
            for (port = address = option->val->string + 1; *port; port++)
            {
                if (*port == ']')
                {
                    *port = 0;
                    port++;
                    break;
                }
            }
        }
        else
        {
            port = address = option->val->string;
        }

        for (; *port; port++)
        {
            if (*port == ':')
            {
                *port = 0;
                port++;
                break;
            }
        }

        if (    !strlen(address) ||
                strlen(address) >= SIRI_CFG_MAX_LEN_ADDRESS ||
                !strx_is_int(port) ||
                strcpy(address_pt, address) == NULL ||
                strx_replace_str(
                        address_pt,
                        "%HOSTNAME",
                        hostname,
                        SIRI_CFG_MAX_LEN_ADDRESS))
        {
            log_critical(
                    "Error reading '%s' in '%s': "
                    "error: got an unexpected value '%s:%s'.",
                    option_name,
                    siri.args->config,
                    address,
                    port);
            exit(EXIT_FAILURE);
        }
        else
        {
            test_port = atoi(port);

            if (test_port < 1 || test_port > 65535)
            {
                log_critical(
                        "Error reading '%s' in '%s': "
                        "error: port should be between 1 and 65535, got '%d'.",
                        option_name,
                        siri.args->config,
                        test_port);
                exit(EXIT_FAILURE);
            }
            else
            {
                *port_pt = (uint16_t) test_port;
            }

            log_debug("Read '%s' from config: %s:%d",
                    option_name,
                    address_pt,
                    *port_pt);
        }

    }
}
