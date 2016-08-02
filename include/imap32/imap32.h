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
#include <slist/slist.h>

enum
{
    IMAP32_ERR=-1,
    IMAP32_OK,
    IMAP32_EXISTS
};

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

typedef int (*imap32_cb)(void * data, void * args);

imap32_t * imap32_new(void);
void imap32_free(imap32_t * imap);
void imap32_free_cb(imap32_t * imap, imap32_cb cb, void * args);
int imap32_add(imap32_t * imap, uint32_t id, void * data, int overwrite);
void * imap32_get(imap32_t * imap, uint32_t id);
void * imap32_pop(imap32_t * imap, uint32_t id);
int imap32_walk(imap32_t * imap, imap32_cb cb, void * args);
void imap32_walkn(imap32_t * imap, size_t * n, imap32_cb cb, void * args);
slist_t * imap32_2slist(imap32_t * imap);
