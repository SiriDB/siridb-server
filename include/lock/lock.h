/*
 * lock.h - SiriDB Lock.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 13-07-2016
 *
 */
#pragma once

typedef enum
{
    LOCK_IS_LOCKED_ERR=-6,
    LOCK_PROCESS_NAME_ERR,
    LOCK_WRITE_ERR,
    LOCK_READ_ERR,
    LOCK_UNLINK_ERR,
    LOCK_MEM_ALLOC_ERR,
    LOCK_REMOVED=0,
    LOCK_NEW,
    LOCK_OVERWRITE
} lock_t;

lock_t lock_lock(const char * path);
lock_t lock_unlock(const char * path);
const char * lock_str(lock_t rc);
