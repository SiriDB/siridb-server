/*
 * cfgparser.c - module for reading (and later writing) to INI style files.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cfgparser/cfgparser.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strextra/strextra.h>

static void cfgparser_free_sections(cfgparser_section_t * root);
static void cfgparser_free_options(cfgparser_option_t * root);
static cfgparser_option_t * cfgparser_new_option(
        cfgparser_section_t * section,
        const char * name,
        cfgparser_tp_t tp,
        cfgparser_u * val,
        cfgparser_u * def);

#define MAXLINE 255

cfgparser_return_t cfgparser_read(cfgparser_t * cfgparser, const char * fn)
{
    FILE * fp;
    char line[MAXLINE];
    char * pt;
    const char * name;
    cfgparser_section_t * section = NULL;
    cfgparser_option_t * option = NULL;
    int found = 0;
    double d;

    fp = fopen(fn, "r");
    if (fp == NULL)
        return CFGPARSER_ERR_READING_FILE;


    while (fgets(line, MAXLINE, fp) != NULL)
    {
        /* set pointer to line */
        pt = line;

        /* trims all whitespace */
        trim(&pt, 0);

        if (*pt == '#' || *pt == 0)
            continue;

        if (*pt == '[' && pt[strlen(pt) - 1] == ']')
        {
            trim(&pt, '[');
            trim(&pt, ']');
            section = cfgparser_section(cfgparser, pt);
            continue;
        }

        if (section == NULL)
        {
            fclose(fp);
            return CFGPARSER_ERR_SESSION_NOT_OPEN;
        }

        for (found = 0, name = pt; *pt; pt++)
        {
            if (isspace(*pt) || *pt == '=')
            {
                if (*pt == '=')
                    found = 1;
                *pt = 0;
                continue;
            }
            if (found)
                break;
        }

        if (!found)
        {
            fclose(fp);
            return CFGPARSER_ERR_MISSING_EQUAL_SIGN;
        }


        if (is_int(pt))
            option = cfgparser_integer_option(section, name, atoi(pt), 0);
        else if (is_float(pt))
        {
            sscanf(pt, "%lf", &d);
            option = cfgparser_real_option(section, name, d, 0.0f);
        }
        else
            option = cfgparser_string_option(section, name, pt, "");

        if (option == NULL)
        {
            fclose(fp);
            return CFGPARSER_ERR_OPTION_ALREADY_DEFINED;
        }

    }

    fclose(fp);

    return CFGPARSER_SUCCESS;
}

cfgparser_t * cfgparser_new(void)
{
    cfgparser_t * cfgparser;

    cfgparser = (cfgparser_t *) malloc(sizeof(cfgparser_t));
    cfgparser->sections = NULL;

    return cfgparser;
}

void cfgparser_free(cfgparser_t * cfgparser)
{
    cfgparser_free_sections(cfgparser->sections);
    free(cfgparser);
}

cfgparser_section_t * cfgparser_section(
        cfgparser_t * cfgparser,
        const char * name)
{
    cfgparser_section_t * current = cfgparser->sections;
    size_t len = strlen(name);

    if (current == NULL)
    {
        cfgparser->sections =
                (cfgparser_section_t *) malloc(sizeof(cfgparser_section_t));
        cfgparser->sections->name =
                (char *) malloc(sizeof(char) * (len + 1));
        memcpy(cfgparser->sections->name, name, len);
        cfgparser->sections->name[len] = 0;
        cfgparser->sections->options = NULL;
        cfgparser->sections->next = NULL;
        return cfgparser->sections;
    }

    while (current->next != NULL)
    {
        if (strlen(current->name) == len &&
                strcmp(current->name, name) == 0)
            return current;
        current = current->next;
    }
    current->next =
            (cfgparser_section_t *) malloc(sizeof(cfgparser_section_t));
    current->next->name = (char *) malloc(sizeof(char) * (len + 1));
    memcpy(current->next->name, name, len);
    current->next->name[len] = 0;
    current->next->options = NULL;
    current->next->next = NULL;

    return current->next;
}

cfgparser_option_t * cfgparser_string_option(
        cfgparser_section_t * section,
        const char * name,
        const char * val,
        const char * def)
{
    size_t len_val = strlen(val);
    size_t len_def = strlen(def);

    cfgparser_u * val_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));
    cfgparser_u * def_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));

    val_u->string = (char *) malloc(sizeof(char) * (len_val + 1));
    def_u->string = (char *) malloc(sizeof(char) * (len_def + 1));

    memcpy(val_u->string, val, len_val);
    val_u->string[len_val] = 0;
    memcpy(def_u->string, def, len_def);
    def_u->string[len_def] = 0;

    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_STRING,
            val_u,
            def_u);
}

cfgparser_option_t * cfgparser_integer_option(
        cfgparser_section_t * section,
        const char * name,
        int32_t val,
        int32_t def)
{
    cfgparser_u * val_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));
    cfgparser_u * def_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));
    val_u->integer = val;
    def_u->integer = def;
    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_INTEGER,
            val_u,
            def_u);
}

cfgparser_option_t * cfgparser_real_option(
        cfgparser_section_t * section,
        const char * name,
        double val,
        double def)
{
    cfgparser_u * val_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));
    cfgparser_u * def_u = (cfgparser_u *) malloc(sizeof(cfgparser_u));
    val_u->real = val;
    def_u->real = def;
    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_REAL,
            val_u,
            def_u);
}

const char * cfgparser_errmsg(cfgparser_return_t err)
{
    switch (err)
    {
    case CFGPARSER_SUCCESS:
        return "success: configuration file parsed successfully";
    case CFGPARSER_ERR_READING_FILE:
        return "error: cannot open file for reading";
    case CFGPARSER_ERR_SESSION_NOT_OPEN:
        return "error: got a line without a section";
    case CFGPARSER_ERR_MISSING_EQUAL_SIGN:
        return "error: missing equal sign in at least one line";
    case CFGPARSER_ERR_OPTION_ALREADY_DEFINED:
        return "error: option defined twice within one section";
    case CFGPARSER_ERR_SECTION_NOT_FOUND:
        return "error: section not found";
    case CFGPARSER_ERR_OPTION_NOT_FOUND:
        return "error: option not found";
    }
    return "";
}

cfgparser_return_t cfgparser_get_section(
        cfgparser_section_t ** section,
        cfgparser_t * cfgparser,
        const char * section_name)
{
    cfgparser_section_t * current = cfgparser->sections;
    size_t len = strlen(section_name);
    while (current != NULL)
    {
        if (strlen(current->name) == len
                && strcmp(current->name, section_name) == 0)
        {
            *section = current;
            return CFGPARSER_SUCCESS;
        }
        current = current->next;
    }
    return CFGPARSER_ERR_SECTION_NOT_FOUND;
}

cfgparser_return_t cfgparser_get_option(
        cfgparser_option_t ** option,
        cfgparser_t * cfgparser,
        const char * section_name,
        const char * option_name)
{
    cfgparser_option_t * current;
    cfgparser_section_t * section;
    cfgparser_return_t rc;
    size_t len = strlen(option_name);

    rc = cfgparser_get_section(&section, cfgparser, section_name);
    if (rc != CFGPARSER_SUCCESS)
        return rc;
    current = section->options;
    while (current != NULL)
    {
        if (strlen(current->name) == len
                && strcmp(current->name, option_name) == 0)
        {
            *option = current;
            return CFGPARSER_SUCCESS;
        }
        current = current->next;
    }

    return CFGPARSER_ERR_OPTION_NOT_FOUND;
}

static cfgparser_option_t * cfgparser_new_option(
        cfgparser_section_t * section,
        const char * name,
        cfgparser_tp_t tp,
        cfgparser_u * val,
        cfgparser_u * def)
{
    cfgparser_option_t * current = section->options;
    cfgparser_option_t * prev;
    size_t len = strlen(name);

    if (current == NULL)
    {
        section->options =
                (cfgparser_option_t *) malloc(sizeof(cfgparser_option_t));
        section->options->name =
                (char *) malloc(len + 1);
        memcpy(section->options->name, name, len);
        section->options->name[len] = 0;
        section->options->tp = tp;
        section->options->val = val;
        section->options->def = def;
        section->options->next = NULL;

        return section->options;
    }

    while (current != NULL)
    {
        prev = current;
        if (strlen(current->name) == len &&
                strcmp(current->name, name) == 0)
        {
            if (tp == CFGPARSER_TP_STRING)
            {
                free(val->string);
                free(def->string);
            }
            free(val);
            free(def);
            return NULL;
        }
        current = current->next;
    }

    prev->next =
            (cfgparser_option_t *) malloc(sizeof(cfgparser_option_t));
    prev->next->name = (char *) malloc(len + 1);
    memcpy(prev->next->name, name, len);
    prev->next->name[len] = 0;
    prev->next->tp = tp;
    prev->next->val = val;
    prev->next->def = def;
    prev->next->next = NULL;

    return prev->next;
}

static void cfgparser_free_sections(cfgparser_section_t * root)
{
    cfgparser_section_t * next;

    while (root != NULL)
    {
        next = root->next;
        free(root->name);
        cfgparser_free_options(root->options);
        free(root);
        root = next;
    }
}

static void cfgparser_free_options(cfgparser_option_t * root)
{
    cfgparser_option_t * next;

    while (root != NULL)
    {
        next = root->next;
        if (root->tp == CFGPARSER_TP_STRING)
        {
            free(root->val->string);
            free(root->def->string);
        }
        free(root->val);
        free(root->def);
        free(root->name);
        free(root);
        root = next;
    }
}
