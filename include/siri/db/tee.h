/*
 * tee.h - To tee the data for a SiriDB database.
 */
#ifndef SIRIDB_TEE_H_
#define SIRIDB_TEE_H_

typedef struct siridb_tee_s siridb_tee_t;

#define SIRIDB_TEE_DEFAULT_UDP_PORT 9104

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
    uint16_t id;
    uint16_t pkg_id;
    uint16_t port;
    uint16_t _pad1;
    char * address;
    uv_udp_t * udp;
    uv_mutex_t lock_;
};


static inline bool siridb_tee_is_configured(siridb_tee_t * tee)
{
    return tee->address != NULL;
};

#define siridb_tee_set_id(__siridb) \
    do { \
        (__siridb)->tee->id = \
        (__siridb)->server->pool*2 + (__siridb)->server->id; \
    } while(0)

#endif /* SIRIDB_TEE_H_ */
