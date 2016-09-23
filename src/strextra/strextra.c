/*
 * strextra.c - Extra String functions used by SiriDB
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 19-03-2016
 *
 */
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <logger/logger.h>

void strx_lower_case(char * sptr)
{
   for (; *sptr != '\0'; sptr++)
        *sptr = tolower( (unsigned char) * sptr);
}

void strx_upper_case(char * sptr)
{
   for (; *sptr != '\0'; sptr++)
        *sptr = toupper( (unsigned char) * sptr);
}

void strx_replace_char(char * sptr, char orig, char repl)
{
    for (; *sptr != '\0'; sptr++)
        if (*sptr == orig) *sptr = repl;
}

void strx_trim(char ** str, char chr)
{
    /*
     * trim: when chr is 0 we will trim whitespace,
     * otherwise only the given char.
     */
    char * end;

    // trim leading chars
    while ((chr && **str == chr) || (!chr && isspace(**str)))
        (*str)++;

    // check all chars?
    if(**str == 0)
        return;

    // trim trailing chars
    end = *str + strlen(*str) - 1;
    while (end > *str &&
            ((chr && *end == chr) || (!chr && isspace(*end))))
        end--;

    // write new null terminator
    *(end + 1) = 0;
}

/*
 * returns true or false
 */
bool strx_is_empty(const char * str)
{
    const char * test = str;
    for (; *test; test++)
    {
        if (!isspace(*test))
        {
            return false;
        }
    }
    return true;
}

bool strx_is_int(const char * str)
{
   // Handle negative numbers.
   if (*str == '-')
       ++str;

   // Handle empty string or just "-".
   if (!*str)
       return false;

   // Check for non-digit chars in the rest of the string.
   while (*str)
   {
       if (!isdigit(*str))
           return false;
       else
           ++str;
   }

   return true;
}

bool strx_is_float(const char * str)
{
   // Handle negative numbers.
   if (*str == '-' || *str == '+')
   {
       ++str;
   }

   size_t dots = 0;

   // Handle empty string or just "-".
   if (!*str)
       return false;

   // Check for non-digit chars in the rest of the string.
   while (*str)
   {
       if (*str == '.')
       {
           ++dots;
       }
       else if (!isdigit(*str))
       {
           return false;
       }

       ++str;
   }

   return dots == 1;
}

bool strx_is_graph(const char * str)
{
    for (; *str; str++)
    {
        if (!isgraph(*str))
        {
            return false;
        }
    }
    return true;
}

/*
 * This function is used to extra a SiriDB string. These strings start
 * with " or with ' and we should replace double this character in the
 * string with a single one.
 *
 * 'dest' string will be terminated and the return value is the new
 * length of 'dest'.
 */
size_t strx_extract_string(char * dest, const char * source, size_t len)
{
    size_t i = 0;

    /* take the first character, this is " or ' */
    char chr = *source;

    /* we need to loop till len-2 so take 2 */
    for (len -= 2; i < len; i++)
    {
        source++;
        if (*source == chr)
        {
            /* this is the character, skip one and take the next */
            source++;
            len--;
        }
        dest[i] = *source;
    }

    /* set final 0 */
    dest[i] = 0;

    return i;
}

/*
 * Returns a string to double.
 * No error checking is done, we make the following assumptions:
 *      - len > 0
 *      - string is allowed to have one dot (.) at most but not required
 *      - string can start with a plus (+) or minus (-) sign.
 */
double strx_to_double(const char * src, size_t len)
{
    char * pt = (char *) src;
    double d = 0;
    double convert;

    switch (*pt)
    {
    case '-':
        convert = -1.0;
        pt++;
        break;
    case '+':
        pt++;
        /* no break */
    default:
        convert = 1.0;
    }

    uint64_t r1 = *pt - '0';

    while (--len && isdigit(*(++pt)))
    {
        r1 = 10 * r1 + *pt - '0';
    }

    d = (double) r1;

    if (--len && *(pt++) == '.')
    {
        uint64_t r2 = *pt - '0';
        ssize_t i;
        for (i = -1; --len && isdigit(*(++pt)); i--)
        {
             r2 = 10 * r2 + *pt - '0';
        }

        d += pow(10, i) * (double) r2;
    }

    return convert * d;
}

/*
 * Returns a string to uint64_t.
 * No error checking is done, we make the following assumptions:
 *      - len > 0
 *      - string can only contain characters [0..9] and no signs
 */
uint64_t strx_to_uint64(const char * src, size_t len)
{
    char * pt = (char *) src;

    uint64_t i = *pt - '0';

    while (--len && isdigit(*(++pt)))
    {
        i = 10 * i + *pt - '0';
    }

    return i;
}
