/*
 * owcrypt.h - One Way Encryption. (used for storing a database user password)
 */
#ifndef OWCRYPT_H_
#define OWCRYPT_H_

#define OWCRYPT_SZ 64
#define OWCRYPT_SALT_SZ 10

void owcrypt(const char * password, const char * salt, char * encrypted);
void owcrypt_gen_salt(char * salt);

#endif  /* OWCRYPT_H_ */
