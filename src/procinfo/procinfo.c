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

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

static int parse_line(char * line);

int procinfo_total_virtual_memory(void)
{
    /* Value is returned in KB */
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
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
    return result;
}

int procinfo_total_physical_memory(void)
{
    /* Value is returned in KB */
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
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
    return result;
}

static int parse_line(char * line)
{
    int i = strlen(line);
    while (*line < '0' || *line > '9')
        line++;

    line[i - 3] = '\0';

    i = atoi(line);

    return i;
}
