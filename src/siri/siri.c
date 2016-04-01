/*
 * siri.h - global methods for SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <siri/siri.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <logger/logger.h>
#include <siri/args/args.h>
#include <siri/net/clserver.h>
#include <siri/net/handle.h>
#include <siri/db/listener.h>
#include <siri/db/props.h>
#include <siri/db/server.h>
#include <siri/db/series.h>
#include <siri/db/buffer.h>
#include <strextra/strextra.h>
#include <siri/cfg/cfg.h>
#include <cfgparser/cfgparser.h>
#include <sys/stat.h>
#include <unistd.h>
#include <qpack/qpack.h>



static void signal_handler(uv_signal_t * req, int signum);
static int siridb_load_databases(void);
static void walk_close_handlers(uv_handle_t * handle, void * arg);

siri_t siri = {
        .grammar=NULL,
        .loop=NULL,
        .siridb_list=NULL
};

void siri_setup_logger(void)
{
    int n;
    char lname[255];
    size_t len = strlen(siri_args.log_level);

    for (n = 0; n < LOGGER_NUM_LEVELS; n++)
    {
        strcpy(lname, LOGGER_LEVEL_NAMES[n]);
        lower_case(lname);
        if (strlen(lname) == len && strcmp(siri_args.log_level, lname) == 0)
        {
            logger_init(stdout, (n + 1) * 10);
            return;
        }
    }
    /* We should not get here since args should always
     * contain a valid log level
     */
    logger_init(stdout, 10);
}

static int siridb_load_databases(void)
{
    struct stat st = {0};
    DIR * db_container_path;
    struct dirent * dbpath;
    char buffer[SIRI_CFG_MAX_LEN_PATH];
    cfgparser_return_t rc;
    cfgparser_t * cfgparser = NULL;
    qp_unpacker_t * unpacker = NULL;
    cfgparser_option_t * option = NULL;
    siridb_t * siridb;
    size_t len;

    char err_msg[512];

    if (stat(siri_cfg.default_db_path, &st) == -1)
    {
        log_warning("Database directory not found, creating directory '%s'.",
                siri_cfg.default_db_path);
        if (mkdir(siri_cfg.default_db_path, 0700) == -1)
        {
            log_error("Cannot create director '%s'.",
                    siri_cfg.default_db_path);
            return 1;
        }
    }

    if ((db_container_path = opendir(siri_cfg.default_db_path)) == NULL)
    {
        log_error("Cannot open database directory '%s'.",
                siri_cfg.default_db_path);
        return 1;
    }

    while((dbpath = readdir(db_container_path)) != NULL)
    {
        struct stat st;

        if ((strlen(dbpath->d_name) == 1 &&
                    strcmp(dbpath->d_name, ".") == 0) ||
                (strlen(dbpath->d_name) >= 2 &&
                        (strncmp(dbpath->d_name, "..", 2) == 0 ||
                         strncmp(dbpath->d_name, "__", 2) == 0)))
            /* skip "." ".." and prefixed with double underscore directories */
            continue;

        if (fstatat(dirfd(db_container_path), dbpath->d_name, &st, 0) < 0)
            continue;

        if (!S_ISDIR(st.st_mode))
            continue;

        /* read database.conf */
        snprintf(buffer,
                SIRI_CFG_MAX_LEN_PATH,
                "%s%s/database.conf",
                siri_cfg.default_db_path,
                dbpath->d_name);

        if (access(buffer, R_OK) == -1)
            continue;

        cfgparser = cfgparser_new();

        if ((rc = cfgparser_read(cfgparser, buffer)) != CFGPARSER_SUCCESS)
        {
            log_error("Could not read '%s': %s",
                    buffer,
                    cfgparser_errmsg(rc));
            closedir(db_container_path);
            cfgparser_free(cfgparser);
            return 1;
        }

        snprintf(buffer,
                SIRI_CFG_MAX_LEN_PATH,
                "%s%s/database.dat",
                siri_cfg.default_db_path,
                dbpath->d_name);

        if ((unpacker = qp_from_file_unpacker(buffer)) == NULL)
        {
            log_error("Could not read '%s'", buffer);
            closedir(db_container_path);
            qp_free_unpacker(unpacker);
            cfgparser_free(cfgparser);
            return 1;
        }

        if (siridb_add_from_unpacker(
                siri.siridb_list,
                unpacker,
                &siridb,
                err_msg))
        {
            log_error("Could not read '%s': %s", buffer, err_msg);
            closedir(db_container_path);
            qp_free_unpacker(unpacker);
            cfgparser_free(cfgparser);
            return 1;
        }

        qp_free_unpacker(unpacker);

        /* set dbpath */
        snprintf(buffer,
                SIRI_CFG_MAX_LEN_PATH,
                "%s%s/",
                siri_cfg.default_db_path,
                dbpath->d_name);

        len = strlen(buffer);
        siridb->dbpath = (char *) malloc(len + 1);
        memcpy(siridb->dbpath, buffer, len);
        siridb->dbpath[len] = 0;

        /* read buffer_path from database.conf */
        rc = cfgparser_get_option(
                    &option,
                    cfgparser,
                    "buffer",
                    "buffer_path");
        if (rc == CFGPARSER_SUCCESS && option->tp == CFGPARSER_TP_STRING)
        {
            len = strlen(option->val->string) + 1;
            siridb->buffer_path = (char *) malloc(len);
            memcpy(siridb->buffer_path, option->val->string, len);
        }
        else
            siridb->buffer_path = siridb->dbpath;

        /* free cfgparser */
        cfgparser_free(cfgparser);

        /* load users */
        if (siridb_load_users(siridb))
        {
            log_error("Could not read users for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load servers */
        if (siridb_load_servers(siridb))
        {
            log_error("Could not read servers for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load series */
        if (siridb_load_series(siridb))
        {
            log_error("Could not read series for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load buffer */
        if (siridb_load_buffer(siridb))
        {
            log_error("Could not read buffer for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* open buffer */
        if (siridb_open_buffer(siridb))
        {
            log_error("Could not open buffer for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* generate pools */
        siridb_gen_pools(siridb);

        siridb->start_ts = (uint32_t) time(NULL);
    }
    closedir(db_container_path);

    return 0;
}

int siri_start(void)
{
    int rc;
    uv_signal_t sig;

    /* initialize listener (set enter and exit functions) */
    siridb_init_listener();

    /* initialize props (set props functions) */
    siridb_init_props();

    /* load SiriDB grammar */
    siri.grammar = compile_grammar();

    /* create store for SiriDB instances */
    siri.siridb_list = siridb_new_list();

    /* load databases */
    if ((rc = siridb_load_databases()))
        return rc; //something went wrong

    /* initialize the default event loop */
    siri.loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(siri.loop);

    /* bind signal to the event loop */
    uv_signal_init(siri.loop, &sig);
    uv_signal_start(&sig, signal_handler, SIGINT);

    /* initialize the client server */
    if ((rc = sirinet_clserver_init(siri.loop)))
        return rc; // something went wrong

    /* start the event loop */
    uv_run(siri.loop, UV_RUN_DEFAULT);

    /* quit, don't forget to run siri_free() (should be done in main) */
    return 0;
}

void siri_free(void)
{
    int rc;
    rc = uv_loop_close(siri.loop);
    if (rc) // could be UV_EBUSY in case handlers are not closed yet
        log_error("Error occurred while closing the event loop: %d", rc);
    free(siri.loop);
    free(siri.grammar);
    siridb_free_list(siri.siridb_list);
}

static void signal_handler(uv_signal_t * req, int signum)
{
    log_debug("You pressed CTRL+C, let's stop the event loop..");
    uv_stop(siri.loop);

    /* close open handlers */
    uv_walk(siri.loop, walk_close_handlers, NULL);

    /* run the loop once more so call-backs on uv_close() can run */
    uv_run(siri.loop, UV_RUN_DEFAULT);
}

static void walk_close_handlers(uv_handle_t * handle, void * arg)
{
    switch (handle->type)
    {
    case UV_SIGNAL:
        uv_close(handle, NULL);
        break;
    case UV_TCP:
        /* TCP server has data set to NULL but
         * clients use data and should be freed.
         */
        uv_close(handle, (handle->data == NULL) ? NULL : sirinet_free_client);
        break;
    default:
        log_error("Oh oh, we need to implement type %d", handle->type);
    }
}
