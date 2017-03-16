/*
 * user.c - SiriDB Administrative User.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 16-03-2017
 *
 */
#include <siri/admin/user.h>
#include <stddef.h>
#include <owcrypt/owcrypt.h>

#define SIRI_ADMIN_USER_SCHEMA 1
#define FILENAME ".users.dat"

static int USER_free(siri_admin_user_t * user, void * args);
static int USER_save(siri_admin_user_t * user, qp_fpacker_t * fpacker);
inline static int USER_cmp(siri_admin_user_t * user, qp_obj_t * qp_name);

/*
 * Initialize siri->users. Returns 0 if successful or -1 in case of an error.
 * (a signal might be raised in case of an error)
 */
int siri_admin_user_init(siri_t * siri)
{
    qp_unpacker_t * unpacker;
    qp_obj_t qp_schema;
    qp_obj_t qp_name;
    qp_obj_t qp_password;
    int rc = 0;

    /* get administrative users file name */
    char fn[strlen(siri->cfg->default_db_path) + strlen(FILENAME) + 1];

    /* initialize linked list */
    siri->users = llist_new();
    if (siri->users == NULL)
    {
        return -1;
    }

    /* make filename */
    sprintf(fn, "%s%s", siri->cfg->default_db_path, FILENAME);

    if (!xpath_file_exist(fn))
    {
        /* missing file, lets create the first user */
        return (siri_admin_user_new(siri, "iris", "siri", 0) ||
                siri_admin_user_save(siri));
    }

    if ((unpacker = qp_unpacker_ff(fn)) == NULL)
    {
        return -1;  /* a signal is raised is case of a memory error */
    }

    if (    !qp_is_array(qp_next(unpacker, NULL)) ||
            qp_next(unpacker, &qp_schema) != QP_INT64 ||
            qp_schema.via.int64 != SIRI_ADMIN_USER_SCHEMA)
    {
        log_critical("Invalid schema detected in '%s'", fn);
        qp_unpacker_ff_free(unpacker);
        return -1;
    }

    while ( rc == 0 &&
            qp_is_array(qp_next(unpacker, NULL)) &&
            qp_next(unpacker, &qp_name) == QP_RAW &&
            qp_next(unpacker, &qp_password) == QP_RAW &&
            qp_password.len > 12)  /* old and new passwords are > 12 */
    {
        rc = siri_admin_user_new(siri, &qp_name, &qp_password, 1);
    }

    return rc;
}

/*
 * Creates a new administrative user and returns 0 if successful. In case of
 * an error, -1 is returned and a signal might be raised. When successful, the
 * user is added to the siri->users linked list.
 *
 * is_encrypted should be none-zero if the password is not encrypted yet.
 *
 * Note: the user will not be saved to disk. Call siri_admin_user_save() to
 *       save a new administrative user.
 */
int siri_admin_user_new(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password,
        int is_encrypted)
{
    siri_admin_user_t * user;

    user = (siri_admin_user_t *) llist_get(
            siri->users,
            (llist_cb) USER_cmp,
            (void *) qp_name);

    if (user != NULL)
    {
        return -1; /* user already exists */
    }

    user = (siri_admin_user_t *) malloc(sizeof(siri_admin_user_t));
    if (user == NULL)
    {
        return -1; /* memory allocation error */
    }

    user->name = strndup(qp_name->via->raw, qp_name->len);
    user->password = strndup(qp_password->via->raw, qp_password->len);

    if (!is_encrypted && user->password != NULL)
    {
        char encrypted[OWCRYPT_SZ];
        char salt[OWCRYPT_SALT_SZ];

        /* generate a random salt */
        owcrypt_gen_salt(salt);

        /* encrypt the users password */
        owcrypt(user->password, salt, encrypted);

        /* replace with encrypted password */
        free(user->password);
        user->password = strndup(encrypted);
    }

    if (    user->name == NULL ||
            user->password == NULL ||
            llist_append(siri->users, user))
    {
        USER_free(user, NULL);
        return -1; /* memory allocation error */
    }
    return 0;
}

/*
 * Returns 0 if the user/password is valid or another value if not.
 */
int siri_admin_user_check(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password)
{
    siri_admin_user_t * user;
    char pw[OWCRYPT_SZ];
    char * password;

    user = (siri_admin_user_t *) llist_get(
            siri->users,
            (llist_cb) USER_cmp,
            (void *) qp_name);

    if (user == NULL)
    {
        return -1;
    }

    password= strndup(qp_password->via->raw, qp_password->len);

    if (password == NULL)
    {
        return -1;
    }

    owcrypt(password, user->password, pw);
    free(password);

    return strcmp(pw, user->password);
}

/*
 * Returns 0 if the password is successful changed or -1 if not.
 *
 * Note: the password change is not saved, call siri_admin_user_save().
 */
int siri_admin_user_change_password(
        siri_t * siri,
        qp_obj_t * qp_name,
        qp_obj_t * qp_password)
{
    siri_admin_user_t * user;
    char encrypted[OWCRYPT_SZ];
    char salt[OWCRYPT_SALT_SZ];
    char * password;

    user = (siri_admin_user_t *) llist_get(
            siri->users,
            (llist_cb) USER_cmp,
            (void *) qp_name);

    if (user == NULL)
    {
        return -1;
    }

    password= strndup(qp_password->via->raw, qp_password->len);

    if (password == NULL)
    {
        return -1;
    }

    /* generate a random salt */
    owcrypt_gen_salt(salt);

    /* encrypt the users password */
    owcrypt(password, salt, encrypted);

    free(password);

    password = strdup(encrypted);
    if (password == NULL)
    {
        return -1;
    }

    /* replace user password with new encrypted password */
    free(user->password);
    user->password = password;

    return 0;
}

/*
 * Returns 0 if dropped or -1 in case the user was not found.
 *
 * Note: users are not saved, call siri_admin_user_save().
 */
int siri_admin_user_drop(siri_t * siri, qp_obj_t * qp_name)
{
    siri_admin_user_t * user;
    user = (siri_admin_user_t *) llist_remove(
            siri->users,
            (llist_cb) USER_cmp,
            NULL);
    if (user == NULL)
    {
        return -1;
    }

    USER_free(user);
    return 0;
}

/*
 * Destroy administrative users. siri->users is allowed to be NULL.
 */
void siri_admin_user_destroy(siri_t * siri)
{
    if (siri->users != NULL)
    {
        llist_free_cb(siri->users, (llist_cb) USER_free, NULL);
    }
}

/*
 * Returns 0 if successful or EOF if not.
 */
int siri_admin_user_save(siri_t * siri)
{
    qp_fpacker_t * fpacker;

    /* get administrative users file name */
    char fn[strlen(siri->cfg->default_db_path) + strlen(FILENAME) + 1];

    /* make filename */
    sprintf(fn, "%s%s", siri->cfg->default_db_path, FILENAME);

    if (
        /* open a new user file */
        (fpacker = qp_open(fn, "w")) == NULL ||

        /* open a new array */
        qp_fadd_type(fpacker, QP_ARRAY_OPEN) ||

        /* write the current schema */
        qp_fadd_int16(fpacker, SIRI_ADMIN_USER_SCHEMA) ||

        /* we can and should skip this if we have no users to save */
        llist_walk(siri->users, (llist_cb) USER_save, fpacker) ||

        /* close file pointer */
        qp_close(fpacker))
    {
        return EOF;
    }
    return 0;
}

static int USER_free(siri_admin_user_t * user, void * args)
{
    free(user->name);
    free(user->password);
    free(user);
    return 0;
}

/*
 * Returns 0 if successful and -1 in case an error occurred.
 */
static int USER_save(siri_admin_user_t * user, qp_fpacker_t * fpacker)
{
    int rc = 0;

    rc += qp_fadd_type(fpacker, QP_ARRAY2);
    rc += qp_fadd_string(fpacker, user->name);
    rc += qp_fadd_string(fpacker, user->password);

    return rc;
}

inline static int USER_cmp(siri_admin_user_t * user, qp_obj_t * qp_name)
{
    return (strncmp(user->name, qp_name->via->raw, qp_name->len) == 0);
}
