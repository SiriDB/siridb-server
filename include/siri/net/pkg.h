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
#include <qpack/qpack.h>
#include <uv.h>

#define PKG___QP_TP 255

typedef struct sirinet_pkg_s
{
    uint32_t len;   // length of data (SN_sizeof(sirinet_pkg_t) excluded)
    uint16_t pid;
    uint8_t tp;
    uint8_t checkbit;
    char data[];
} sirinet_pkg_t;

sirinet_pkg_t * sirinet_pkg_new(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const char * data);
sirinet_pkg_t * sirinet_pkg_copy(sirinet_pkg_t * source);
qp_packer_t * sirinet_packer_new(size_t alloc_size);
sirinet_pkg_t * sirinet_packer2pkg(
        qp_packer_t * packer,
        uint16_t pid,
        uint8_t tp);
sirinet_pkg_t * sirinet_pkg_err(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const char * data);

int sirinet_pkg_send(uv_stream_t * client, sirinet_pkg_t * pkg);
sirinet_pkg_t * sirinet_pkg_dup(sirinet_pkg_t * pkg);

/* Shortcut to print an packer object */
#define sn_packer_print(packer)             \
    qp_print(packer->buffer + sizeof(sirinet_pkg_t), packer->len - sizeof(sirinet_pkg_t))
