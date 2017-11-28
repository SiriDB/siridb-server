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
#include <assert.h>
#include <siri/db/db.h>
#include <siri/db/re.h>

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
        pcre2_code ** regex,
        pcre2_match_data ** match_data,
        const char * source,
        size_t len,
        char * err_msg)
{
    int options = 0;
    int pcre_error_num;
    PCRE2_SIZE pcre_error_offset;
    char pattern[len + 1];
    memcpy(pattern, source, len);
    pattern[0] = '^';

    switch (pattern[--len])
    {
    case 'i':
        options |= PCRE2_CASELESS;
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

    *regex = pcre2_compile(
                (PCRE2_SPTR8) pattern,
                PCRE2_ZERO_TERMINATED,
                options,
                &pcre_error_num,
                &pcre_error_offset,
                NULL);

    if (*regex == NULL)
    {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(pcre_error_num, buffer, sizeof(buffer));
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot compile regular expression '%s': %s",
                pattern,
                buffer);

        return -1;
    }

    *match_data = pcre2_match_data_create_from_pattern(*regex, NULL);

    /*
     * pcre_study() returns NULL for both errors and when it can not
     * optimize the regex.  The last argument is how one checks for
     * errors (it is NULL if everything works, and points to an error
     * string otherwise.
     */
    if(*match_data == NULL)
    {
        snprintf(err_msg,
                SIRIDB_MAX_SIZE_ERR_MSG,
                "Cannot create match data for regular expression '%s'",
                pattern);

        /* free and set regex back to NULL */
        pcre2_code_free(*regex);
        *regex = NULL;

        return -1;
    }

    return 0;
}
