/*
 * users.h - contains functions for a SiriDB database members.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-05-2016
 *
 */
#pragma once

#include <inttypes.h>
#include <siri/db/db.h>
#include <llist/llist.h>

typedef struct siridb_s siridb_t;

int siridb_users_load(siridb_t * siridb);
void siridb_users_free(llist_t * users);

int siridb_users_add_user(
        siridb_t * siridb,
        siridb_user_t * user,
        char * err_msg);

int siridb_users_drop_user(
        siridb_t * siridb,
        const char * username,
        char * err_msg);

/*
 * Returns NULL when the user is not found of when the given password is
 * incorrect. When *password is NULL the password will NOT be checked and
 * the user will be returned when found.
 */
siridb_user_t * siridb_users_get_user(
        llist_t * users,
        const char * username,
        const char * password);

int siridb_users_save(siridb_t * siridb);