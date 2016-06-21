/*
 * promise.h - Promise SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 21-06-2016
 *
 */
#pragma once


typedef void (* sirinet_promise_cb)(
        uv_handle_t * client,
        const sirinet_pkg_t * pkg,
        void * data);


typedef struct sirinet_promise_s
{
    uv_timer_t timer;
    sirinet_promise_cb cb;
    void * data;
} sirinet_promise_t;
