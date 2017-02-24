/*
 * owcrypt.h - One Way Encryption.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 24-02-2017
 *
 */
#pragma once

#define OWCRYPT_SZ 96
#define OWCRYPT_SALT_SZ 11

void owcrypt(const char * password, const char * salt, char * encrypted);
void owcrypt_gen_salt(char * salt);
