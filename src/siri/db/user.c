/*
 * user.c - contains functions for a SiriDB database member.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#define _GNU_SOURCE
#include <assert.h>
#include <crypt.h>
#include <logger/logger.h>
#include <siri/db/access.h>
#include <siri/db/db.h>
#include <siri/db/user.h>
#include <siri/db/users.h>
#include <siri/err.h>
#include <siri/grammar/grammar.h>
#include <strextra/strextra.h>
#include <string.h>

#define SIRIDB_MIN_PASSWORD_LEN 4
#define SIRIDB_MAX_PASSWORD_LEN 128
#define SIRIDB_MIN_USER_LEN 2
#define SIRIDB_MAX_USER_LEN 60

#define SEED_CHARS "./0123456789" \
    "abcdefghijklmnopqrstuvwxyz"  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

/*
 * Creates a new user with reference counter zero.
 *
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_user_t * siridb_user_new(void)
{
    siridb_user_t * user = (siridb_user_t *) malloc(sizeof(siridb_user_t));
    if (user == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        user->access_bit = 0;
        user->password = NULL;
        user->name = NULL;
        user->ref = 1;
    }
    return user;
}

/*
 * This function can raise a SIGNAL. In this case the packer is not filled
 * with the correct values.
 */
void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop)
{
    switch (prop)
    {
    case CLERI_GID_K_NAME:
        qp_add_string(packer, user->name);
        break;
    case CLERI_GID_K_ACCESS:
        {
            char buffer[SIRIDB_ACCESS_STR_MAX];
            siridb_access_to_str(buffer, user->access_bit);
            qp_add_string(packer, buffer);
        }
        break;
    }
}

/*
 * Returns 0 when successful or -1 if not.
 *
 * This function can have raised a SIGNAL when the result is -1 in case of
 * a memory allocation error.
 */
int siridb_user_set_password(
        siridb_user_t * user,
        const char * password,
        char * err_msg)
{
    char salt[] = "$1$........$";
    char * encrypted;
    struct crypt_data data;

    if (strlen(password) < SIRIDB_MIN_PASSWORD_LEN)
    {
        sprintf(err_msg, "Password should be at least %d characters.",
                SIRIDB_MIN_PASSWORD_LEN);
        return -1;
    }

    if (strlen(password) > SIRIDB_MAX_PASSWORD_LEN)
    {
        sprintf(err_msg, "Password should be at most %d characters.",
                SIRIDB_MAX_PASSWORD_LEN);
        return -1;
    }

    if (!strx_is_graph(password))
    {
        sprintf(err_msg,
                "Password contains illegal characters. (only graphical "
                "characters are allowed, no spaces, tabs etc.)");
        return -1;
    }
    /* encrypt the users password */
    for (size_t i = 3, len=strlen(SEED_CHARS); i < 11; i++)
    {
        salt[i] = SEED_CHARS[rand() % len];
    }

    data.initialized = 0;
    encrypted = crypt_r(password, salt, &data);

    /* replace user password with encrypted password */
    free(user->password);
    user->password = strdup(encrypted);
    if (user->password == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    return 0;
}

/*
 * Returns 0 when successful or a positive value in case the name is not valid.
 * A negative value is returned and a signal is raised in case a critical error
 * has occurred.
 *
 * (err_msg is set in case of all errors)
 */
int siridb_user_set_name(
        siridb_t * siridb,
        siridb_user_t * user,
        const char * name,
        char * err_msg)
{
    if (strlen(name) < SIRIDB_MIN_USER_LEN)
    {
        sprintf(err_msg, "User name should be at least %d characters.",
                SIRIDB_MIN_USER_LEN);
        return 1;
    }

    if (strlen(name) > SIRIDB_MAX_USER_LEN)
    {
        sprintf(err_msg, "User name should be at least %d characters.",
                SIRIDB_MAX_USER_LEN);
        return 1;
    }

    if (!strx_is_graph(name))
    {
        sprintf(err_msg,
                "User name contains illegal characters. (only graphical "
                "characters are allowed, no spaces, tabs etc.)");
        return 1;
    }

    if (siridb_users_get_user(siridb->users, name, NULL) != NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "User '%s' already exists.",
                name);
        return 1;
    }

    free(user->name);
    user->name = strdup(name);

    if (user->name == NULL)
    {
        ERR_ALLOC
        sprintf(err_msg, "Memory allocation error.");
        return -1;
    }

    return 0;
}

/*
 * Returns true (1) if the user has access or false (0) if not. In case the
 * user has no access, 'err_msg' is filled with an appropriate error message.
 *
 * Make sure 'err_msg' is a pointer to a string which can hold at least
 * SIRIDB_MAX_SIZE_ERR_MSG.
 */
int siridb_user_check_access(
        siridb_user_t * user,
        uint32_t access_bit,
        char * err_msg)
{
    if ((user->access_bit & access_bit) == access_bit)
    {
        return 1;  // true
    }

    char buffer[SIRIDB_ACCESS_STR_MAX];
    siridb_access_to_str(buffer, access_bit);
    snprintf(
            err_msg,
            SIRIDB_MAX_SIZE_ERR_MSG,
            "Access denied. User '%s' has no '%s' privileges.",
            user->name,
            buffer);

    return 0;   // false
}

int siridb_user_cexpr_cb(siridb_user_t * user, cexpr_condition_t * cond)
{
    /* str is not used for user */

    switch (cond->prop)
    {
    case CLERI_GID_K_ACCESS:
        return cexpr_int_cmp(cond->operator, user->access_bit, cond->int64);
    case CLERI_GID_K_NAME:
        return cexpr_str_cmp(cond->operator, user->name, cond->str);
    }

    /* this must NEVER happen */
    log_critical("Unknown user property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * Never call this function but use siridb_user_decref.
 *
 * Destroy user. (parsing NULL is not allowed)
 */
void siridb__user_free(siridb_user_t * user)
{
#ifdef DEBUG
    log_debug("Free user: '%s'", user->name);
#endif
    free(user->name);
    free(user->password);
    free(user);
}
