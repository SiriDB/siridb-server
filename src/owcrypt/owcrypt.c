/*
 * owcrypt.c - One Way Encryption.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-02-2017
 *
 */
#include <stdlib.h>
#include <string.h>
#include <owcrypt/owcrypt.h>

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

/*
 * Encrypts a password using a salt.
 *
 * Usage:
 *
 *  char[OWCRYPT_SZ] encrypted;
 *  owcrypt("my_password", "my11chrsalt", encrypted);
 *
 *  // Checking can be done like this:
 *  char[OWCRYPT_SZ] pw;
 *  owcrypt("pasword_to_check", encrypted, pw");
 *  if (strcmp(pw, encrypted) === 0) {
 *      // valid
 *  }
 *
 * Parameters:
 *  const char * password: must be a terminated string
 *  const char * salt: must be a string with at least a length 11.
 *  char * encrypted: must be able to hold at least 96 chars.
 */
void owcrypt(const char * password, const char * salt, char * encrypted)
{
    unsigned char i, c, j;
    unsigned long long k;
    const char * p;
    const char * w;

    for (i = 0; i < OWCRYPT_SALT_SZ; i++)
    {
        encrypted[i] = salt[i];
    }

    encrypted[OWCRYPT_SALT_SZ] = '$';
    encrypted[OWCRYPT_SALT_SZ + 1] = '0';

    for (w = password, i = OWCRYPT_SALT_SZ + 2; i < OWCRYPT_SZ - 1; i++, w++)
    {
        if (!*w)
        {
            w = password;
        }

        for (k = 0, p = password; *p; p++)
        {
            for (c = 0; c < 11; c++)
            {
                j = salt[c] + *p + *w;
                k += j * P[(j + i + k) % 109];
            }
        }
        encrypted[i] = VCHARS[k % 64];
    }
    encrypted[OWCRYPT_SZ - 1] = '\0';
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
    for (i = 0; i < OWCRYPT_SALT_SZ; i++)
    {
        salt[i] = VCHARS[rand() % 64];
    }
}
