/*
 * owcrypt.c - One Way Encryption. (used for storing a database user password)
 *
 * purpose:
 *  - provide an encryption algorithm to prevent password guessing.
 *  - passwords should be stored using one way encryption so the original
 *    is lost.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <owcrypt/owcrypt.h>

#define OLD_SALT_SZ 8

#define CRYPTSET(x) t = encrypted[x] + k; encrypted[x] = VCHARS[t % 64]

#define VCHARS "./0123456789" \
    "abcdefghijklmnopqrstuvwxyz"  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static const int P[109] = {
        281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367,
        373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449,
        457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541, 547,
        557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631,
        641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727,
        733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823,
        827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919,
        929, 937, 941, 947, 953, 967, 971, 977, 983, 991, 997 };

static const unsigned int END = OWCRYPT_SZ - 1;

static void owcrypt0(
        const char * password,
        const char * salt,
        char * encrypted);
static void owcrypt1(
        const char * password,
        const char * salt,
        char * encrypted);


/*
 * Encrypts a password using a salt.
 *
 * Usage:
 *
 *  char[OWCRYPT_SZ] encrypted;
 *  owcrypt("my_password", "saltsalt$1", encrypted);
 *
 *  #  Checking can be done like this:
 *  char[OWCRYPT_SZ] pw;
 *  owcrypt("pasword_to_check", encrypted, pw");
 *  if (strcmp(pw, encrypted) === 0) {
 *      #  valid
 *  }
 *
 * Parameters:
 *  const char * password: must be a terminated string
 *  const char * salt: must be a string with at least a length OWCRYPT_SALT_SZ.
 *  char * encrypted: must be able to hold at least OWCRYPT_SZ chars.
 */
void owcrypt(const char * password, const char * salt, char * encrypted)
{
    switch (salt[OWCRYPT_SALT_SZ - 1])
    {
    case '0':
        /* deprecated version */
        owcrypt0(password, salt, encrypted);
        break;
    case '1':
        owcrypt1(password, salt, encrypted);
        break;
    default:
        encrypted[0] = '\0';
        break;
    }
}

/*
 * Generate a random salt.
 *
 * Make sure random is initialized, for example using:
 *  srand(time(NULL));
 */
void owcrypt_gen_salt(char * salt)
{
    int i;
    for (i = 0; i < OWCRYPT_SALT_SZ - 2; i++)
    {
        salt[i] = VCHARS[rand() % 64];
    }
    salt[OWCRYPT_SALT_SZ - 2] = '$';
    salt[OWCRYPT_SALT_SZ - 1] = '1';
}

static void owcrypt1(
        const char * password,
        const char * salt,
        char * encrypted)
{
    unsigned int i, j, c;
    unsigned long int k, t;
    unsigned const char * p, * w;

    memset(encrypted, 0, OWCRYPT_SZ);

    for (i = 0; i < OWCRYPT_SALT_SZ; i++)
    {
        encrypted[i] = salt[i];
    }

    for (   w = (unsigned char *) password, i = OWCRYPT_SALT_SZ;
            i < END;
            i++, w++)
    {
        if (!*w)
        {
            w = (unsigned char *) password;
        }

        for (k = 0, p = (unsigned char *) password; *p; p++)
        {
            for (c = 0; c < OWCRYPT_SALT_SZ; c++)
            {
                k += P[(salt[c] + *p + *w + i + k) % 109];
            }
        }
        CRYPTSET(i);

        for (j = k % 3 + OWCRYPT_SALT_SZ, c = k % 5 + 1; j < END; j += c)
        {
            CRYPTSET(j);
        }
    }
}

/* deprecated */
static void owcrypt0(
        const char * password,
        const char * salt,
        char * encrypted)
{
    unsigned int i, c, j;
    unsigned long long k;
    const char * p, * w;

    for (i = 0; i < OLD_SALT_SZ; i++)
    {
        encrypted[i] = salt[i];
    }

    encrypted[OLD_SALT_SZ] = '$';
    encrypted[OLD_SALT_SZ + 1] = '0';

    for (w = password, i = OLD_SALT_SZ + 2; i < OWCRYPT_SZ - 1; i++, w++)
    {
        if (!*w)
        {
            w = password;
        }

        for (k = 0, p = password; *p; p++)
        {
            for (c = 0; c < OLD_SALT_SZ; c++)
            {
                j = salt[c] + *p + *w;
                k += j * P[(j + i + k) % 109];
            }
        }
        encrypted[i] = VCHARS[k % 64];
    }
    encrypted[OWCRYPT_SZ - 1] = '\0';
}
