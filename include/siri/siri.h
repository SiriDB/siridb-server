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
#pragma once

#define PCRE2_CODE_UNIT_WIDTH 8

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

#define SIRI_MAX_SIZE_ERR_MSG 1024
#define SIRIDB_BUILD_DATE __DATE__ " " __TIME__
#define MAX_NUMBER_DB 4

typedef struct cleri_grammar_s cleri_grammar_t;
typedef struct siridb_list_s siridb_list_t;
typedef struct siri_fh_s siri_fh_t;
typedef struct siri_optimize_s siri_optimize_t;
typedef struct siri_heartbeat_s siri_heartbeat_t;
typedef struct siri_backup_s siri_backup_t;
typedef struct siri_cfg_s siri_cfg_t;
typedef struct siri_args_s siri_args_t;
typedef struct llist_s llist_t;

typedef enum
{
    SIRI_STATUS_LOADING,
    SIRI_STATUS_RUNNING,
    SIRI_STATUS_CLOSING
} siri_status_t;

typedef struct siri_s
{
    siri_status_t status;
    uv_loop_t * loop;
    cleri_grammar_t * grammar;
    llist_t * siridb_list;
    siri_fh_t * fh;
    siri_optimize_t * optimize;
    uv_timer_t * backup;
    uv_timer_t * heartbeat;
    siri_cfg_t * cfg;
    siri_args_t * args;
    uv_mutex_t siridb_mutex;
    uint32_t startup_time;

    /* initialized by sidi_admin_account_init */
    llist_t * accounts;

    /* initialized by sidi_admin_request_init */
    pcre2_code * dbname_regex;
    pcre2_match_data * dbname_match_data;

    /* socket and promises used for expanding (client) */
    uv_tcp_t * socket;
    uv_timer_t timer;
} siri_t;

void siri_setup_logger(void);
int siri_start(void);
void siri_free(void);

extern siri_t siri;
