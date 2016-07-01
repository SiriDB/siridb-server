/*
 * pkg.h - SiriDB Package type.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-06-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <uv.h>

#define PKG_HEADER_SIZE 14

typedef struct sirinet_pkg_s
{
    uint64_t pid;
    uint32_t len;   // length of data (SN_PKG_HEADER_SIZE excluded)
    uint16_t tp;
    char data[];
} sirinet_pkg_t;


/* do not forget to run free(pkg) when using sirinet_new_pkg */
sirinet_pkg_t * sirinet_pkg_new(
        uint64_t pid,
        uint32_t len,
        uint16_t tp,
        const char * data);

int sirinet_pkg_send(uv_stream_t * client, sirinet_pkg_t * pkg);
sirinet_pkg_t * sirinet_pkg_dup(sirinet_pkg_t * pkg);
