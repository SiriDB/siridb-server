#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

void add_spaces(char *dest, int num_of_spaces)
{
    size_t len = strlen(dest);
    memset(dest + len, ' ', num_of_spaces );
    dest[len + num_of_spaces] = '\0';
}

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

bool starts_with(const char * str, const char * prefix)
{
    size_t len_str = strlen(str);
    size_t len_prefix = strlen(prefix);
    return len_str < len_prefix ?
            false : strncmp(prefix, str, len_prefix) == 0;
}

bool match(const char * str, const char * prefix)
{
    size_t len_str = strlen(str);
    size_t len_prefix = strlen(prefix);
    size_t min = (len_prefix < len_str) ? len_prefix : len_str;
    return strncmp(prefix, str, min) == 0;
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

int strincmp(const char * stra, const char * strb, size_t len)
{
    const char * a = stra;
    const char * b = strb;
    int ret;
    for (; len--; a++, b++)
        if ((ret = tolower(*a) - tolower(*b)))
            return ret;
    return 0;
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

void extract_string(char ** dest, const char * source, size_t len)
{
    /*
     * This function is used to extra a SiriDB string. These strings start
     * with " or with ' and we should replace double this character in the
     * string with a single one.
     */
    size_t i = 0;

    /* we need at most len - 1 characters since the begin and end will
     * be stripped off, but we do need to set a NULL.
     */
    *dest = (char *) malloc(--len);

    /* take the first character, this is " or ' */
    char chr = *source;

    /* we need to loop till len-2 so take off one more */
    for (--len; i < len; i++)
    {
        source++;
        if (*source == chr)
        {
            /* this is the character, skip one and take the next */
            source++;
            len--;
        }
        (*dest)[i] = *source;
    }

    /* set final 0 */
    (*dest)[i] = 0;
}


