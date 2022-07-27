/*
 * siri.c - Root for SiriDB.
 *
 *
 * Info siri->siridb_mutex:
 *
 *  Main thread:
 *      siri->siridb_list :    read (no lock)          write (lock)
 *
 *  Other threads:
 *      siri->siridb_list :    read (lock)          write (not allowed)
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/buffersync.h>
#include <siri/cfg/cfg.h>
#include <siri/db/aggregate.h>
#include <siri/db/buffer.h>
#include <siri/db/groups.h>
#include <siri/db/listener.h>
#include <siri/db/pools.h>
#include <siri/db/props.h>
#include <siri/db/series.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/tee.h>
#include <siri/db/users.h>
#include <siri/api.h>
#include <siri/err.h>
#include <siri/health.h>
#include <siri/help/help.h>
#include <siri/net/bserver.h>
#include <siri/net/clserver.h>
#include <siri/net/pipe.h>
#include <siri/net/stream.h>
#include <siri/service/account.h>
#include <siri/service/request.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stddef.h>
#include <stdio.h>
#include <xstr/xstr.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <xpath/xpath.h>


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
static int closing_attempts = 40;  /* times 3 seconds is 2 minutes  */

#define N_SIGNALS 6
static int signals[N_SIGNALS] = {
        SIGHUP,
        SIGINT,
        SIGTERM,
        SIGSEGV,
        SIGABRT,
        SIGPIPE};

siri_t siri = {
        .grammar=NULL,
        .loop=NULL,
        .siridb_list=NULL,
        .fh=NULL,
        .optimize=NULL,
        .heartbeat=NULL,
        .buffersync=NULL,
        .cfg=NULL,
        .args=NULL,
        .status=SIRI_STATUS_LOADING,
        .startup_time=0,
        .accounts=NULL,
        .dbname_regex=NULL,
        .dbname_match_data=NULL,
        .client=NULL};

void siri_setup_logger(void)
{
    int n;
    char lname[255];
    size_t len = strlen(siri.args->log_level);

#ifdef NDEBUG
    /* force colors while debugging... */
    if (siri.args->log_colorized)
#endif
    {
        Logger.flags |= LOGGER_FLAG_COLORED;
    }

    for (n = 0; n < LOGGER_NUM_LEVELS; n++)
    {
        strcpy(lname, logger_level_name(n));
        xstr_lower_case(lname);
        if (strlen(lname) == len && strcmp(siri.args->log_level, lname) == 0)
        {
            logger_init(stdout, n);
            return;
        }
    }

    assert (0);
    /* We should not get here since args should always
     * contain a valid log level
     */
    logger_init(stdout, 0);
}

int make_database_directory(void)
{
    char tmppath[XPATH_MAX];
    char * sysuser = getenv("USER");
    char * homedir = getenv("HOME");
    size_t len;

    memset(tmppath, 0, XPATH_MAX);

    if (*siri.cfg->db_path == '\0')
    {
        if (!homedir || !sysuser || strcmp(sysuser, "root") == 0)
        {
            strcpy(siri.cfg->db_path, "/var/lib/siridb/");
        }
        else
        {
            snprintf(siri.cfg->db_path, XPATH_MAX, "%s%s.siridb/",
                    homedir,
                    homedir[strlen(homedir)-1] == '/' ? "" : "/");
        }
    }

    if (!xpath_is_dir(siri.cfg->db_path))
    {
        log_warning("Database directory not found, creating directory '%s'.",
                siri.cfg->db_path);
        if (mkdir(siri.cfg->db_path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.",
                    siri.cfg->db_path);
            return -1;
        }
    }

    if (realpath(siri.cfg->db_path, tmppath) == NULL)
    {
        log_warning(
                "Could not resolve default database path: %s",
                siri.cfg->db_path);
    }
    else
    {
        memcpy(siri.cfg->db_path, tmppath, sizeof(tmppath));
    }

    len = strlen(siri.cfg->db_path);

    if (len >= XPATH_MAX - 2)
    {
        log_warning(
                "Default database path exceeds %d characters, please "
                "check your configuration file: %s",
                XPATH_MAX - 3,
                siri.args->config);
        return -1;
    }

    /* add trailing slash (/) if its not already there */
    if (siri.cfg->db_path[len - 1] != '/')
    {
        siri.cfg->db_path[len] = '/';
        siri.cfg->db_path[len+1] = '\0';
    }

    return 0;
}

void set_max_open_files_limit(void)
{
    struct rlimit rlim;

    if (siri.cfg->max_open_files < MIN_OPEN_FILES_LIMIT ||
            siri.cfg->max_open_files > MAX_OPEN_FILES_LIMIT)
    {
        log_warning(
                "Value max_open_files must be a value between %d and %d "
                "but we found %d. Using default value instead: %d",
                MIN_OPEN_FILES_LIMIT, MAX_OPEN_FILES_LIMIT,
                siri.cfg->max_open_files, DEFAULT_OPEN_FILES_LIMIT);
        siri.cfg->max_open_files = DEFAULT_OPEN_FILES_LIMIT;
    }

    getrlimit(RLIMIT_NOFILE, &rlim);

    uint16_t min_limit = (uint16_t)
            ((double) siri.cfg->max_open_files / RLIMIT_PERC_FOR_SHARDING) -1;

    if (min_limit > (uint64_t) rlim.rlim_max)
    {
        siri.cfg->max_open_files =
                (uint16_t) ((double) rlim.rlim_max * RLIMIT_PERC_FOR_SHARDING);
        log_warning(
                "We want to set a max-open-files value which "
                "exceeds %d%% of the current hard limit.\n\nWe "
                "will use %d as max_open_files for now.\n"
                "Please increase the hard-limit using:\n"
                "ulimit -Hn %d",
                (uint8_t) (RLIMIT_PERC_FOR_SHARDING * 100),
                siri.cfg->max_open_files,
                min_limit);
        min_limit = siri.cfg->max_open_files * 2;
    }

    if (min_limit > (uint64_t) rlim.rlim_cur)
    {
        rlim_t prev = rlim.rlim_cur;
        log_info(
                "Increasing soft-limit from %d to %d since we want "
                "to use only %d%% from the soft-limit for shard files",
                (uint64_t) rlim.rlim_cur,
                min_limit,
                (uint8_t) (RLIMIT_PERC_FOR_SHARDING * 100));
        rlim.rlim_cur = min_limit;
        if (setrlimit(RLIMIT_NOFILE, &rlim))
        {
            siri.cfg->max_open_files = (uint16_t) (prev / 2);
            log_warning("Could not set the soft-limit to %d, "
                    "changing max open files to: %u",
                    min_limit, siri.cfg->max_open_files);
        }
    }
}

int siri_start(void)
{
    int rc;
    struct timespec start;
    struct timespec end;
    uv_signal_t sig[N_SIGNALS];
    int i;

    /* get start time so we can calculate the startup_time */
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* initialize listener (set enter and exit functions) */
    siridb_init_listener();

    /* initialize props (set props functions) */
    siridb_init_props();

    /* initialize aggregation */
    siridb_init_aggregates();

    /* load SiriDB grammar */
    siri.grammar = compile_siri_grammar_grammar();

    /* create store for SiriDB instances */
    siri.siridb_list = llist_new();
    if (siri.siridb_list == NULL)
    {
        return -1;
    }

    /* initialize file handler for shards */
    siri.fh = siri_fh_new(siri.cfg->max_open_files);

    /* initialize the default event loop */
    siri.loop = malloc(sizeof(uv_loop_t));
    if (siri.loop == NULL)
    {
        return -1;
    }
    uv_loop_init(siri.loop);

    /* initialize the back-end-, client- server and load databases */
    if (    (siri.cfg->http_status_port && (rc = siri_health_init())) ||
            (siri.cfg->http_api_port && (rc = siri_api_init())) ||
            (rc = siri_service_account_init(&siri)) ||
            (rc = siri_service_request_init()) ||
            (rc = sirinet_bserver_init(&siri)) ||
            (rc = sirinet_clserver_init(&siri)) ||
            (rc = SIRI_load_databases()))
    {
        SIRI_destroy();
        free(siri.loop);
        siri.loop = NULL;
        return rc;  /* something went wrong  */
    }

    /* bind signals to the event loop */
    for (i = 0; i < N_SIGNALS; i++)
    {
        uv_signal_init(siri.loop, &sig[i]);
        uv_signal_start(&sig[i], SIRI_signal_handler, signals[i]);
    }

    /* initialize optimize task (bind siri.optimize) */
    siri_optimize_init(&siri);

    /* initialize heart-beat task (bind siri.heartbeat) */
    siri_heartbeat_init(&siri);

    /* initialize buffer-sync task (bind siri.buffersync) */
    siri_buffersync_init(&siri);

    /* initialize backup (bind siri.backup) */
    if (siri_backup_init(&siri))
    {
        SIRI_destroy();
        free(siri.loop);
        siri.loop = NULL;
        return -1;
    }

    /* update siridb status to running */
    SIRI_set_running_state();

    /* set startup time */
    clock_gettime(CLOCK_MONOTONIC, &end);
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
        if (rc) /* could be UV_EBUSY (-16) in case handlers are not closed */
        {
            log_error("Error occurred while closing the event loop: %d", rc);
        }
    }

    /* first free the File Handler. (this will close all open shard files) */
    siri_fh_free(siri.fh);

    /* this will free each SiriDB database and the list */
    llist_free_cb(siri.siridb_list, (llist_cb) siridb_decref_cb, NULL);

    /* free siridb grammar */
    cleri_grammar_free(siri.grammar);

    /* free siridb service accounts */
    siri_service_account_destroy(&siri);

    /* free siridb service request */
    siri_service_request_destroy();

    /* free config */
    siri_cfg_destroy(&siri);

    /* free event loop */
    free(siri.loop);
}

static int SIRI_load_databases(void)
{
    DIR * db_container_path;
    struct dirent * dbpath;
    char * buffer;

    if (!xpath_is_dir(siri.cfg->db_path))
    {
        log_warning("Database directory not found, creating directory '%s'.",
                siri.cfg->db_path);
        if (mkdir(siri.cfg->db_path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.",
                    siri.cfg->db_path);
            return -1;
        }
    }

    if ((db_container_path = opendir(siri.cfg->db_path)) == NULL)
    {
        log_error("Cannot open database directory '%s'.",
                siri.cfg->db_path);
        return -1;
    }

    while((dbpath = readdir(db_container_path)) != NULL)
    {
        if (    strcmp(dbpath->d_name, ".") == 0 ||
                strcmp(dbpath->d_name, "..") == 0 ||
                strncmp(dbpath->d_name, "__", 2) == 0)
        {
            /* skip "." ".." and prefixed with double underscore directories */
            continue;
        }

        if (asprintf(
                &buffer,
                "%s%s/",
                siri.cfg->db_path,
                dbpath->d_name) < 0)
        {
            /* allocation error occurred */
            log_critical("Could not allocate space for database path");
            continue;
        }

        if (!siridb_is_db_path(buffer))
        {
            /* this is not a SiriDB database directory, files are missing */
            goto next;
        }

        if (siri.siridb_list->len == MAX_NUMBER_DB)
        {
            log_critical(
                    "Cannot load '%s' since no more than %d databases "
                    "are allowed on a single SiriDB process.",
                    dbpath->d_name,
                    MAX_NUMBER_DB);
            goto next;
        }

        if (siridb_new(buffer, 0) == NULL)
        {
            log_error("Could not load '%s'.", dbpath->d_name);
        }
next:
        free(buffer);
    }
    closedir(db_container_path);

    return 0;
}

static void SIRI_destroy(void)
{
    log_warning("Closing SiriDB Server (version: %s)", SIRIDB_VERSION);
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
        siridb_t * siridb = (siridb_t *) db_node->data;
        if (siridb->replicate != NULL)
        {
            siridb_replicate_close(siridb->replicate);
        }
        if (siridb->reindex != NULL && siridb->reindex->timer != NULL)
        {
            siridb_reindex_close(siridb->reindex);
        }
        if (siridb->groups != NULL)
        {
            siridb_groups_destroy(siridb->groups);
        }
        siridb->server->flags &= ~SERVER_FLAG_RUNNING;
        siridb_servers_send_flags(siridb->servers);

        db_node = db_node->next;
    }
}

static void SIRI_walk_try_close(uv_handle_t * handle, int * num)
{
    if (handle->type == UV_ASYNC || handle->type == UV_TIMER)
    {
        (*num)++;
    }
}

static void SIRI_try_close(uv_timer_t * handle)
{
    int num = -1;  /* minus one because we should not include 'this' timer */

    uv_walk(siri.loop, (uv_walk_cb) SIRI_walk_try_close, &num);

    if (!--closing_attempts && num)
    {
        log_error("SiriDB will close but still had %d task(s) running.", num);
        /*
         * We usually assume all async tasks and timers will finish 'normal'
         * and take care of destroying the handle. Since now we will loop and
         * force all handlers to close we must be able to act on this behavior.
         * Therefore we set siri_err which can be checked.
         */
        siri_err = ERR_CLOSE_TIMEOUT_REACHED;
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
        log_info(
                "SiriDB is closing but is waiting for %d task(s) to finish...",
                num);
    }
}

static void SIRI_signal_handler(
        uv_signal_t * req __attribute__((unused)),
        int signum)
{
    if (signum == SIGPIPE)
    {
        log_warning(
                "Signal (%d) received, probably a connection was lost",
                signum);
        return;
    }

    if (siri.status == SIRI_STATUS_CLOSING)
    {
        log_error(
                "Receive a second signal (%d), stop SiriDB immediately!",
                signum);
        /* set siri_err, see ERR_CLOSE_TIMEOUT_REACHED for the reason why */
        siri_err = ERR_CLOSE_ENFORCED;
        SIRI_destroy();
    }
    else
    {
        /* stop optimize task */
        siri_optimize_stop();

        /* stop heart-beat task */
        siri_heartbeat_stop(&siri);

        /* stop buffer-sync task */
        siri_buffersync_stop(&siri);

        /* destroy backup (mode) task */
        siri_backup_destroy(&siri);

        /* mark SiriDB as closing and remove ONLINE flag from servers. */
        SIRI_set_closing_state();

        if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
        {
            log_warning("Asked SiriDB Server to stop (%d)", signum);
        }
        else
        {
            log_critical("Signal (%d) received, stop SiriDB!",
                    signum);

            /* set siri_err, see ERR_CLOSE_TIMEOUT_REACHED for the reason why */
            if (!siri_err)
            {
                siri_err = signum;
            }
        }
        /*
         * Try to finish open tasks
         */
        uv_timer_init(siri.loop, &closing_timer);
        uv_timer_start(
                &closing_timer,
                SIRI_try_close,
                0,
                WAIT_BETWEEN_CLOSE_ATTEMPTS);
    }
}

static void SIRI_walk_close_handlers(
        uv_handle_t * handle,
        void * arg __attribute__((unused)))
{
    if (uv_is_closing(handle))
    {
        return;
    }

    switch (handle->type)
    {
    case UV_SIGNAL:
        /* this is where we cleanup the signal handlers */
        uv_close(handle, NULL);
        break;

    case UV_UDP:
        siridb_tee_close((siridb_tee_t *) handle->data);
        break;

    case UV_TCP:
    case UV_NAMED_PIPE:
        {
            if (handle->data == NULL)
            {
                uv_close(handle, NULL);
            }
            else if (siri_health_is_handle(handle))
            {
                siri_health_close((siri_health_request_t *) handle->data);
            }
            else
            {
                sirinet_stream_decref((sirinet_stream_t *) handle->data);
            }
        }
        break;

    case UV_TIMER:
        /* we do not expect any timer object since they should all be closed
         * (or at least closing) at this point.
         */
#ifndef NDEBUG
        LOGC(   "Found a non closing Timer, all timers should "
                "be stopped at this point.");
#endif
        uv_timer_stop((uv_timer_t *) handle);
        uv_close(handle, NULL);
        break;

    case UV_ASYNC:
#ifndef NDEBUG
        LOGC(   "An async task is only expected to be found in case "
                "not all tasks were closed within the timeout limit, "
                "or when a critical signal error is raised.");
#endif
        uv_close(handle, siri_async_close);
        break;

    default:

#ifndef NDEBUG
        LOGC("Oh oh, we might need to implement type %d", handle->type);
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
    uv_run(siri.loop, UV_RUN_NOWAIT);
}
