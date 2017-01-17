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
#include <stdlib.h>
#include <pcre.h>
#include <string.h>

static void KWCACHE_kw_match(
        cleri_kwcache_t * kwcache,
        cleri_parse_t * pr,
        const char * str);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_kwcache_t * cleri_kwcache_new(void)
{
    cleri_kwcache_t * kwcache;
    kwcache = (cleri_kwcache_t *) malloc(sizeof(cleri_kwcache_t));
    if (kwcache != NULL)
    {
        kwcache->len = 0;
        kwcache->str = NULL;
        kwcache->next = NULL;
    }
    return kwcache;
}

/*
 * Returns 0 when no kw_match is found, -1 when an error has occurred, or the
 * new kwcache->len value.
 */
ssize_t cleri_kwcache_match(
        cleri_parse_t * pr,
        const char * str)
{
    cleri_kwcache_t * kwcache = pr->kwcache;
    if (kwcache->str != NULL)
    {
        while (1)
        {
            if (str == kwcache->str)
            {
                return kwcache->len;
            }

            if (kwcache->next == NULL)
            {
                break;
            }
            kwcache = kwcache->next;
        }
        kwcache->next = (cleri_kwcache_t *) malloc(sizeof(cleri_kwcache_t));
        if (kwcache->next == NULL)
        {
            return -1;
        }
        kwcache = kwcache->next;
        kwcache->len = 0;
        kwcache->next = NULL;
    }

    kwcache->str = str;
    KWCACHE_kw_match(kwcache, pr, str);
    return kwcache->len;
}

/*
 * Destroy kwcache. (parsing NULL is allowed)
 */
void cleri_kwcache_free(cleri_kwcache_t * kwcache)
{
    cleri_kwcache_t * next;
    while (kwcache != NULL)
    {
        next = kwcache->next;
        free(kwcache);
        kwcache = next;
    }
}

/*
 * This function will set kwcache->len if a match is found.
 */
static void KWCACHE_kw_match(
        cleri_kwcache_t * kwcache,
        cleri_parse_t * pr,
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
    {
        return;
    }

    kwcache->len = sub_str_vec[1];
}
