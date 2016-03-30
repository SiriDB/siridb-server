#include <siri/cfg/cfg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <siri/args/args.h>
#include <cfgparser/cfgparser.h>
#include <logger/logger.h>
#include <strextra/strextra.h>

siri_cfg_t siri_cfg = {
        .listen_client_address="localhost",
        .listen_client_port=9000,
        .listen_backend_address="localhost",
        .listen_backend_port=9010,
        .default_db_path="/var/lib/siridb/"
};

static void siri_cfg_read_address_port(
        cfgparser_t * cfgparser,
        const char * option_name,
        char * address_pt,
        uint16_t * port_pt);
static void siri_cfg_read_default_db_path(cfgparser_t * cfgparser);

void siri_cfg_init(void)
{
    /* Read the application configuration file. */
    cfgparser_t * cfgparser = cfgparser_new();
    cfgparser_return_t rc;

    rc = cfgparser_read(cfgparser, siri_args.config);
    if (rc != CFGPARSER_SUCCESS)
    {
        /* we could choose to continue with defaults but this is probably
         * not what users want so lets quit.
         */
        fprintf(stderr,
                "Could not read '%s': %s\n",
                siri_args.config,
                cfgparser_errmsg(rc));
        cfgparser_free(cfgparser);
        exit(EXIT_FAILURE);
    }

    siri_cfg_read_address_port(
            cfgparser,
            "listen_client",
            siri_cfg.listen_client_address,
            &siri_cfg.listen_client_port);

    siri_cfg_read_address_port(
            cfgparser,
            "listen_backend",
            siri_cfg.listen_backend_address,
            &siri_cfg.listen_backend_port);

    siri_cfg_read_default_db_path(cfgparser);

    cfgparser_free(cfgparser);
}

static void siri_cfg_read_default_db_path(cfgparser_t * cfgparser)
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
        log_warning(
                "Error reading '[siridb]%s' in '%s': %s. "
                "Using default value: '%s'",
                "default_db_path",
                siri_args.config,
                cfgparser_errmsg(rc),
                siri_cfg.default_db_path);
    else if (option->tp != CFGPARSER_TP_STRING)
        log_warning(
                "Error reading '[siridb]%s' in '%s': %s. "
                "Using default value: '%s:%d'",
                "default_db_path",
                siri_args.config,
                "error: expecting a string value",
                siri_cfg.default_db_path);
    else
    {
        strncpy(siri_cfg.default_db_path,
                option->val->string,
                SIRI_CFG_MAX_LEN_PATH - 1);
        len = strlen(siri_cfg.default_db_path);

        /* add trailing slash (/) if its not already there */
        if (siri_cfg.default_db_path[len - 1] != '/')
        {
            siri_cfg.default_db_path[len] = '/';
            siri_cfg.default_db_path[len + 1] = 0;
        }
    }
}

static void siri_cfg_read_address_port(
        cfgparser_t * cfgparser,
        const char * option_name,
        char * address_pt,
        uint16_t * port_pt)
{
    char * port;
    char * address;
    cfgparser_option_t * option;
    cfgparser_return_t rc;
    uint16_t test_port;

    rc = cfgparser_get_option(
                &option,
                cfgparser,
                "siridb",
                option_name);
    if (rc != CFGPARSER_SUCCESS)
        log_warning(
                "Error reading '[siridb]%s' in '%s': %s. "
                "Using default value: '%s:%d'",
                option_name,
                siri_args.config,
                cfgparser_errmsg(rc),
                address_pt,
                *port_pt);
    else if (option->tp != CFGPARSER_TP_STRING)
        log_warning(
                "Error reading '[siridb]%s' in '%s': %s. "
                "Using default value: '%s:%d'",
                option_name,
                siri_args.config,
                "error: expecting a string value",
                address_pt,
                *port_pt);
    else
    {
        for (port = address = option->val->string; *port; port++)
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
                !is_int(port)
                )
            log_warning(
                    "Error reading '[siridb]%s' in '%s': "
                    "error: got an unexpected value '%s:%s'. "
                    "Using default value: '%s:%d'",
                    option_name,
                    siri_args.config,
                    address,
                    port,
                    address_pt,
                    *port_pt);
        else
        {
            if (*address == '*')
                strcpy(address_pt, "0.0.0.0");
            else
                strcpy(address_pt, address);

            test_port = atoi(port);

            if (test_port < 1 || test_port > 65535)
                log_warning(
                        "Error reading '[siridb]%s' in '%s': "
                        "error: port should be between 1 and 65535, got '%d'. "
                        "Using default value: '%d'",
                        option_name,
                        siri_args.config,
                        test_port,
                        *port_pt);
            else
                *port_pt = test_port;

            log_debug("Read '%s' from config: %s:%d",
                    option_name,
                    address_pt,
                    *port_pt);
        }

    }
}
