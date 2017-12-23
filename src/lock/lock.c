/*
 * lock.c - SiriDB Lock.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * This module works only in Linux
 *
 * changes
 *  - initial version, 13-07-2016
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <lock/lock.h>
#include <xpath/xpath.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

static lock_t LOCK_create(const char * lock_fn, pid_t pid);
static int LOCK_get_process_name(char ** name, pid_t pid);

/*
 * Returns LOCK_NEW if a new lock is created, LOCK_OVERWRITE if an existing
 * lock file was overwritten or a negative result in case of an error.
 *
 * Flags:
 *
 *  LOCK_QUIT_IF_EXIST: do not check the lock file but simply consider the
 *                      database locked if a lock file exists in the path.
 */
lock_t lock_lock(const char * path, int flags)
{
    pid_t pid = getpid();
    pid_t ppid = 0;
    char * lock_fn;
    char * proc_name = NULL;
    char * pproc_name = NULL;
    char strpid[10];
    int is_locked;
    FILE * fp;
    lock_t lock_rc;

    /* initialize strpid to zero */
    memset(&strpid, 0, sizeof(strpid));

    if (asprintf(&lock_fn, "%s%s", path, ".lock") < 0)
    {
        return LOCK_MEM_ALLOC_ERR;
    }

    fp = fopen(lock_fn, "r");

    if (fp != NULL)
    {
        if (flags & LOCK_QUIT_IF_EXIST)
        {
            fclose(fp);
            free(lock_fn);
            return LOCK_IS_LOCKED_ERR;
        }

        if (fread(strpid, sizeof(char), 9, fp))
        {
            ppid = strtoul(strpid, NULL, 10);
        }
        fclose(fp);

        if (!ppid)
        {
            free(lock_fn);
            return LOCK_READ_ERR;
        }

        if (ppid == pid)
        {
            /*
             * The process id in the existing lock file is the same as 'this'
             * process, we therefore can overwrite the lock.
             */
            lock_rc = LOCK_create(lock_fn, pid);
            free(lock_fn);
            return lock_rc;
        }

        if (LOCK_get_process_name(&proc_name, pid))
        {
            free(lock_fn);
            return LOCK_MEM_ALLOC_ERR;
        }

        if (proc_name == NULL)
        {
            free(lock_fn);
            return LOCK_PROCESS_NAME_ERR;
        }

        if (LOCK_get_process_name(&pproc_name, ppid))
        {
            free(proc_name);
            free(lock_fn);
            return LOCK_MEM_ALLOC_ERR;
        }

        /*
         * If the process id in the lock file does not exist or if the
         * process id is in use by another program than siridb we can
         * assume the lock is not valid.
         */
        is_locked = (pproc_name != NULL && strcmp(proc_name, pproc_name) == 0);

        free(proc_name);
        free(pproc_name);

        if (is_locked)
        {
            free(lock_fn);
            return LOCK_IS_LOCKED_ERR;
        }
    }

    lock_rc = LOCK_create(lock_fn, pid);
    free(lock_fn);

    return lock_rc;
}

/*
 * Returns LOCK_REMOVED if successful or something else in case of an error.
 */
lock_t lock_unlock(const char * path)
{
    char * lock_fn;
    lock_t lock_rc;

    if (asprintf(&lock_fn, "%s%s", path, ".lock") < 0)
    {
        return LOCK_MEM_ALLOC_ERR;
    }

    lock_rc = (unlink(lock_fn)) ? LOCK_UNLINK_ERR : LOCK_REMOVED;
    free(lock_fn);

    return lock_rc;
}

/*
 * Returns a message by lock result code.
 */
const char * lock_str(lock_t rc)
{
    switch (rc)
    {
    case LOCK_IS_LOCKED_ERR:
        return "Database is locked by another process";
    case LOCK_PROCESS_NAME_ERR:
        return "Error occurred while reading process name";
    case LOCK_WRITE_ERR:
        return "Error occurred while writing the lock file";
    case LOCK_READ_ERR:
        return "Error occurred while reading existing lock file";
    case LOCK_UNLINK_ERR:
        return "Error occurred while removing the lock file";
    case LOCK_MEM_ALLOC_ERR:
        return "Memory allocation error occurred";
    case LOCK_REMOVED:
        return "Database is successfully unlocked";
    case LOCK_NEW:
        return "Database is successfully locked with a new lock file";
    case LOCK_OVERWRITE:
        return
            "Database is successfully locked but a lock file existed which "
            "indicates that the database was not closed correctly last time";
    }
    return "Unknown type";
}

/*
 * Returns LOCK_NEW if a new lock is created, LOCK_OVERWRITE if an existing
 * lock file was overwritten or LOCK_WRITE_ERR in case of an error.
 */
static lock_t LOCK_create(const char * lock_fn, pid_t pid)
{
    lock_t rc_success = (!xpath_file_exist(lock_fn)) ?
            LOCK_NEW : LOCK_OVERWRITE;

    FILE * fp = fopen(lock_fn, "w");
    if (fp == NULL)
    {
        return LOCK_WRITE_ERR;
    }
    if (fprintf(fp, "%d", pid) < 0)
    {
        fclose(fp);
        return LOCK_WRITE_ERR;
    }
    return (fclose(fp)) ? LOCK_WRITE_ERR : rc_success;
}

/*
 * Returns 0 if successful or -1 in case of a memory allocation error.
 *
 * When successful, name contains the process name by process id or NULL
 * if the process is not found.
 *
 * In case 'name' is not NULL, 'calloc' is used to set the value and should
 * be destroyed using free(). When -1 is returned 'name' is always NULL.
 */
#ifdef __APPLE__
static int LOCK_get_process_name(char ** name, pid_t pid)
{
    struct proc_bsdinfo proc;

    int st = proc_pidinfo(
            pid,
            PROC_PIDTBSDINFO,
            0,
            &proc,
            PROC_PIDTBSDINFO_SIZE);

    if (st == 0)
    {
        return 0;
    }
    else if (st != PROC_PIDTBSDINFO_SIZE)
    {
        return -1;
    }

    *name = strdup(proc.pbi_comm);

    if (*name == NULL)
    {
        return -1;
    }

    return 0;
}
#else
static int LOCK_get_process_name(char ** name, pid_t pid)
{
    size_t n = 1024;
    *name = (char *) calloc(n, sizeof(char));

    if (*name == NULL)
    {
        return -1;
    }
    else
    {
        sprintf(*name, "/proc/%d/comm", pid);
        FILE * fp = fopen(*name, "r");
        if (fp == NULL)
        {
            /* most likely the file does not exist */
            free(*name);
            *name = NULL;
        }
        else
        {
            ssize_t size = getline(name, &n, fp);
            fclose(fp);
            if(size > 0)
            {
                if((*name)[size - 1] == '\n')
                {
                    (*name)[size - 1] = 0;
                }
            } else if (size < 0)
            {
                free(*name);
                *name = NULL;
                return -1;
            }
        }
    }
    return 0;
}
#endif
