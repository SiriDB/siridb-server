/*
 * imap32.c - map for uint32_t integer keys
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-03-2016
 *
 */
#include <stdio.h>
#include <imap32/imap32.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

imap32_t * imap32_new(void)
{
    imap32_t * imap = (imap32_t *) malloc(sizeof(imap32_t));
    imap->size = 0;
    imap->grid = NULL;
    return imap;
}

void imap32_free(imap32_t * imap)
{
    uint_fast16_t i;
    while (imap->size--)
    {
        im_grid_t * grid = imap->grid + imap->size;

        if (grid == NULL)
            continue;

        for (i = 0; i < 256; i++)
        {
            if ((*grid)[i] == NULL)
                continue;
            free((*grid)[i]);
        }
    }
    free(imap->grid);
    free(imap);
}

void imap32_add(imap32_t * imap, uint32_t id, void * data)
{
    uint32_t key = id / 65536;

    if (key >= imap->size)
    {
        size_t temp = imap->size;
        imap->size = key + 1;
        imap->grid = (im_grid_t *) realloc(
                imap->grid, imap->size * sizeof(im_grid_t));

        // TODO: Actually this is not totally right since 0 is not NULL
        memset(imap->grid + temp, 0, (imap->size- temp) * sizeof(im_grid_t));
    }
    im_grid_t * grid = imap->grid + key;
    id %= 65536;
    key = id / 256;

    if ((*grid)[key] == NULL)
        // TODO: Same for calloc, 0 is not NULL
        (*grid)[key] = (im_store_t *) calloc(1, sizeof(im_store_t));

    im_store_t * store = (*grid)[key];

    id %= 256;

    if (store->data[id] == NULL)
        store->size++;

    store->data[id] = data;
}

void * imap32_get(imap32_t * imap, uint32_t id)
{
    uint32_t key = id / 65536;

    if (key >= imap->size)
        return NULL;

    im_grid_t * grid = imap->grid + key;

    id %= 65536;
    key = id / 256;

    return ((*grid)[key] == NULL) ? NULL : (*grid)[key]->data[id % 256];
}

void * imap32_pop(imap32_t * imap, uint32_t id)
{
    void * data;
    uint32_t key = id / 65536;

    if (key >= imap->size)
        return NULL;

    im_grid_t * grid = imap->grid + key;

    id %= 65536;
    key = id / 256;

    im_store_t * store = (*grid)[key];

    if (store == NULL)
        return NULL;

    id %= 256;

    if (store->data[id] == NULL)
        return NULL;

    data = store->data[id];
    store->data[id] = NULL;

    if (--store->size)
        return data;

    free((*grid)[key]);
    (*grid)[key] = NULL;

    return data;
}

void imap32_walk(imap32_t * imap, imap32_cb_t cb, void * args)
{
    im_store_t * store;
    im_grid_t * grid;
    void * data;
    for (size_t n = 0; n < imap->size; n++)
    {
        grid = imap->grid + n;
        for (uint_fast8_t i = 255; i--;)
        {
            if ((store = (*grid)[i]) == NULL)
                continue;
            for (uint_fast8_t j = 255; j--;)
            {
                if ((data = store->data[j]) == NULL)
                    continue;
                (*cb)(data, args);
            }
        }
    }
}
