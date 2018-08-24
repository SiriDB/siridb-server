/*
 * owcrypt.h - One Way Encryption. (used for storing a database user password)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-02-2017
 *
 */
#ifndef OWCRYPT_H_
#define OWCRYPT_H_

#define OWCRYPT_SZ 64
#define OWCRYPT_SALT_SZ 10

void owcrypt(const char * password, const char * salt, char * encrypted);
void owcrypt_gen_salt(char * salt);

#endif  /* OWCRYPT_H_ */
