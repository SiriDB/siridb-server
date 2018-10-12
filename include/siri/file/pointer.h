/*
 * pointer.h - File pointer used in combination with file handler.
 */
#ifndef SIRI_FP_H_
#define SIRI_FP_H_

typedef struct siri_fp_s siri_fp_t;

#include <stdio.h>
#include <inttypes.h>

siri_fp_t * siri_fp_new(void);
/* closes the file pointer, decrement reference counter and free if needed */
void siri_fp_decref(siri_fp_t * fp);
void siri_fp_close(siri_fp_t * fp);

struct siri_fp_s
{
    FILE * fp;
    uint8_t ref;
};

#endif  /* SIRI_FP_H_ */
