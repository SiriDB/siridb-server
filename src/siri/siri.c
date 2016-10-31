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
#include <assert.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/async.h>
#include <siri/cfg/cfg.h>
#include <siri/db/aggregate.h>
#include <siri/db/buffer.h>
#include <siri/db/groups.h>
#include <siri/db/pools.h>
#include <siri/db/props.h>
#include <siri/db/series.h>
#include <siri/db/server.h>
#include <siri/db/servers.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <siri/help/help.h>
#include <siri/net/bserver.h>
#include <siri/net/clserver.h>
#include <siri/net/socket.h>
#include <siri/parser/listener.h>
#include <siri/siri.h>
#include <siri/version.h>
#include <stddef.h>
#include <stdio.h>
#include <strextra/strextra.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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
static int closing_attempts = 40;  // times 3 seconds is 2 minutes

#define N_SIGNALS 5
static int signals[N_SIGNALS] = {
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
            logger_init(stdout, n);
            return;
        }
    }
    /* We should not get here since args should always
     * contain a valid log level
     */
    logger_init(stdout, 0);
}



int siri_start(void)
{
    int rc;
    struct timespec start;
    struct timespec end;
    uv_signal_t sig[N_SIGNALS];

    /* get start time so we can calculate the startup_time */
    clock_gettime(CLOCK_MONOTONIC, &start);

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

    /* initialize the default event loop */
    siri.loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(siri.loop);

    /* initialize the back-end-, client- server and load databases */
    if (    (rc = sirinet_bserver_init(&siri)) ||
            (rc = sirinet_clserver_init(&siri)) ||
            (rc = SIRI_load_databases()))
    {
        SIRI_destroy();
        free(siri.loop);
        siri.loop = NULL;
        return rc; // something went wrong
    }

    /* bind signals to the event loop */
    for (int i = 0; i < N_SIGNALS; i++)
    {
        uv_signal_init(siri.loop, &sig[i]);
        uv_signal_start(&sig[i], SIRI_signal_handler, signals[i]);
    }

    /* initialize optimize task (bind siri.optimize) */
    siri_optimize_init(&siri);

    /* initialize heart-beat task (bind siri.heartbeat) */
    siri_heartbeat_init(&siri);

    /* initialize backup (bind siri.backup) */
    siri_backup_init(&siri);

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
        if (rc) // could be UV_EBUSY (-16) in case handlers are not closed yet
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

    /* free event loop */
    free(siri.loop);
}

static int SIRI_load_databases(void)
{
    DIR * db_container_path;
    struct dirent * dbpath;
    char buffer[PATH_MAX];

    if (!xpath_is_dir(siri.cfg->default_db_path))
    {
        log_warning("Database directory not found, creating directory '%s'.",
                siri.cfg->default_db_path);
        if (mkdir(siri.cfg->default_db_path, 0700) == -1)
        {
            log_error("Cannot create directory '%s'.",
                    siri.cfg->default_db_path);
            return -1;
        }
    }

    if ((db_container_path = opendir(siri.cfg->default_db_path)) == NULL)
    {
        log_error("Cannot open database directory '%s'.",
                siri.cfg->default_db_path);
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

        snprintf(buffer,
                PATH_MAX,
                "%s%s/",
                siri.cfg->default_db_path,
                dbpath->d_name);

        if (!siridb_is_db_path(buffer))
        {
        	/* this is not a SiriDB database directory, files are missing */
        	continue;
        }

        if (siri.siridb_list->len == MAX_NUMBER_DB)
        {
        	log_critical(
        			"Cannot load '%s' since no more than %d databases "
        			"are allowed on a single SiriDB process.",
					dbpath->d_name,
					MAX_NUMBER_DB);
        	continue;
        }

        if (siridb_new(buffer, 0) == NULL)
        {
            log_error("Could not load '%s'.", dbpath->d_name);
        }
    }
    closedir(db_container_path);

    return 0;
}

static void SIRI_destroy(void)
{
#ifndef DEBUG
    log_info("Closing SiriDB Server (version: %s)", SIRIDB_VERSION);
#else
    log_warning("Closing SiriDB Server (%s-DEBUG-RELEASE-%s)",
    		SIRIDB_VERSION,
			SIRIDB_BUILD_DATE);
#endif
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
        log_info("SiriDB is closing but is waiting for %d task(s) to "
                "finish...", num);
    }
}


static void SIRI_signal_handler(uv_signal_t * req, int signum)
{
    if (signum == SIGPIPE)
    {
        log_warning("Signal (%d) received, probably a connection was lost");
        return;
    }

    if (siri.status == SIRI_STATUS_CLOSING)
    {
        log_error("Receive a second signal (%d), stop SiriDB immediately!",
                signum);
        /* set siri_err, see ERR_CLOSE_TIMEOUT_REACHED for the reason why */
        siri_err = ERR_CLOSE_ENFORCED;
        SIRI_destroy();
    }
    else
    {
        /* stop optimize task */
        siri_optimize_stop(&siri);

        /* stop heart-beat task */
        siri_heartbeat_stop(&siri);

        /* destroy backup (mode) task */
        siri_backup_destroy(&siri);

        /* mark SiriDB as closing and remove ONLINE flag from servers. */
        SIRI_set_closing_state();

        if (signum == SIGINT || signum == SIGTERM)
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

static void SIRI_walk_close_handlers(uv_handle_t * handle, void * arg)
{
    if (uv_is_closing(handle))
    {
        return;
    }

    switch (handle->type)
    {
    case UV_WORK:
        break;
    case UV_SIGNAL:
        /* this is where we cleanup the signal handlers */
        uv_close(handle, NULL);
        break;

    case UV_TCP:
        /* This can be a TCP server with data set to NULL or a SiriDB socket
         * which should be destroyed.
         */
        if (handle->data == NULL)
        {
            uv_close(handle, NULL);
        }
        else
        {
            sirinet_socket_decref(handle);
        }
        break;

    case UV_TIMER:
        /* we do not expect any timer object since they should all be closed
         * (or at least closing) at this point.
         */
#ifdef DEBUG
        LOGC(   "Found a non closing Timer, all timers should "
                "be stopped at this point.");
#endif
        uv_timer_stop((uv_timer_t *) handle);
        uv_close(handle, NULL);
        break;

    case UV_ASYNC:
#ifdef DEBUG
        LOGC(   "An async task is only expected to be found in case "
                "not all tasks were closed within the timeout limit, "
                "or when a critical signal error is raised.");
#endif
        uv_close(handle, siri_async_close);
        break;

    default:

#ifdef DEBUG
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
    uv_run(siri.loop, UV_RUN_DEFAULT);
}
