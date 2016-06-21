/*
 * bclient.h - Back-end Client SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 20-06-2016
 *
 */
#pragma once

#include <imap64/imap64.h>
#include <inttypes.h>
#include <uv.h>
#include <siri/net/pkg.h>

typedef void (* sirinet_promise_cb)(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg,
        void * data);

typedef struct sirinet_bclient_s
{
    uint64_t pid;
    imap64_t * promises;
    uv_tcp_t * socket;
    uv_connect_t * connect;
} sirinet_bclient_t;

typedef struct sirinet_promise_s
{
    uv_timer_t timer;
    sirinet_promise_cb cb;
    void * data;
} sirinet_promise_t;

sirinet_bclient_t * sirinet_bclient_new(void);
void sirinet_bclient_init(sirinet_bclient_t * bclient);
void sirinet_bclient_free(sirinet_bclient_t * bclient);
