/*
 * access.h - Access constants
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-03-2016
 *
 */
#pragma once

#define SIRIDB_ACCESS_SELECT    1
#define SIRIDB_ACCESS_SHOW      2
#define SIRIDB_ACCESS_LIST      4
#define SIRIDB_ACCESS_CREATE    8
#define SIRIDB_ACCESS_INSERT    16
#define SIRIDB_ACCESS_DROP      32
#define SIRIDB_ACCESS_COUNT     64
#define SIRIDB_ACCESS_GRANT     128
#define SIRIDB_ACCESS_REVOKE    256
#define SIRIDB_ACCESS_ALTER     512
#define SIRIDB_ACCESS_PAUSE     1024
#define SIRIDB_ACCESS_CONTINUE  2048

