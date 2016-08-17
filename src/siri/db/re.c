/*
 * re.c - Helpers for regular expressions
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 04-08-2016
 *
 */

#include <siri/db/re.h>
#include <siri/db/db.h>
#include <assert.h>

/*
 * Compiles both a 'pcre' regular expression and 'pcre_extra' optimization if
 * the expression could be optimized.
 *
 * When successful, 0 is returned. In case of an error, -1 is returned and
 * the 'err_msg' is set to an appropriate error message. Both 'regex' and
 * 'regex_extra' are NULL when an error is returned.
 *
 * (SIRIDB_MAX_SIZE_ERR_MSG is honored for the error message)
 */
int siridb_re_compile(
        pcre ** regex,
        pcre_extra ** regex_extra,
        const char * source,
        size_t len,
        char * err_msg)
{
    LOGC("HERE1");
    int options = 0;
    const char * pcre_error_str;
    int pcre_error_offset;
    char pattern[len + 1];
    memcpy(pattern, source, len);
    pattern[0] = '^';

    switch (pattern[--len])
    {
    case 'i':
        options = PCRE_CASELESS;
        len--;
        /* no break */
    case '/':
        pattern[len] = '$';
        pattern[len + 1] = '\0';
        break;
    default:
        /* by the grammar definition, only the optional 'i' is allowed */
        assert (0);
        break;
    }

    *regex = pcre_compile(
                pattern,
                options,
                &pcre_error_str,
                &pcre_error_offset,
                NULL);

    if (*regex == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot compile regular expression '%s': %s",
                pattern,
                pcre_error_str);

        return -1;
    }

    *regex_extra = pcre_study(*regex, 0, &pcre_error_str);

    /*
     * pcre_study() returns NULL for both errors and when it can not
     * optimize the regex.  The last argument is how one checks for
     * errors (it is NULL if everything works, and points to an error
     * string otherwise.
     */
    if(pcre_error_str != NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot compile regular expression '%s': %s",
                pattern,
                pcre_error_str);

        /* free and set regex back to NULL */
        free(*regex);
        *regex = NULL;

        return -1;
    }
    LOGC("HERE2");
    return 0;
}
