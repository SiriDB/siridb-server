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
#include <siri/net/clserver.h>
#include <siri/net/bserver.h>
#include <siri/parser/listener.h>
#include <siri/db/props.h>
#include <siri/db/users.h>
#include <siri/db/servers.h>
#include <siri/db/series.h>
#include <siri/db/shards.h>
#include <siri/db/buffer.h>
#include <siri/db/aggregate.h>
#include <siri/db/pools.h>
#include <strextra/strextra.h>
#include <siri/cfg/cfg.h>
#include <cfgparser/cfgparser.h>
#include <sys/stat.h>
#include <unistd.h>
#include <qpack/qpack.h>
#include <assert.h>
#include <siri/net/socket.h>
#include <siri/version.h>

static void SIRI_signal_handler(uv_signal_t * req, int signum);
static int SIRI_load_databases(void);
static void SIRI_close_handlers(void);
static void SIRI_walk_close_handlers(uv_handle_t * handle, void * arg);
static void SIRI_destroy(void);
static void SIRI_set_running_state(void);
static void SIRI_set_closing_state(void);
static void SIRI_try_close(uv_timer_t * handle);
static void SIRI_walk_try_close(uv_handle_t * handle, int * num);

#define WAIT_BETWEEN_CLOSE_ATTEMPTS 3000

static uv_timer_t closing_timer;
static int closing_attempts = 20;

#define N_SIGNALS 3
static int signals[N_SIGNALS] = {SIGINT, SIGTERM, SIGSEGV};

siri_t siri = {
        .grammar=NULL,
        .loop=NULL,
        .siridb_list=NULL,
        .fh=NULL,
        .optimize=NULL,
        .heartbeat=NULL,
        .cfg=NULL,
        .args=NULL,
        .status=SIRI_STATUS_LOADING,
        .startup_time=0
};


void siri_setup_logger(void)
{
    int n;
    char lname[255];
    size_t len = strlen(siri.args->log_level);

#ifndef DEBUG
    /* force colors while debugging... */
    if (siri.args->log_colorized)
#endif
    {
        Logger.flags |= LOGGER_FLAG_COLORED;
    }

    for (n = 0; n < LOGGER_NUM_LEVELS; n++)
    {
        strcpy(lname, LOGGER_LEVEL_NAMES[n]);
        strx_lower_case(lname);
        if (strlen(lname) == len && strcmp(siri.args->log_level, lname) == 0)
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

static int SIRI_load_databases(void)
{
    struct stat st = {0};
    DIR * db_container_path;
    struct dirent * dbpath;
    char buffer[PATH_MAX];
    cfgparser_return_t rc;
    cfgparser_t * cfgparser = NULL;
    qp_unpacker_t * unpacker = NULL;
    cfgparser_option_t * option = NULL;
    siridb_t * siridb;

    char err_msg[512];

    if (stat(siri.cfg->default_db_path, &st) == -1)
    {
        log_warning("Database directory not found, creating directory '%s'.",
                siri.cfg->default_db_path);
        if (mkdir(siri.cfg->default_db_path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.",
                    siri.cfg->default_db_path);
            return 1;
        }
    }

    if ((db_container_path = opendir(siri.cfg->default_db_path)) == NULL)
    {
        log_error("Cannot open database directory '%s'.",
                siri.cfg->default_db_path);
        return 1;
    }

    while((dbpath = readdir(db_container_path)) != NULL)
    {
        struct stat st = {0};

        if (    strcmp(dbpath->d_name, ".") == 0 ||
                strcmp(dbpath->d_name, "..") == 0 ||
                strncmp(dbpath->d_name, "__", 2) == 0)
        {
            /* skip "." ".." and prefixed with double underscore directories */
            continue;
        }

        if (fstatat(dirfd(db_container_path), dbpath->d_name, &st, 0) < 0)
        {
            continue;
        }

        if (!S_ISDIR(st.st_mode))
        {
            continue;
        }

        /* read database.conf */
        snprintf(buffer,
                PATH_MAX,
                "%s%s/database.conf",
                siri.cfg->default_db_path,
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
                PATH_MAX,
                "%s%s/database.dat",
                siri.cfg->default_db_path,
                dbpath->d_name);

        if ((unpacker = qp_from_file_unpacker(buffer)) == NULL)
        {
            log_error("Could not read '%s'", buffer);
            closedir(db_container_path);
            qp_free_unpacker(unpacker);
            cfgparser_free(cfgparser);
            return 1;
        }

        if (siridb_from_unpacker(
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

        /* append SiriDB to siridb_list and increment reference count */
        llist_append(siri.siridb_list, siridb);
        siridb_incref(siridb);

        qp_free_unpacker(unpacker);

        log_info("Start loading database: '%s'", siridb->dbname);

        /* set dbpath */
        snprintf(buffer,
                PATH_MAX,
                "%s%s/",
                siri.cfg->default_db_path,
                dbpath->d_name);

        siridb->dbpath = strdup(buffer);

        /* read buffer_path from database.conf */
        rc = cfgparser_get_option(
                    &option,
                    cfgparser,
                    "buffer",
                    "buffer_path");

        siridb->buffer_path = (
                rc == CFGPARSER_SUCCESS &&
                option->tp == CFGPARSER_TP_STRING) ?
                        strdup(option->val->string) : siridb->dbpath;

        /* free cfgparser */
        cfgparser_free(cfgparser);

        /* load users */
        if (siridb_users_load(siridb))
        {
            log_error("Could not read users for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load servers */
        if (siridb_servers_load(siridb))
        {
            log_error("Could not read servers for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load series */
        if (siridb_series_load(siridb))
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
        if (siridb_buffer_open(siridb))
        {
            log_error("Could not open buffer for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* load shards */
        if (siridb_shards_load(siridb))
        {
            log_error("Could not read shards for database '%s'", siridb->dbname);
            closedir(db_container_path);
            return 1;
        }

        /* generate pools */
        siridb_pools_gen(siridb);

        /* update series props */
        log_info("Updating series properties");
        imap32_walk(
                siridb->series_map,
                (imap32_cb_t) siridb_series_update_props,
                NULL);

        siridb->start_ts = (uint32_t) time(NULL);

        log_info("Finished loading database: '%s'", siridb->dbname);
    }
    closedir(db_container_path);

    return 0;
}

int siri_start(void)
{
    int rc;
    struct timespec start;
    struct timespec end;
    uv_signal_t sig[N_SIGNALS];

    /* get start time so we can calculate the startup_time */
    clock_gettime(CLOCK_REALTIME, &start);

    /* initialize listener (set enter and exit functions) */
    siriparser_init_listener();

    /* initialize props (set props functions) */
    siridb_init_props();

    /* initialize aggregation */
    siridb_init_aggregates();

    /* load SiriDB grammar */
    siri.grammar = compile_grammar();

    /* create store for SiriDB instances */
    siri.siridb_list = llist_new();

    /* initialize file handler for shards */
    siri.fh = siri_fh_new(siri.cfg->max_open_files);

    /* load databases */
    if ((rc = SIRI_load_databases()))
        return rc; //something went wrong

    /* initialize the default event loop */
    siri.loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(siri.loop);

    /* bind signals to the event loop */
    for (int i = 0; i < N_SIGNALS; i++)
    {
        uv_signal_init(siri.loop, &sig[i]);
        uv_signal_start(&sig[i], SIRI_signal_handler, signals[i]);
    }

    /* initialize the back-end server */
    if ((rc = sirinet_bserver_init(&siri)))
    {
        SIRI_close_handlers();
        return rc; // something went wrong
    }

    /* initialize the client server */
    if ((rc = sirinet_clserver_init(&siri)))
    {
        SIRI_close_handlers();
        return rc; // something went wrong
    }

    /* initialize optimize task (bind siri.optimize) */
    siri_optimize_init(&siri);

    /* initialize heart-beat task (bind siri.heartbeat) */
    siri_heartbeat_init(&siri);

    /* update siridb status to running */
    SIRI_set_running_state();

    /* set startup time */
    clock_gettime(CLOCK_REALTIME, &end);
    siri.startup_time = end.tv_sec - start.tv_sec;

    /* start the event loop */
    uv_run(siri.loop, UV_RUN_DEFAULT);

    /* quit, don't forget to run siri_free() (should be done in main) */
    return 0;
}

void siri_free(void)
{
    if (siri.loop != NULL)
    {
        int rc;
        rc = uv_loop_close(siri.loop);
        if (rc) // could be UV_EBUSY (-16) in case handlers are not closed yet
        {
            log_error("Error occurred while closing the event loop: %d", rc);
        }
    }

    /* first free the File Handler. (this will close all open shard files) */
    siri_fh_free(siri.fh);

    /* this will free each SiriDB database and the list */
    llist_free_cb(siri.siridb_list, (llist_cb_t) siridb_free_cb, NULL);

    /* free siridb grammar */
    cleri_free_grammar(siri.grammar);

    /* free event loop */
    free(siri.loop);
}

static void SIRI_destroy(void)
{
    log_info("Closing SiriDB Server (version: %s)", SIRIDB_VERSION);

    /* stop the event loop */
    uv_stop(siri.loop);

    /* use one iteration to close all open handlers */
    SIRI_close_handlers();
}

static void SIRI_set_running_state(void)
{
    siri.status = SIRI_STATUS_RUNNING;

    llist_node_t * db_node = siri.siridb_list->first;
    while (db_node != NULL)
    {
        siridb_server_t * server = ((siridb_t *) db_node->data)->server;
        server->flags |= SERVER_FLAG_RUNNING;
        db_node = db_node->next;
    }
}

static void SIRI_set_closing_state(void)
{
    siri.status = SIRI_STATUS_CLOSING;

    llist_node_t * db_node = siri.siridb_list->first;
    while (db_node != NULL)
    {
        siridb_server_t * server = ((siridb_t *) db_node->data)->server;
        server->flags ^= SERVER_FLAG_RUNNING & server->flags;
        db_node = db_node->next;
    }
}

static void SIRI_walk_try_close(uv_handle_t * handle, int * num)
{
    if (handle->type == UV_ASYNC)
    {
        (*num)++;
    }
}

static void SIRI_try_close(uv_timer_t * handle)
{
    int num = 0;

    uv_walk(siri.loop, (uv_walk_cb) SIRI_walk_try_close, &num);

    if (!--closing_attempts && num)
    {
        log_error("SiriDB will close but still had %d task(s) running.", num);
        num = 0;
    }

    if (!num)
    {
        /* close this timer */
        uv_timer_stop(handle);
        uv_close((uv_handle_t *) handle, NULL);

        /* stop SiriDB */
        SIRI_destroy();
    }
    else
    {
        log_info("SiriDB is closing but is waiting for %d task(s) to "
                "finish...", num);
    }
}

static void SIRI_signal_handler(uv_signal_t * req, int signum)
{
    if (siri.status == SIRI_STATUS_CLOSING)
    {
        log_error("Receive a second signal (%d), stop SiriDB immediately!");
        SIRI_destroy();
    }
    else
    {
        /* stop optimize task */
        siri_optimize_stop(&siri);

        /* stop heart-beat task */
        siri_heartbeat_stop(&siri);

        /* mark SiriDB as closing and remove ONLINE flag from servers. */
        SIRI_set_closing_state();

        if (signum == SIGINT || signum == SIGTERM)
        {
            log_info("Asked SiriDB Server to stop (%d)", signum);

            /* set SiriDB in closing mode and start a timer to check if
             * we can finish open tasks before really closing
             */
            uv_timer_init(siri.loop, &closing_timer);
            uv_timer_start(
                    &closing_timer,
                    SIRI_try_close,
                    0,
                    WAIT_BETWEEN_CLOSE_ATTEMPTS);
        }
        else
        {
            log_critical("Signal (%d) received, stop SiriDB immediately!");
            SIRI_destroy();
        }
    }
}

static void SIRI_walk_close_handlers(uv_handle_t * handle, void * arg)
{
    switch (handle->type)
    {
    case UV_SIGNAL:
        /* this is where we cleanup the signal handlers */
        uv_close(handle, NULL);
        break;

    case UV_TCP:
        /* This can be a TCP server with data set to NULL or a SiriDB socket
         * which should be destroyed.
         */
        uv_close(handle, (handle->data == NULL) ?
                NULL : (uv_close_cb) sirinet_socket_free);
        break;

    case UV_TIMER:
        /* we do not expect any timer object since they should all be closed
         * (or at least closing) at this point.
         */
#ifdef DEBUG
        if (!uv_is_closing(handle))
        {
            log_critical(
                    "Found a non closing Timer, all timers should"
                    "be stopped at this point!!!");
            uv_timer_stop((uv_timer_t *) handle);
            uv_close(handle, NULL);
        }
#endif
        break;

    case UV_ASYNC:
#ifdef DEBUG
        log_critical(
                "An async task is only expected to be found in case"
                "not all tasks were closed within the timeout limit.");
#endif
        uv_close(handle, (uv_close_cb) free);
        break;

    default:

#ifdef DEBUG
        log_critical("Oh oh, we need to implement type %d", handle->type);
        assert(0);
#endif

        break;
    }
}

static void SIRI_close_handlers(void)
{
    /* close open handlers */
    uv_walk(siri.loop, SIRI_walk_close_handlers, NULL);

    /* run the loop once more so call-backs on uv_close() can run */
    uv_run(siri.loop, UV_RUN_DEFAULT);
}
