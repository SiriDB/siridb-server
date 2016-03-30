/*
 * kwcache.c - holds keyword regular expression result while parsing.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/kwcache.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <pcre.h>
#include <string.h>

static void pcre_kw_match(
        cleri_kwcache_t * kwcache,
        cleri_parse_result_t * pr,
        const char * str);


cleri_kwcache_t * cleri_new_kwcache(void)
{
    cleri_kwcache_t * kwcache;
    kwcache = (cleri_kwcache_t *) malloc(sizeof(cleri_kwcache_t));
    kwcache->len = 0;
    kwcache->str = NULL;
    kwcache->next = NULL;
    return kwcache;
}

size_t cleri_kwcache_match(
        cleri_parse_result_t * pr,
        const char * str)
{
    /* returns 0 when no kw_match is found */

    cleri_kwcache_t * kwcache = pr->kwcache;
    if (kwcache->str != NULL)
    {
        while (1)
        {
            if (str == kwcache->str)
                return kwcache->len;
            if (kwcache->next == NULL)
                break;
            kwcache = kwcache->next;
        }
        kwcache->next = (cleri_kwcache_t *) malloc(sizeof(cleri_kwcache_t));
        kwcache = kwcache->next;
        kwcache->len = 0;
        kwcache->next = NULL;
    }

    kwcache->str = str;
    pcre_kw_match(kwcache, pr, str);
    return kwcache->len;
}

void cleri_free_kwcache(cleri_kwcache_t * kwcache)
{
    cleri_kwcache_t * next;
    while (kwcache != NULL)
    {
        next = kwcache->next;
        free(kwcache);
        kwcache = next;
    }
}

static void pcre_kw_match(
        cleri_kwcache_t * kwcache,
        cleri_parse_result_t * pr,
        const char * str)
{
    int pcre_exec_ret;
    int sub_str_vec[2];
    pcre_exec_ret = pcre_exec(
                pr->re_keywords,
                pr->re_kw_extra,
                str,
                strlen(str),
                0,                     // start looking at this point
                0,                     // OPTIONS
                sub_str_vec,
                2);                    // length of sub_str_vec
    if (pcre_exec_ret != 1)
        return;

    kwcache->len = sub_str_vec[1];
}
