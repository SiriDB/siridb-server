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
#define _GNU_SOURCE
#include <assert.h>
#include <crypt.h>
#include <logger/logger.h>
#include <qpack/qpack.h>
#include <siri/db/query.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <stdlib.h>
#include <strextra/strextra.h>
#include <string.h>
#include <time.h>
#include <xpath/xpath.h>

#define SIRIDB_USERS_SCHEMA 1
#define SIRIDB_USERS_FN "users.dat"

inline static int USERS_cmp(siridb_user_t * user, const char * name);
static int USERS_free(siridb_user_t * user, void * args);
static int USERS_save(siridb_user_t * user, qp_fpacker_t * fpacker);

#define MSG_ERR_CANNOT_WRITE_USERS "Could not write users to file!"

/*
 * Returns 0 if successful or -1 in case of an error.
 * (a SIGNAL might be raised in case of an error)
 */
int siridb_users_load(siridb_t * siridb)
{
    qp_unpacker_t * unpacker;
    qp_obj_t qp_name;
    qp_obj_t qp_password;
    qp_obj_t qp_access_bit;
    siridb_user_t * user;
    char err_msg[SIRIDB_MAX_SIZE_ERR_MSG];

    log_info("Loading users");

    /* we should not have any users at this moment */
    assert(siridb->users == NULL);

    /* create a new user list */
    siridb->users = llist_new();
    if (siridb->users == NULL)
    {
        return -1;  /* signal is raised */
    }

    /* get user access file name */
    SIRIDB_GET_FN(fn, SIRIDB_USERS_FN)

    if (!xpath_file_exist(fn))
    {
        /* we do not have a user access file, lets create the first user */
        user = siridb_user_new();
        if (user == NULL)
        {
            return -1;  /* signal is raised */
        }

        user->access_bit = SIRIDB_ACCESS_PROFILE_FULL;

        if (    siridb_user_set_name(siridb, user, "iris", err_msg) ||
                siridb_user_set_password(user, "siri", err_msg) ||
                siridb_users_add_user(siridb, user, err_msg))
        {
            log_error("%s", err_msg);
            siridb_user_decref(user);
            return -1;
        }

        return 0;
    }

    if ((unpacker = qp_unpacker_ff(fn)) == NULL)
    {
        return -1;  /* a signal is raised is case of a memory error */
    }

    /* unpacker will be freed in case macro fails */
    siridb_schema_check(SIRIDB_USERS_SCHEMA)

    int rc = 0;
    while (qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, &qp_name) == QP_RAW &&
            qp_next(unpacker, &qp_password) == QP_RAW &&
            qp_next(unpacker, &qp_access_bit) == QP_INT64)
    {
        user = siridb_user_new();
        if (user == NULL)
        {
            rc = -1;  /* signal is raised */
        }
        else
        {
            user->name = strndup(qp_name.via.raw, qp_name.len);
            user->password = strndup(qp_password.via.raw, qp_password.len);

            if (user->name == NULL || user->password == NULL)
            {
                ERR_ALLOC
                siridb_user_decref(user);
                rc = -1;
            }
            else
            {
                user->access_bit = (uint32_t) qp_access_bit.via.int64;
                if (llist_append(siridb->users, user))
                {
                    siridb_user_decref(user);
                    rc = -1;  /* signal is raised */
                }
            }
        }
    }

    /* free unpacker */
    qp_unpacker_ff_free(unpacker);

    return rc;
}

/*
 * Typedef: sirinet_clserver_get_file
 *
 * Returns the length of the content for a file and set buffer with the file
 * content. Note that malloc is used to allocate memory for the buffer.
 *
 * In case of an error -1 is returned and buffer will be set to NULL.
 */
ssize_t siridb_users_get_file(char ** buffer, siridb_t * siridb)
{
    /* get users file name */
    SIRIDB_GET_FN(fn, SIRIDB_USERS_FN)

    return xpath_get_content(buffer, fn);
}

/*
 * Destroy servers, parsing NULL is not allowed.
 */
void siridb_users_free(llist_t * users)
{
    llist_free_cb(users, (llist_cb) USERS_free, NULL);
}

/*
 * Returns 0 when successful,or -1 is returned in case of a critical
 * error. (a critical error also raises a signal). The err_msg will contain
 * the error in any case.
 */
int siridb_users_add_user(
        siridb_t * siridb,
        siridb_user_t * user,
        char * err_msg)
{
    /* add the user to the users */
    if (llist_append(siridb->users, user))
    {
        /* this is critical, a signal is raised */
        sprintf(err_msg, "Memory allocation error.");
        return -1;
    }

    if (siridb_users_save(siridb))
    {
        /* this is critical, a signal is raised */
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Could not save user '%s' to file.",
                user->name);
        log_critical(err_msg);
        return -1;
    }

    return 0;
}

/*
 * Returns NULL when the user is not found of when the given password is
 * incorrect. When *password is NULL the password will NOT be checked and
 * the user will be returned when found.
 */
siridb_user_t * siridb_users_get_user(
        llist_t * users,
        const char * name,
        const char * password)
{
    siridb_user_t * user;
    char * pw;
    struct crypt_data data;

    if ((user = llist_get(
            users,
            (llist_cb) USERS_cmp,
            (void *) name)) == NULL)
    {
        return NULL;
    }

    if (password == NULL)
    {
        return user;
    }


    data.initialized = 0;
    pw = crypt_r(password, user->password, &data);

    return (strcmp(pw, user->password) == 0) ? user : NULL;
}

/*
 * We get and remove the user in one code block so we do not need a dropped
 * flag on the user object.
 *
 * Returns 0 if successful. In case of an error -1 is returned and err_msg
 * is set to an appropriate value.
 */
int siridb_users_drop_user(
        siridb_t * siridb,
        const char * name,
        char * err_msg)
{
    siridb_user_t * user;

    if ((user = llist_remove(
            siridb->users,
            (llist_cb) USERS_cmp,
            (void *) name)) == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User '%s' does not exist.",
                name);
        return -1;
    }

    /* decrement reference for user object */
    siridb_user_decref(user);

    if (siridb_users_save(siridb))
    {
        log_critical(MSG_ERR_CANNOT_WRITE_USERS);
        sprintf(err_msg, MSG_ERR_CANNOT_WRITE_USERS);
        return -1;
    }

    return 0;
}

/*
 * Returns 0 if successful; EOF and a signal is raised in case an error occurred.
 */
int siridb_users_save(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;

    /* get user access file name */
    SIRIDB_GET_FN(fn, SIRIDB_USERS_FN)

    if (
        /* open a new user file */
        (fpacker = qp_open(fn, "w")) == NULL ||

        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int16(fpacker, SIRIDB_USERS_SCHEMA) ||

        /* we can and should skip this if we have no users to save */
        llist_walk(siridb->users, (llist_cb) USERS_save, fpacker) ||

        /* close file pointer */
        qp_close(fpacker))
    {
        ERR_FILE
        return EOF;
    }

    return 0;
}

/*
 * Returns 0 if successful and -1 in case an error occurred.
 */
static int USERS_save(siridb_user_t * user, qp_fpacker_t * fpacker)
{
    int rc = 0;

    rc += qp_fadd_type(fpacker, QP_ARRAY3);
    rc += qp_fadd_string(fpacker, user->name);
    rc += qp_fadd_string(fpacker, user->password);
    rc += qp_fadd_int32(fpacker, (int32_t) user->access_bit);
    return rc;
}

inline static int USERS_cmp(siridb_user_t * user, const char * name)
{
    return (strcmp(user->name, name) == 0);
}

static int USERS_free(siridb_user_t * user, void * args)
{
    siridb_user_decref(user);
    return 0;
}

