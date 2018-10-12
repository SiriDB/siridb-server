/*
 * lock.h - Lock a directory by using a lock file.
 */
#ifndef LOCK_H_
#define LOCK_H_

#define LOCK_QUIT_IF_EXIST 1

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

lock_t lock_lock(const char * path, int flags);
lock_t lock_unlock(const char * path);
const char * lock_str(lock_t rc);

#endif  /* LOCK_H_ */
