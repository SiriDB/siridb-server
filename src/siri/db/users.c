/*
 * users.c - contains functions for a SiriDB database members.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-05-2016
 *
 */

#include <siri/db/users.h>
#include <siri/db/query.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <time.h>
#include <unistd.h>
#include <qpack/qpack.h>
#include <strextra/strextra.h>
#include <logger/logger.h>

#define SIRIDB_MIN_USER_LEN 2
#define SIRIDB_MAX_USER_LEN 60

#define SIRIDB_USER_ACCESS_SCHEMA 1
#define SIRIDB_USER_ACCESS_FN "useraccess.dat"

inline static int USERS_cmp(siridb_user_t * user, const char * name);
static int USERS_free(siridb_user_t * user, void * args);
static int USERS_save(siridb_user_t * user, qp_fpacker_t * fpacker);

int siridb_users_load(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;
    qp_obj_t * username;
    qp_obj_t * password;
    qp_obj_t * access_bit;
    siridb_user_t * user;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

    /* we should not have any users at this moment */
    assert(siridb->users == NULL);

    /* create a new user list */
    siridb->users = llist_new();

    /* get user access file name */
    SIRIDB_GET_FN(fn, SIRIDB_USER_ACCESS_FN)

    if (access(fn, R_OK) == -1)
    {
        /* we do not have a user access file, lets create the first user */
        user = siridb_user_new();
        siridb_user_incref(user);
        user->username = strdup("iris");
        user->access_bit = SIRIDB_ACCESS_PROFILE_FULL;

        if (    siridb_user_set_password(user, "siri", err_msg) ||
                siridb_users_add_user(siridb, user, err_msg))
        {
            log_error("%s", err_msg);
            siridb_user_decref(user);
            return -1;
        }

        return 0;
    }

    if ((unpacker = qp_from_file_unpacker(fn)) == NULL)
        return 1;

    /* unpacker will be freed in case macro fails */
    siridb_schema_check(SIRIDB_USER_ACCESS_SCHEMA)

    username = qp_new_object();
    password = qp_new_object();
    access_bit = qp_new_object();

    while (qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, username) == QP_RAW &&
            qp_next(unpacker, password) == QP_RAW &&
            qp_next(unpacker, access_bit) == QP_INT64)
    {
        user = siridb_user_new();
        siridb_user_incref(user);

        user->username = strndup(username->via->raw, username->len);
        user->password = strndup(password->via->raw, password->len);

        user->access_bit = (uint32_t) access_bit->via->int64;

        llist_append(siridb->users, user);
    }

    /* free objects */
    qp_free_object(username);
    qp_free_object(password);
    qp_free_object(access_bit);

    /* free unpacker */
    qp_free_unpacker(unpacker);

    return 0;
}

void siridb_users_free(llist_t * users)
{
    llist_free_cb(users, (llist_cb_t) USERS_free, NULL);
}

int siridb_users_add_user(
        siridb_t * siridb,
        siridb_user_t * user,
        char * err_msg)
{
    if (strlen(user->username) < SIRIDB_MIN_USER_LEN)
    {
        sprintf(err_msg, "User name should be at least %d characters.",
                SIRIDB_MIN_USER_LEN);
        return 1;
    }

    if (strlen(user->username) > SIRIDB_MAX_USER_LEN)
    {
        sprintf(err_msg, "User name should be at least %d characters.",
                SIRIDB_MAX_USER_LEN);
        return 1;
    }

    if (!strx_is_graph(user->username))
    {
        sprintf(err_msg,
                "User name contains illegal characters. (only graphical "
                "characters are allowed, no spaces, tabs etc.)");
        return 1;
    }

    if (llist_get(siridb->users, (llist_cb_t) USERS_cmp, user->username) != NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User name '%s' already exists.",
                user->username);
        return 1;
    }

    /* add the user to the users */
    llist_append(siridb->users, user);

    if (siridb_users_save(siridb))
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Could not save user '%s' to file.",
                user->username);
        log_critical(err_msg);
        llist_remove(siridb->users, (llist_cb_t) USERS_cmp, user->username);
        return 1;
    }

    return 0;
}

siridb_user_t * siridb_users_get_user(
        llist_t * users,
        const char * username,
        const char * password)
{
    siridb_user_t * user;
    char * pw;

    if ((user = llist_get(
            users,
            (llist_cb_t) USERS_cmp,
            (void *) username)) == NULL)
    {
        return NULL;
    }

    if (password == NULL)
    {
        return user;
    }

    pw = crypt(password, user->password);

    return (strcmp(pw, user->password) == 0) ? user : NULL;
}

int siridb_users_drop_user(
        siridb_t * siridb,
        const char * username,
        char * err_msg)
{
    siridb_user_t * user;

    if ((user = llist_remove(
            siridb->users,
            (llist_cb_t) USERS_cmp,
            (void *) username)) == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User '%s' does not exist.",
                username);
        return 1;
    }

    /* decrement reference for user object */
    siridb_user_decref(user);

    if (siridb_users_save(siridb))
    {
        log_critical("Could not write users to file!");
    }

    return 0;
}

int siridb_users_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    /* get user access file name */
    SIRIDB_GET_FN(fn, SIRIDB_USER_ACCESS_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
    {
        return 1;
    }

    /* open a new array */
    qp_fadd_type(fpacker, QP_ARRAY_OPEN);

    /* write the current schema */
    qp_fadd_int16(fpacker, SIRIDB_USER_ACCESS_SCHEMA);

    /* we can and should skip this if we have no users to save */
    llist_walk(siridb->users, (llist_cb_t) USERS_save, fpacker);

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}

static int USERS_save(siridb_user_t * user, qp_fpacker_t * fpacker)
{
    qp_fadd_type(fpacker, QP_ARRAY3);
    qp_fadd_string(fpacker, user->username);
    qp_fadd_string(fpacker, user->password);
    qp_fadd_int32(fpacker, (int32_t) user->access_bit);
    return 0;
}

inline static int USERS_cmp(siridb_user_t * user, const char * name)
{
    return (strcmp(user->username, name) == 0);
}

static int USERS_free(siridb_user_t * user, void * args)
{
    siridb_user_decref(user);
    return 0;
}

