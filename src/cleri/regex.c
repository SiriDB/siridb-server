/*
 * regex.c - cleri regular expression element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/regex.h>
#include <logger/logger.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void cleri_free_regex(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj);

static cleri_node_t *  cleri_parse_regex(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

cleri_object_t * cleri_regex(
        uint32_t gid,
        const char * pattern)
{
    cleri_object_t * cl_object;
    const char * pcre_error_str;
    int pcre_error_offset;

    if (pattern[0] != '^')
    {
        /* this is critical and unexpected, memory is not cleaned */
        printf("critical: regex patterns must start with ^ but got '%s'\n",
                pattern);
        exit(EXIT_FAILURE);
    }

    cl_object = cleri_new_object(
            CLERI_TP_REGEX,
            &cleri_free_regex,
            &cleri_parse_regex);

    cl_object->cl_obj->regex = (cleri_regex_t *) malloc(sizeof(cleri_regex_t));
    cl_object->cl_obj->regex->gid = gid;
    cl_object->cl_obj->regex->regex = pcre_compile(
            pattern,
            0,
            &pcre_error_str,
            &pcre_error_offset,
            NULL);
    if(cl_object->cl_obj->regex->regex == NULL)
    {
        /* this is critical and unexpected, memory is not cleaned */
        printf("critical: could not compile '%s': %s\n",
                pattern,
                pcre_error_str);
        exit(EXIT_FAILURE);
    }
    cl_object->cl_obj->regex->regex_extra =
            pcre_study(cl_object->cl_obj->regex->regex, 0, &pcre_error_str);

    /* pcre_study() returns NULL for both errors and when it can not
     * optimize the regex.  The last argument is how one checks for
     * errors (it is NULL if everything works, and points to an error
     * string otherwise. */
    if(pcre_error_str != NULL)
    {
        printf("critical: could not compile '%s': %s\n",
                pattern,
                pcre_error_str);
        exit(EXIT_FAILURE);
    }
    return cl_object;
}

static void cleri_free_regex(
        cleri_grammar_t * grammar,
        cleri_object_t * cl_obj)
{
    free(cl_obj->cl_obj->regex->regex);
    if (cl_obj->cl_obj->regex->regex_extra != NULL)
        free(cl_obj->cl_obj->regex->regex_extra);
    free(cl_obj->cl_obj->regex);
}

static cleri_node_t *  cleri_parse_regex(
        cleri_parse_result_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    int pcre_exec_ret;
    int sub_str_vec[2];
    const char * str = parent->str + parent->len;
    cleri_node_t * node;

    pcre_exec_ret = pcre_exec(
            cl_obj->cl_obj->regex->regex,
            cl_obj->cl_obj->regex->regex_extra,
            str,
            strlen(str),
            0,                     // start looking at this point
            0,                     // OPTIONS
            sub_str_vec,
            2);                    // length of sub_str_vec
    if (pcre_exec_ret < 0)
    {
        cleri_expecting_update(pr->expecting, cl_obj, str);
        return NULL;
    }
    /* since each regex pattern should start with ^ we now sub_str_vec[0]
     * should be 0. sub_str_vec[1] contains the end position in the sting
     */
    node = cleri_new_node(cl_obj, str, (size_t) sub_str_vec[1]);
    parent->len += node->len;
    cleri_children_add(parent->children, node);
    return node;
}
