/*
 * imap64.h - map for uint64_t integer keys
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-04-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <slist/slist.h>

#define IMAP64_NODE_SZ 16

enum
{
    IMAP64_ERR=-1,
    IMAP64_OK,
    IMAP64_EXISTS
};

typedef struct imap64_node_s * imap64_nodes_t[IMAP64_NODE_SZ];

typedef struct imap64_node_s
{
    size_t size;
    void * data ;
    imap64_nodes_t nodes;
} imap64_node_t;


typedef struct imap64_s
{
    size_t len;
    void * data;
    imap64_nodes_t nodes;
} imap64_t;

typedef int (*imap64_cb)(void * data, void * args);

imap64_t * imap64_new(void);
void imap64_free(imap64_t * imap);
void imap64_free_cb(imap64_t * imap, imap64_cb cb, void * args);
int imap64_add(imap64_t * imap, uint64_t id, void * data, uint8_t overwrite);
void * imap64_get(imap64_t * imap, uint64_t id);
void * imap64_pop(imap64_t * imap, uint64_t id);
int imap64_walk(imap64_t * imap, imap64_cb cb, void * args);
void imap64_walkn(imap64_t * imap, size_t * n, imap64_cb cb, void * args);
slist_t * imap64_2slist(imap64_t * imap);
