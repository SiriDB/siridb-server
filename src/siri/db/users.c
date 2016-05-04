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

#define SIRIDB_MIN_PASSWORD_LEN 4
#define SIRIDB_MAX_PASSWORD_LEN 128

#define SIRIDB_USER_ACCESS_SCHEMA 1

#define SEED_CHARS "./0123456789" \
    "abcdefghijklmnopqrstuvwxyz"  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

#define SIRIDB_USER_ACCESS_FN "useraccess.dat"

/* do not forget to run 'siridb_free_user()' on the returned user */
static siridb_user_t * pop_user(
        siridb_t * siridb,
        const char * username);

static siridb_user_t * get_user(
        struct siridb_s * siridb,
        const char * username);

static void append_user(siridb_t * siridb, siridb_user_t * user);

static int save_users(siridb_t * siridb);

static siridb_users_t * new_users(void);

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
    siridb->users = new_users();

    /* get user access file name */
    siridb_get_fn(fn, SIRIDB_USER_ACCESS_FN)

    if (access(fn, R_OK) == -1)
    {
        /* we do not have a user access file, lets create the first user */
        user = siridb_user_new();
        user->username = strdup("iris");
        user->password = strdup("siri");

        user->access_bit = 0;
        if (siridb_users_add_user(siridb, user, err_msg))
        {
            log_error("%s", err_msg);
            free(user);
            return 1;
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

        user->username = strndup(username->via->raw, username->len);
        user->password = strndup(password->via->raw, password->len);

        user->access_bit = (uint32_t) access_bit->via->int64;
        append_user(siridb, user);
    }

    /* free objects */
    qp_free_object(username);
    qp_free_object(password);
    qp_free_object(access_bit);

    /* free unpacker */
    qp_free_unpacker(unpacker);

    return 0;
}

void siridb_users_free(siridb_users_t * users)
{
    siridb_users_t * next;
    while (users != NULL)
    {
        next = users->next;
        siridb_user_free(users->user);
        free(users);
        users = next;
    }
}

int siridb_users_add_user(
        siridb_t * siridb,
        siridb_user_t * user,
        char * err_msg)
{
    char salt[] = "$1$........$";
    char * encrypted;

    if (strlen(user->password) < SIRIDB_MIN_PASSWORD_LEN)
    {
        sprintf(err_msg, "Password should be at least %d characters.",
                SIRIDB_MIN_PASSWORD_LEN);
        return 1;
    }

    if (strlen(user->password) > SIRIDB_MAX_PASSWORD_LEN)
    {
        sprintf(err_msg, "Password should be at most %d characters.",
                SIRIDB_MAX_PASSWORD_LEN);
        return 1;
    }

    if (!strx_is_graph(user->password))
    {
        sprintf(err_msg,
                "Password contains illegal characters. (only graphical "
                "characters are allowed, no spaces, tabs etc.)");
        return 1;
    }

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

    if (get_user(siridb, user->username) != NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User name '%s' already exists.",
                user->username);
        return 1;
    }

    /* encrypt the users password */
    for (size_t i = 3, len=strlen(SEED_CHARS); i < 11; i++)
        salt[i] = SEED_CHARS[rand() % len];
    encrypted = crypt(user->password, salt);

    /* replace user password with encrypted password */
    user->password = strdup(encrypted);

    /* add the user to the users */
    append_user(siridb, user);

    if (save_users(siridb))
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Could not save user '%s' to file.",
                user->username);
        pop_user(siridb, user->username);
        return 1;
    }

    return 0;
}

siridb_user_t * siridb_users_get_user(
        struct siridb_s * siridb,
        const char * username,
        const char * password)
{
    siridb_user_t * user;
    char * pw;

    if ((user = get_user(siridb, username)) == NULL)
        return NULL;

    pw = crypt(password, user->password);

    return (strcmp(pw, user->password) == 0) ? user : NULL;
}

int siridb_users_drop_user(
        struct siridb_s * siridb,
        const char * username,
        char * err_msg)
{
    siridb_user_t * user;

    if ((user = pop_user(siridb, username)) == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User '%s' does not exist.",
                username);
        return 1;
    }

    /* free user object */
    siridb_user_free(user);

    if (save_users(siridb))
        log_critical("Could not save users!");

    return 0;
}

static siridb_user_t * get_user(siridb_t * siridb, const char * username)
{
    siridb_users_t * current;

    current = siridb->users;

    if (current->user == NULL)
        return NULL;

    for (; current != NULL; current = current->next)
    {
        if (strcmp(current->user->username, username) == 0)
            return current->user;
    }
    return NULL;
}

static siridb_user_t * pop_user(
        siridb_t * siridb,
        const char * username)
{
    siridb_users_t * current;
    siridb_users_t * prev = NULL;
    siridb_user_t * user;

    current = siridb->users;

    /* we do not have any user so nothing to delete */
    if (current->user == NULL)
        return NULL;


    for (; current != NULL; prev = current, current = current->next)
    {
        if (strcmp(current->user->username, username) == 0)
        {
            if (prev == NULL)
                siridb->users = (current->next != NULL) ?
                        current->next : new_users();
            else
                prev->next = current->next;

            user = current->user;
            free(current);
            return user;
        }
    }
    /* cannot not find the user */
    return NULL;
}

/* we do not check here if the user already exists, so be sure this
 * is done before appending the user.
 */
static void append_user(siridb_t * siridb, siridb_user_t * user)
{
    siridb_users_t * current = siridb->users;

    if (current->user == NULL)
    {
        current->user = user;
        return;
    }

    while (current->next != NULL)
        current = current->next;

    current->next = (siridb_users_t *) malloc(sizeof(siridb_users_t));
    current->next->user = user;
    current->next->next = NULL;
}

static int save_users(siridb_t * siridb)
{
    qp_fpacker_t * fpacker;
    siridb_users_t * current = siridb->users;

    /* get user access fine name */
    siridb_get_fn(fn, SIRIDB_USER_ACCESS_FN)

    if ((fpacker = qp_open(fn, "w")) == NULL)
        return 1;

    /* open a new array */
    qp_fadd_type(fpacker, QP_ARRAY_OPEN);

    /* write the current schema */
    qp_fadd_int8(fpacker, SIRIDB_USER_ACCESS_SCHEMA);

    /* we can and should skip this if we have no users to save */
    if (current->user != NULL)
    {
        while (current != NULL)
        {
            qp_fadd_type(fpacker, QP_ARRAY3);
            qp_fadd_string(fpacker, current->user->username);
            qp_fadd_string(fpacker, current->user->password);
            qp_fadd_int32(fpacker, (int32_t) current->user->access_bit);
            current = current->next;
        }
    }

    /* close file pointer */
    qp_close(fpacker);

    return 0;
}

static siridb_users_t * new_users(void)
{
    siridb_users_t * users =
            (siridb_users_t *) malloc(sizeof(siridb_users_t));
    users->user = NULL;
    users->next = NULL;
    return users;
}
