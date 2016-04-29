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


void lower_case(char * sptr)
{
   for (; *sptr != '\0'; sptr++)
        *sptr = tolower( (unsigned char) * sptr);
}

void upper_case(char * sptr)
{
   for (; *sptr != '\0'; sptr++)
        *sptr = toupper( (unsigned char) * sptr);
}

void replace_char(char * sptr, char orig, char repl)
{
    for (; *sptr != '\0'; sptr++)
        if (*sptr == orig) *sptr = repl;
}

void trim(char ** str, char chr)
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

bool is_empty(const char * str)
{
    const char * test = str;
    for (; *test; test++)
        if (!isspace(*test))
            return false;
    return true;
}

bool is_int(const char * str)
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

bool is_float(const char * str)
{
   // Handle negative numbers.
   if (*str == '-')
       ++str;

   size_t dots = 0;

   // Handle empty string or just "-".
   if (!*str)
       return false;

   // Check for non-digit chars in the rest of the string.
   while (*str)
   {
       if (*str == '.')
           ++dots;
       else if (!isdigit(*str))
           return false;

       ++str;
   }

   return dots == 1;
}

bool is_graph(const char * str)
{
    for (; *str; str++)
        if (!isgraph(*str))
            return false;
    return true;
}

void extract_string(char * dest, const char * source, size_t len)
{
    /*
     * This function is used to extra a SiriDB string. These strings start
     * with " or with ' and we should replace double this character in the
     * string with a single one.
     *
     * TODO: This probably can go wrong with unicode.
     */
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
}


