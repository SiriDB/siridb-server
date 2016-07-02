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

#define IMAP64_NODE_SZ 16

typedef struct imap64_node_s
{
    size_t size;
    void * data ;
    struct imap64_node_s * node[IMAP64_NODE_SZ];
} imap64_node_t;


typedef struct imap64_s
{
    size_t len;
    void * data ;
    imap64_node_t * node[IMAP64_NODE_SZ];
} imap64_t;

typedef void (*imap64_cb_t)(void * data, void * args);

imap64_t * imap64_new(void);
void imap64_free(imap64_t * imap);
int imap64_add(imap64_t * imap, uint64_t id, void * data);
void * imap64_get(imap64_t * imap, uint64_t id);
void * imap64_pop(imap64_t * imap, uint64_t id);
void imap64_walk(imap64_t * imap, imap64_cb_t cb, void * args);

