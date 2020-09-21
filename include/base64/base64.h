/*
 * base64.h - Base64 encoding/decoding
 */
#ifndef SIRI_BASE64_H_
#define SIRI_BASE64_H_

#include <stddef.h>

char * base64_decode(const void * data, size_t n, size_t * size);
char * base64_encode(const void * data, size_t n, size_t * size);


#endif  /* SIRI_BASE64_H_ */
