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

#include <siri/db/user.h>
#include <crypt.h>
#include <string.h>
#include <siri/grammar/grammar.h>
#include <strextra/strextra.h>
#include <siri/db/access.h>
#include <siri/db/db.h>
#include <assert.h>
#include <logger/logger.h>

#define SIRIDB_MIN_PASSWORD_LEN 4
#define SIRIDB_MAX_PASSWORD_LEN 128

#define SEED_CHARS "./0123456789" \
    "abcdefghijklmnopqrstuvwxyz"  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void USER_free(siridb_user_t * user);

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
        user->username = NULL;
        user->ref = 0;
    }
    return user;
}

/*
 * Increment the user reference counter.
 */
inline void siridb_user_incref(siridb_user_t * user)
{
    user->ref++;
}

/*
 * Decrement user reference counter and free the user when zero is reached.
 */
void siridb_user_decref(siridb_user_t * user)
{
    if (!--user->ref)
    {
        USER_free(user);
    }
}

/*
 * This function can raise a SIGNAL. In this case the packer is not filled
 * with the correct values.
 */
void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop)
{
    switch (prop)
    {
    case CLERI_GID_K_USER:
        qp_add_string(packer, user->username);
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
    encrypted = crypt(password, salt);

    /* replace user password with encrypted password */
    user->password = strdup(encrypted);
    if (user->password == NULL)
    {
        ERR_ALLOC
        return -1;
    }

    return 0;
}

int siridb_user_check_access(
        siridb_user_t * user,
        siridb_access_t access_bit,
        char * err_msg)
{
    if (user->access_bit & access_bit)
        return 1;

    char buffer[SIRIDB_ACCESS_STR_MAX];
    siridb_access_to_str(buffer, access_bit);
    snprintf(
            err_msg,
            SIRIDB_MAX_SIZE_ERR_MSG,
            "Access denied. User '%s' has no '%s' privileges.",
            user->username,
            buffer);

    return 0;
}

int siridb_user_cexpr_cb(siridb_user_t * user, cexpr_condition_t * cond)
{
    /* str is not used for user */

    switch (cond->prop)
    {
    case CLERI_GID_K_ACCESS:
        return cexpr_int_cmp(cond->operator, user->access_bit, cond->int64);
    case CLERI_GID_K_USER:
        return cexpr_str_cmp(cond->operator, user->username, cond->str);
    }

    /* this must NEVER happen */
    log_critical("Unknown user property received: %d", cond->prop);
    assert (0);
    return -1;
}

/*
 * Destroy user. (parsing NULL is not allowed)
 */
static void USER_free(siridb_user_t * user)
{
#ifdef DEBUG
    log_debug("Free user: %s", user->username);
#endif
    free(user->username);
    free(user->password);
    free(user);
}
