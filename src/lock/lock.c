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
#define _GNU_SOURCE
#include <lock/lock.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lock_t LOCK_create(const char * lock_fn, pid_t pid);
static int LOCK_get_process_name(char ** name, pid_t pid);

/*
 * Returns LOCK_NEW if a new lock is created, LOCK_OVERWRITE if an existing
 * lock file was overwritten or a negative result in case of an error.
 */
lock_t lock_lock(const char * path)
{
    pid_t pid = getpid();
    pid_t ppid = 0;
    char * lock_fn;
    char * proc_name;
    char * pproc_name;
    char strpid[10] = {0};
    int name_cmp;

    if (asprintf(&lock_fn, "%s%s", path, ".lock") < 0)
    {
        return LOCK_MEM_ALLOC_ERR;
    }

    if (access(lock_fn, F_OK) != -1)
    {
        FILE * fp = fopen(lock_fn, "r");

        if (fp != NULL)
        {
            if (fread(strpid, sizeof(char), 9, fp))
            {
                ppid = strtoul(strpid, NULL, 10);
            }
            fclose(fp);
        }

        if (!ppid)
        {
            free(lock_fn);
            return LOCK_READ_ERR;
        }

        if (ppid == pid)
        {
            return LOCK_create(lock_fn, pid);
        }

        if (LOCK_get_process_name(&proc_name, pid))
        {
            return LOCK_MEM_ALLOC_ERR;
        }

        if (proc_name == NULL)
        {
            return LOCK_PROCESS_NAME_ERR;
        }

        if (LOCK_get_process_name(&pproc_name, ppid))
        {
            free(proc_name);
            return LOCK_MEM_ALLOC_ERR;
        }

        name_cmp = (pproc_name != NULL && strcmp(proc_name, pproc_name) == 0);

        free(proc_name);
        free(pproc_name);

        if (name_cmp)
        {
            return LOCK_IS_LOCKED_ERR;
        }
    }

    return LOCK_create(lock_fn, pid);
}

/*
 * Returns LOCK_REMOVED if successful or something else in case of an error.
 */
lock_t lock_unlock(const char * path)
{
    char * lock_fn;
    if (asprintf(&lock_fn, "%s%s", path, ".lock") < 0)
    {
        return LOCK_MEM_ALLOC_ERR;
    }
    return (unlink(lock_fn)) ? LOCK_UNLINK_ERR : LOCK_REMOVED;
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
    lock_t rc_success = (access(lock_fn, F_OK) == -1) ?
            LOCK_NEW : LOCK_OVERWRITE;

    FILE * fp = fopen(lock_fn, "w");
    if (fp == NULL)
    {
        return -1;
    }
    if (fprintf(fp, "%d", pid) < 0)
    {
        fclose(fp);
        return -1;
    }
    return (fclose(fp)) ? LOCK_WRITE_ERR : rc_success;
}

/*
 * Returns 0 if successful or -1 in case of a memory allocation error.
 *
 * When successful, name contains the process name by process id or NULL
 * if the process is not found.
 */
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
                return -1;
            }
        }
    }
    return 0;
}
