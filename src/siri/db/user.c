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
#include <siri/grammar/grammar.h>
#include <strextra/strextra.h>

#define SIRIDB_MIN_PASSWORD_LEN 4
#define SIRIDB_MAX_PASSWORD_LEN 128

#define SEED_CHARS "./0123456789" \
    "abcdefghijklmnopqrstuvwxyz"  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

siridb_user_t * siridb_user_new(void)
{
    siridb_user_t * user = (siridb_user_t *) malloc(sizeof(siridb_user_t));
    user->access_bit = 0;
    user->password = NULL;
    user->username = NULL;
    return user;
}

void siridb_user_free(siridb_user_t * user)
{
    if (user != NULL)
    {
        free(user->username);
        free(user->password);
        free(user);
    }
}

void siridb_user_prop(siridb_user_t * user, qp_packer_t * packer, int prop)
{
    switch (prop)
    {
    case CLERI_GID_K_USER:
        qp_add_string(packer, user->username);
        break;
    case CLERI_GID_K_ACCESS:
        qp_add_string(packer, "dummy");
        break;
    }
}

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
        return 1;
    }

    if (strlen(password) > SIRIDB_MAX_PASSWORD_LEN)
    {
        sprintf(err_msg, "Password should be at most %d characters.",
                SIRIDB_MAX_PASSWORD_LEN);
        return 1;
    }

    if (!strx_is_graph(password))
    {
        sprintf(err_msg,
                "Password contains illegal characters. (only graphical "
                "characters are allowed, no spaces, tabs etc.)");
        return 1;
    }
    /* encrypt the users password */
    for (size_t i = 3, len=strlen(SEED_CHARS); i < 11; i++)
        salt[i] = SEED_CHARS[rand() % len];
    encrypted = crypt(password, salt);

    /* replace user password with encrypted password */
    user->password = strdup(encrypted);

    return 0;
}
