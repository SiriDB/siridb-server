/*
 * user.h - contains functions for a SiriDB database member.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#pragma once

#include <qpack/qpack.h>
#include <inttypes.h>
#include <siri/db/db.h>

typedef struct siridb_s siridb_t;

typedef struct siridb_user_s
{
    char * username;
    char * password; /* keeps an encrypted password */
    uint32_t access_bit;
} siridb_user_t;

typedef struct siridb_users_s
{
    siridb_user_t * user;
    struct siridb_users_s * next;
} siridb_users_t;

void siridb_free_users(siridb_users_t * users);

siridb_user_t * siridb_new_user(void);

int siridb_add_user(
        siridb_t * siridb,
        siridb_user_t * user,
        char * err_msg);

int siridb_load_users(siridb_t * siridb);

void siridb_free_user(siridb_user_t * user);

int siridb_drop_user(
        siridb_t * siridb,
        const char * username,
        char * err_msg);

siridb_user_t * siridb_get_user(
        siridb_t * siridb,
        const char * username,
        const char * password);

void siridb_prop_user(siridb_user_t * user, qp_packer_t * packer, int prop);
