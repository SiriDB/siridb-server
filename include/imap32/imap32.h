/*
 * imap32.h - map for uint32_t integer keys
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-03-2016
 *
 */
#pragma once

#include <inttypes.h>

typedef struct im_store_s
{
    size_t size;
    void * data[256];
} im_store_t;

typedef struct im_grid_s
{
    size_t size;
    im_store_t * store[256];
} im_grid_t;

typedef struct imap32_s
{
    size_t len;
    size_t size;
    size_t offset;
    im_grid_t * grid;
} imap32_t;

typedef void (*imap32_cb_t)(void * data, void * args);

/* create a new imap32 */
imap32_t * imap32_new(void);

/* call imap32_free when finished using an imap32 */
void imap32_free(imap32_t * imap);

/* overwrites an existing value */
void imap32_add(imap32_t * imap, uint32_t id, void * data);

/* returns NULL in case the value is not found */
void * imap32_get(imap32_t * imap, uint32_t id);

/* returns NULL in case the value is not found */
void * imap32_pop(imap32_t * imap, uint32_t id);

/* run the callback for each element in the imap32 */
void imap32_walk(imap32_t * imap, imap32_cb_t cb, void * args);

