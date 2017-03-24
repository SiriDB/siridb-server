/*
 * client.h - Client for expanding a siridb database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-03-2017
 *
 */
#pragma once


typedef struct siri_admin_client_s
{
    uint16_t pid;
    uint16_t port;
    char * host;
    char * username;
    char * password;
    char * dbname;
    char * dbpath;
    uv_stream_t * client;
} siri_admin_client_t;


void siri_admin_client_free(siri_admin_client_t * adm_client);
