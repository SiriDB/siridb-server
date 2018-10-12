/*
 * siri.h - Root for SiriDB.
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
#ifndef SIRI_H_
#define SIRI_H_

#define PCRE2_CODE_UNIT_WIDTH 8

#define SIRI_MAX_SIZE_ERR_MSG 1024
#define MAX_NUMBER_DB 4

typedef enum
{
    SIRI_STATUS_LOADING,
    SIRI_STATUS_RUNNING,
    SIRI_STATUS_CLOSING
} siri_status_t;

typedef struct siri_s siri_t;

extern siri_t siri;

#include <uv.h>
#include <pcre2.h>
#include <siri/grammar/grammar.h>
#include <siri/db/db.h>
#include <siri/file/handler.h>
#include <stdbool.h>
#include <siri/optimize.h>
#include <siri/backup.h>
#include <siri/heartbeat.h>
#include <siri/cfg/cfg.h>
#include <siri/args/args.h>
#include <llist/llist.h>

void siri_setup_logger(void);
int siri_start(void);
void siri_free(void);

struct siri_s
{
    siri_status_t status;
    uv_loop_t * loop;
    cleri_grammar_t * grammar;
    llist_t * siridb_list;
    siri_fh_t * fh;
    siri_optimize_t * optimize;
    uv_timer_t * backup;
    uv_timer_t * heartbeat;
    uv_timer_t * buffersync;
    siri_cfg_t * cfg;
    siri_args_t * args;
    uv_mutex_t siridb_mutex;
    uint32_t startup_time;

    /* initialized by sidi_service_account_init */
    llist_t * accounts;

    /* initialized by sidi_service_request_init */
    pcre2_code * dbname_regex;
    pcre2_match_data * dbname_match_data;

    /* socket and promises used for expanding (client) */
    sirinet_stream_t * client;
    uv_timer_t timer;
};

#endif  /* SIRI_H_ */
