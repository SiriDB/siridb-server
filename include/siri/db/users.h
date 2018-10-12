/*
 * users.h - Collection of database users.
 */
#ifndef SIRIDB_USERS_H_
#define SIRIDB_USERS_H_

#include <inttypes.h>
#include <siri/db/db.h>
#include <siri/db/user.h>
#include <llist/llist.h>

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
siridb_user_t * siridb_users_get_user(
        siridb_t * siridb,
        const char * username,
        const char * password);
int siridb_users_save(siridb_t * siridb);
ssize_t siridb_users_get_file(char ** buffer, siridb_t * siridb);

#endif  /* SIRIDB_USERS_H_ */
