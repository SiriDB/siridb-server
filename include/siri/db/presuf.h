/*
 * presuf.h - Prefix and Suffix store.
 */
#ifndef SIRIDB_PRESUF_H_
#define SIRIDB_PRESUF_H_

typedef struct siridb_presuf_s siridb_presuf_t;

#include <cleri/cleri.h>

void siridb_presuf_free(siridb_presuf_t * presuf);
siridb_presuf_t * siridb_presuf_add(
        siridb_presuf_t ** presuf,
        cleri_node_t * node);
int siridb_presuf_is_unique(siridb_presuf_t * presuf);
void siridb_presuf_cleanup(void);
const char * siridb_presuf_name(
        siridb_presuf_t * presuf,
        const char * name,
        size_t len);

struct siridb_presuf_s
{
    char * prefix;
    char * suffix;
    size_t len;  /* prefix len + suffix len + terminator char */
    siridb_presuf_t * prev;
};

#endif  /* SIRIDB_PRESUF_H_ */
