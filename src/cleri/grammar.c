/*
 * grammar.c - this should contain the 'start' or your grammar.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/grammar.h>
#include <logger/logger.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcre.h>

cleri_grammar_t * cleri_grammar(
        cleri_object_t * start,
        const char * re_keywords)
{
    cleri_grammar_t * grammar;
    grammar = (cleri_grammar_t *) malloc(sizeof(cleri_grammar_t));
    grammar->start = start;

    const char * pcre_error_str;
    int pcre_error_offset;
    grammar->re_keywords = pcre_compile(
            re_keywords,
            0,
            &pcre_error_str,
            &pcre_error_offset,
            NULL);
    if(grammar->re_keywords == NULL)
    {
        /* this is critical and unexpected, memory is not cleaned */
        printf("critical: could not compile '%s': %s\n",
                re_keywords,
                pcre_error_str);
        exit(EXIT_FAILURE);
    }
    grammar->re_kw_extra =
            pcre_study(grammar->re_keywords, 0, &pcre_error_str);

    /* pcre_study() returns NULL for both errors and when it can not
     * optimize the regex.  The last argument is how one checks for
     * errors (it is NULL if everything works, and points to an error
     * string otherwise. */
    if(pcre_error_str != NULL)
    {
        printf("critical: could not compile '%s': %s\n",
                re_keywords,
                pcre_error_str);
        exit(EXIT_FAILURE);
    }
    grammar->olist = NULL;
    return grammar;
}

void cleri_free_grammar(cleri_grammar_t * grammar)
{
    grammar->olist = cleri_new_olist();
    free(grammar->re_keywords);
    if (grammar->re_kw_extra != NULL)
        free(grammar->re_kw_extra);
    cleri_free_object(grammar, grammar->start);
    cleri_free_olist(NULL, grammar->olist);
    free(grammar);
}
