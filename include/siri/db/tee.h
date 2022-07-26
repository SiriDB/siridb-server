/*
 * tee.h - To tee the data for a SiriDB database.
 */
#ifndef SIRIDB_TEE_H_
#define SIRIDB_TEE_H_

typedef struct siridb_tee_s siridb_tee_t;

#define SIRIDB_TEE_DEFAULT_TCP_PORT 9104

enum
{
    SIRIDB_TEE_FLAG = 1<<31,
};


#include <uv.h>
#include <stdbool.h>
#include <siri/net/pkg.h>

siridb_tee_t * siridb_tee_new(void);
void siridb_tee_close(siridb_tee_t * tee);
int siridb_tee_set_address_port(
        siridb_tee_t * tee,
        const char * address,
        uint16_t port);
void siridb_tee_write(siridb_tee_t * tee, sirinet_pkg_t * pkg);
void siridb_tee_free(siridb_tee_t * tee);
const char * siridb_tee_str(siridb_tee_t * tee);

typedef void (*siridb_tee_cb)(uv_handle_t *);


struct siridb_tee_s
{
    uint32_t flags;  /* maps to sirnet_stream_t tp for cleanup */
    uint16_t port;
    uint16_t _pad0;
    char * address;
    uv_udp_t * udp;
    uv_mutex_t lock_;
};


static inline bool siridb_tee_is_configured(siridb_tee_t * tee)
{
    return tee->address != NULL;
};

static inline bool siridb_tee_is_handle(uv_handle_t * handle)
{
    return
        handle->type == UV_TCP &&
        handle->data &&
        (((siridb_tee_t *) handle->data)->flags & SIRIDB_TEE_FLAG);
}

#endif /* SIRIDB_TEE_H_ */
