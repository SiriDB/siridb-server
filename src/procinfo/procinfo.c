/*
 * procinfo.c - Process info for the current running process.
 *
 * Got most information from:
 *
 * http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-
 *      memory-consumption-from-inside-a-process
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 18-03-2016
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <logger/logger.h>
#include <procinfo/procinfo.h>
#include <inttypes.h>

#ifdef __APPLE__
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <libproc.h>
#else
#include <xpath/xpath.h>
#endif


static long int parse_line(char * line);

/*
 * Unused function.
 */
long int procinfo_total_virtual_memory(void)
{
    /* Value is returned in KB */
    int64_t result = -1;
    FILE * file = fopen("/proc/self/status", "r");

    if (file != NULL)
    {
        char line[128];

        while (fgets(line, 128, file) != NULL)
        {
            if (strncmp(line, "VmSize:", 7) == 0)
            {
                result = parse_line(line);
                break;
            }
        }
        fclose(file);
    }

    return result;
}

#ifdef __APPLE__
long int procinfo_total_physical_memory(void)
{
    kern_return_t ret;
    mach_msg_type_number_t out_count;
    mach_task_basic_info_data_t taskinfo;

    taskinfo.virtual_size = 0;
    out_count = MACH_TASK_BASIC_INFO_COUNT;

    ret = task_info(
            mach_task_self(),
            MACH_TASK_BASIC_INFO,
            (task_info_t) &taskinfo,
            &out_count);

    return (ret == KERN_SUCCESS) ?
            (long int) taskinfo.resident_size / 1024 : -1;
}
#else
long int procinfo_total_physical_memory(void)
{
    /* Value is returned in KB */
    long int result = -1;
    FILE * file = fopen("/proc/self/status", "r");

    if (file != NULL)
    {
        char line[128];

        while (fgets(line, 128, file) != NULL)
        {
            if (strncmp(line, "VmRSS:", 6) == 0)
            {
                result = parse_line(line);
                break;
            }
        }
        fclose(file);
    }
    return result;
}
#endif

#ifdef __APPLE__
long int procinfo_open_files(const char * path)
{
    pid_t pid = getpid();
    size_t len = strlen(path);
    long int buffer_size = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, 0, 0);
    long int buffer_used, number_of_proc_fds, i, count;

    if (buffer_size < 0)
    {
        return -1;
    }

    struct proc_fdinfo * fd_info = (struct proc_fdinfo *) malloc(buffer_size);

    if (fd_info == NULL)
    {
        return -1;
    }

    buffer_used = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fd_info, buffer_size);
    number_of_proc_fds = buffer_used / PROC_PIDLISTFD_SIZE;

    for (i = 0, count = 0; i < number_of_proc_fds; i++)
    {
        if (fd_info[i].proc_fdtype == PROX_FDTYPE_VNODE)
        {
            struct vnode_fdinfowithpath vnode_info;

            int res = proc_pidfdinfo(
                    pid,
                    fd_info[i].proc_fd,
                    PROC_PIDFDVNODEPATHINFO,
                    &vnode_info,
                    PROC_PIDFDVNODEPATHINFO_SIZE);

            if (    res == PROC_PIDFDVNODEPATHINFO_SIZE &&
                    strncmp(path, vnode_info.pvip.vip_path, len) == 0)
            {
                count++;
            }
        }
    }
    free(fd_info);
    return count;
}
#else
long int procinfo_open_files(const char * path)
{
    long int count = 0;
    DIR * dirp;
    struct dirent * entry;
    size_t len = strlen(path);
    char buffer[SIRI_PATH_MAX];
    char buf[SIRI_PATH_MAX];

    if ((dirp = opendir("/proc/self/fd")) == NULL)
    {
        return -1;
    }

    while ((entry = readdir(dirp)) != NULL)
    {
        if (entry->d_type == DT_REG || entry->d_type == DT_LNK)
        {
            snprintf(buffer, SIRI_PATH_MAX, "/proc/self/fd/%s", entry->d_name);

            if (realpath(buffer, buf) == NULL)
            {
                continue;
            }

            if (strncmp(path, buf, len) == 0)
            {
                count++;
            }
        }
    }
    closedir(dirp);

    return count;
}
#endif

static long int parse_line(char * line)
{
    long int i = strlen(line);

    while (*line < '0' || *line > '9')
    {
        line++;
    }

    line[i - 3] = '\0';

    i = atol(line);

    return i;
}
