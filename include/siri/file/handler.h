/*
 * handler.h - File handler for shard files.
 */
#ifndef SIRI_FH_H_
#define SIRI_FH_H_

typedef struct siri_fh_s siri_fh_t;

#include <inttypes.h>
#include <siri/file/pointer.h>

siri_fh_t * siri_fh_new(uint16_t size);
void siri_fh_free(siri_fh_t * fh);
int siri_fopen(
        siri_fh_t * fh,
        siri_fp_t * fp,
        const char * fn,
        const char * modes);

struct siri_fh_s
{
    uint16_t size;
    uint16_t idx;
    siri_fp_t ** fpointers;
};

#endif  /* SIRI_FH_H_ */
