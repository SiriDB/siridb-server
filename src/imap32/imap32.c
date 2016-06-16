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
#include <assert.h>

imap32_t * imap32_new(void)
{
    imap32_t * imap = (imap32_t *) malloc(sizeof(imap32_t));
    imap->len = 0;
    imap->size = 0;
    imap->offset = 0;
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

            if (grid->store[i] == NULL)
                continue;
            free(grid->store[i]);
        }
    }
    free(imap->grid);
    free(imap);
}

void imap32_add(imap32_t * imap, uint32_t id, void * data)
{
    uint32_t key = id / 65536;

#ifdef DEBUG
    assert (data != NULL);
#endif

    if (!imap->size)
        imap->offset = key;
    else if (key < imap->offset)
    {
        size_t temp = imap->size;
        size_t diff = imap->offset - key;
        imap->size += diff;
        imap->grid = (im_grid_t *) realloc(
                imap->grid, imap->size * sizeof(im_grid_t));
        memmove(imap->grid + diff, imap->grid, temp * sizeof(im_grid_t));
        memset(imap->grid, 0, diff * sizeof(im_grid_t));
        imap->offset = key;
    }

    key -= imap->offset;

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

    if (grid->store[key] == NULL)
    {
        grid->size++;
        // TODO: Same for calloc, 0 is not NULL
        grid->store[key] = (im_store_t *) calloc(1, sizeof(im_store_t));
    }
    im_store_t * store = grid->store[key];

    id %= 256;

    if (store->data[id] == NULL)
    {
        store->size++;
        imap->len++;
    }

    store->data[id] = data;
}

void * imap32_get(imap32_t * imap, uint32_t id)
{
    uint32_t key = id / 65536 - imap->offset;

    if (key >= imap->size)
        return NULL;

    im_grid_t * grid = imap->grid + key;

    id %= 65536;
    key = id / 256;

    return (grid->store[key] == NULL) ?
            NULL : grid->store[key]->data[id % 256];
}

void * imap32_pop(imap32_t * imap, uint32_t id)
{
    void * data;
    uint32_t key = id / 65536 - imap->offset;

    if (key >= imap->size)
        return NULL;

    im_grid_t * grid = imap->grid + key;

    id %= 65536;
    key = id / 256;

    im_store_t * store = grid->store[key];

    if (store == NULL)
        return NULL;

    id %= 256;

    if (store->data[id] == NULL)
        return NULL;

    imap->len--;
    data = store->data[id];
    store->data[id] = NULL;

    if (--store->size)
        return data;

    free(grid->store[key]);
    grid->store[key] = NULL;

    if (--grid->size)
        return data;

    if (grid == imap->grid)
    {
        for (   key = 1;
                key < imap->size && !(imap->grid + key)->size;
                key++);
        imap->size -= key;
        if (imap->size)
        {
            memmove(imap->grid,
                    imap->grid + key,
                    imap->size * sizeof(im_grid_t));
            imap->grid = (im_grid_t *) realloc(
                            imap->grid, imap->size * sizeof(im_grid_t));
            imap->offset += key;
        }
        else
        {
            free(imap->grid);
            imap->offset = 0;
            imap->grid = NULL;
        }
    }
    else if (grid == imap->grid + imap->size - 1)
    {
        while (imap->size > 0 && !(imap->grid + (--imap->size))->size);
        imap->size = key;
        if (imap->size)
            imap->grid = (im_grid_t *) realloc(
                            imap->grid, imap->size * sizeof(im_grid_t));
        else
        {
            free(imap->grid);
            imap->offset = 0;
            imap->grid = NULL;
        }
    }

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
            if ((store = grid->store[i]) == NULL)
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
