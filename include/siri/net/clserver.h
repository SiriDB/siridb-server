/*
 * clserver.h - TCP server for serving client requests.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 09-03-2016
 *
 */
#pragma once

#include <uv.h>
#include <siri/net/pipe.h>
#include <siri/net/socket.h>
#include <siri/siri.h>

typedef struct siri_s siri_t;

#define sirinet_client_incref(client)                                          \
switch ((client)->type)                                                        \
{                                                                              \
case UV_TCP:                                                                   \
    sirinet_socket_incref(client);                                             \
    break;                                                                     \
case UV_NAMED_PIPE:                                                            \
    sirinet_pipe_incref(client);                                               \
    break;                                                                     \
default:                                                                       \
    break;                                                                     \
}

#define sirinet_client_decref(client)                                          \
switch ((client)->type)                                                        \
{                                                                              \
case UV_TCP:                                                                   \
    sirinet_socket_decref(client);                                             \
    break;                                                                     \
case UV_NAMED_PIPE:                                                            \
    sirinet_pipe_decref(client);                                               \
    break;                                                                     \
default:                                                                       \
    uv_close((uv_handle_t *) (client), NULL);                                  \
    break;                                                                     \
}

#define CLIENT_SIRIDB(client, siridb)                                          \
siridb_t * siridb = NULL;                                                      \
switch ((client)->type)                                                        \
{                                                                              \
case UV_TCP:                                                                   \
    siridb = ((sirinet_socket_t *) (client)->data)->siridb;                    \
    break;                                                                     \
case UV_NAMED_PIPE:                                                            \
    siridb = ((sirinet_pipe_t *) (client)->data)->siridb;                      \
    break;                                                                     \
default:                                                                       \
    break;                                                                     \
}

#define CLIENT_USER(client, user)                                              \
siridb_user_t * user = NULL;                                                   \
switch ((client)->type)                                                        \
{                                                                              \
case UV_TCP:                                                                   \
    user = (siridb_user_t *) ((sirinet_socket_t *) (client)->data)->origin;    \
    break;                                                                     \
case UV_NAMED_PIPE:                                                            \
    user = (siridb_user_t *) ((sirinet_pipe_t *) (client)->data)->origin;      \
    break;                                                                     \
default:                                                                       \
    break;                                                                     \
}

int sirinet_clserver_init(siri_t * siri);

typedef ssize_t (*sirinet_clserver_getfile)(char ** buffer, siridb_t * siridb);
