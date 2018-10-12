/*
 * pkg.h - SiriDB Package type.
 */
#ifndef SIRINET_PKG_H_
#define SIRINET_PKG_H_

typedef struct sirinet_pkg_s sirinet_pkg_t;

#include <inttypes.h>
#include <qpack/qpack.h>
#include <siri/net/stream.h>
#include <uv.h>

sirinet_pkg_t * sirinet_pkg_new(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const unsigned char * data);
qp_packer_t * sirinet_packer_new(size_t alloc_size);
sirinet_pkg_t * sirinet_packer2pkg(
        qp_packer_t * packer,
        uint16_t pid,
        uint8_t tp);
sirinet_pkg_t * sirinet_pkg_err(
        uint16_t pid,
        uint32_t len,
        uint8_t tp,
        const char * msg);

int sirinet_pkg_send(sirinet_stream_t * client, sirinet_pkg_t * pkg);
sirinet_pkg_t * sirinet_pkg_dup(sirinet_pkg_t * pkg);

/* Shortcut to print an packer object */
#define sn_packer_print(packer)             \
    qp_print(packer->buffer + sizeof(sirinet_pkg_t), packer->len - sizeof(sirinet_pkg_t))

struct sirinet_pkg_s
{
    uint32_t len;   /* length of data, sizeof(sirinet_pkg_t) is not included */
    uint16_t pid;
    uint8_t tp;
    uint8_t checkbit;
    unsigned char data[];
};

#endif  /* SIRINET_PKG_H_ */
