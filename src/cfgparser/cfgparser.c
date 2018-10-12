/*
 * cfgparser.c - Reading (and later writing) to INI style files.
 */
#include <cfgparser/cfgparser.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <xstr/xstr.h>
#include <siri/err.h>

static void cfgparser_free_sections(cfgparser_section_t * root);
static void cfgparser_free_options(cfgparser_option_t * root);
static cfgparser_option_t * cfgparser_new_option(
        cfgparser_section_t * section,
        const char * name,
        cfgparser_tp_t tp,
        cfgparser_via_t * val,
        cfgparser_via_t * def);

#define MAXLINE 255

/*
 * Returns CFGPARSER_SUCCESS if successful or something else in case of an
 * error.
 *
 * Note: In case of a memory allocation error a SIGNAL is raised but
 *       the returned error message might be incorrect. We do however
 *       never return CFGPARSER_SUCCESS when this happens.
 */
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
    {
        return CFGPARSER_ERR_READING_FILE;
    }


    while (fgets(line, MAXLINE, fp) != NULL)
    {
        /* set pointer to line */
        pt = line;

        /* trims all whitespace */
        xstr_trim(&pt, 0);

        if (*pt == '#' || *pt == 0)
        {
            continue;
        }

        if (*pt == '[' && pt[strlen(pt) - 1] == ']')
        {
            xstr_trim(&pt, '[');
            xstr_trim(&pt, ']');
            section = cfgparser_section(cfgparser, pt);
            if (section == NULL)
            {
                /* signal is raised */
                fclose(fp);
                return CFGPARSER_ERR_SESSION_NOT_OPEN;
            }
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
                {
                    found = 1;
                }
                *pt = 0;
                continue;
            }
            if (found)
            {
                break;
            }
        }

        if (!found)
        {
            fclose(fp);
            return CFGPARSER_ERR_MISSING_EQUAL_SIGN;
        }


        if (xstr_is_int(pt))
        {
            option = cfgparser_integer_option(section, name, atoi(pt), 0);
        }
        else if (xstr_is_float(pt))
        {
            sscanf(pt, "%lf", &d);
            option = cfgparser_real_option(section, name, d, 0.0f);
        }
        else
        {
            option = cfgparser_string_option(section, name, pt, "");
        }

        if (option == NULL)
        {
            fclose(fp);
            /*
             * this could also be due to a allocation error, a SIGNAL is set
             * in that case.
             */
            return CFGPARSER_ERR_OPTION_ALREADY_DEFINED;
        }

    }

    fclose(fp);

    return CFGPARSER_SUCCESS;
}

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
cfgparser_t * cfgparser_new(void)
{
    cfgparser_t * cfgparser;

    cfgparser = (cfgparser_t *) malloc(sizeof(cfgparser_t));
    if (cfgparser != NULL)
    {
        cfgparser->sections = NULL;
    }
    return cfgparser;
}

/*
 * Destroy cfgparser. (parsing NULL is not allowed)
 */
void cfgparser_free(cfgparser_t * cfgparser)
{
    cfgparser_free_sections(cfgparser->sections);
    free(cfgparser);
}

/*
 * Returns a section from cfgparser. If the section does not exist, a new
 * section will be created.
 *
 * In case of an error, NULL is returned and a SIGNAL is raised.
 */
cfgparser_section_t * cfgparser_section(
        cfgparser_t * cfgparser,
        const char * name)
{
    cfgparser_section_t * current = cfgparser->sections;

    if (current == NULL)
    {
        cfgparser->sections =
                (cfgparser_section_t *) malloc(sizeof(cfgparser_section_t));
        if (cfgparser->sections == NULL)
        {
            ERR_ALLOC
        }
        else
        {
            cfgparser->sections->options = NULL;
            cfgparser->sections->next = NULL;
            cfgparser->sections->name = strdup(name);
            if (cfgparser->sections->name == NULL)
            {
                ERR_ALLOC
                free(cfgparser->sections);
                cfgparser->sections = NULL;
            }
        }
        return cfgparser->sections;
    }

    while (current->next != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            return current;
        }
        current = current->next;
    }
    current->next =
            (cfgparser_section_t *) malloc(sizeof(cfgparser_section_t));
    if (current->next == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        current->next->options = NULL;
        current->next->next = NULL;
        current->next->name = strdup(name);
        if (current->next->name == NULL)
        {
            ERR_ALLOC
            free(current->next);
            current->next = NULL;
        }
    }
    return current->next;
}

/*
 * Creates and returns a new options. NULL is returned in case the option
 * already existed.
 *
 * In case of an allocation error a SIGNAL is raises and NULL is returned.
 */
cfgparser_option_t * cfgparser_string_option(
        cfgparser_section_t * section,
        const char * name,
        const char * val,
        const char * def)
{
    cfgparser_via_t * val_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));
    cfgparser_via_t * def_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));

    if (val_u == NULL || def_u == NULL)
    {
        ERR_ALLOC
        free(val_u);
        free(def_u);
        return NULL;
    }

    val_u->string = strdup(val);
    def_u->string = strdup(def);

    if (val_u->string == NULL || def_u->string == NULL)
    {
        ERR_ALLOC
        free(val_u->string);
        free(def_u->string);
        free(val_u);
        free(def_u);
        return NULL;
    }

    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_STRING,
            val_u,
            def_u);
}

/*
 * Creates and returns a new options. NULL is returned in case the option
 * already existed.
 *
 * In case of an allocation error a SIGNAL is raises and NULL is returned.
 */
cfgparser_option_t * cfgparser_integer_option(
        cfgparser_section_t * section,
        const char * name,
        int32_t val,
        int32_t def)
{
    cfgparser_via_t * val_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));
    cfgparser_via_t * def_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));
    if (val_u == NULL || def_u == NULL)
    {
        ERR_ALLOC
        free(val_u);
        free(def_u);
        return NULL;
    }
    val_u->integer = val;
    def_u->integer = def;
    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_INTEGER,
            val_u,
            def_u);
}

/*
 * Creates and returns a new options. NULL is returned in case the option
 * already existed.
 *
 * In case of an allocation error a SIGNAL is raises and NULL is returned.
 */
cfgparser_option_t * cfgparser_real_option(
        cfgparser_section_t * section,
        const char * name,
        double val,
        double def)
{
    cfgparser_via_t * val_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));
    cfgparser_via_t * def_u = (cfgparser_via_t *) malloc(sizeof(cfgparser_via_t));
    if (val_u == NULL || def_u == NULL)
    {
        ERR_ALLOC
        free(val_u);
        free(def_u);
        return NULL;
    }
    val_u->real = val;
    def_u->real = def;
    return cfgparser_new_option(
            section,
            name,
            CFGPARSER_TP_REAL,
            val_u,
            def_u);
}

/*
 * Returns an error message for a cfgparser_return_t code.
 */
const char * cfgparser_errmsg(cfgparser_return_t err)
{
    switch (err)
    {
    case CFGPARSER_SUCCESS:
        return "configuration file parsed successfully";
    case CFGPARSER_ERR_READING_FILE:
        return "cannot open file for reading";
    case CFGPARSER_ERR_SESSION_NOT_OPEN:
        return "got a line without a section";
    case CFGPARSER_ERR_MISSING_EQUAL_SIGN:
        return "missing equal sign in at least one line";
    case CFGPARSER_ERR_OPTION_ALREADY_DEFINED:
        return "option defined twice within one section";
    case CFGPARSER_ERR_SECTION_NOT_FOUND:
        return "section not found";
    case CFGPARSER_ERR_OPTION_NOT_FOUND:
        return "option not found";
    }
    return "";
}

/*
 * Finds a section in a cfgparser. When the result is CFGPARSER_SUCCESS,
 * 'section' is set. In case CFGPARSER_ERR_SECTION_NOT_FOUND is returned,
 * 'section' is untouched.
 */
cfgparser_return_t cfgparser_get_section(
        cfgparser_section_t ** section,
        cfgparser_t * cfgparser,
        const char * section_name)
{
    cfgparser_section_t * current = cfgparser->sections;
    while (current != NULL)
    {
        if (strcmp(current->name, section_name) == 0)
        {
            *section = current;
            return CFGPARSER_SUCCESS;
        }
        current = current->next;
    }
    return CFGPARSER_ERR_SECTION_NOT_FOUND;
}

/*
 * Finds a option in a cfgparser. When the result is CFGPARSER_SUCCESS,
 * 'option' is set. In case CFGPARSER_ERR_SECTION_NOT_FOUND or
 * CFGPARSER_ERR_OPTION_NOT_FOUND is returned, 'option' is untouched.
 */
cfgparser_return_t cfgparser_get_option(
        cfgparser_option_t ** option,
        cfgparser_t * cfgparser,
        const char * section_name,
        const char * option_name)
{
    cfgparser_option_t * current;
    cfgparser_section_t * section;
    cfgparser_return_t rc;

    rc = cfgparser_get_section(&section, cfgparser, section_name);
    if (rc != CFGPARSER_SUCCESS)
    {
        return rc;
    }
    current = section->options;
    while (current != NULL)
    {
        if (strcmp(current->name, option_name) == 0)
        {
            *option = current;
            return CFGPARSER_SUCCESS;
        }
        current = current->next;
    }
    return CFGPARSER_ERR_OPTION_NOT_FOUND;
}

/*
 * Creates and returns a new option or NULL in case the option exists.
 * Note that in this case 'val' and 'def' are destroyed.
 *
 * In case an error occurs, NULL is returned too and a SIGNAL is raised.
 */
static cfgparser_option_t * cfgparser_new_option(
        cfgparser_section_t * section,
        const char * name,
        cfgparser_tp_t tp,
        cfgparser_via_t * val,
        cfgparser_via_t * def)
{
    cfgparser_option_t * current = section->options;
    cfgparser_option_t * prev;

    if (current == NULL)
    {
        section->options =
                (cfgparser_option_t *) malloc(sizeof(cfgparser_option_t));
        if (section->options == NULL)
        {
            ERR_ALLOC
        }
        else
        {
            section->options->tp = tp;
            section->options->val = val;
            section->options->def = def;
            section->options->next = NULL;
            section->options->name = strdup(name);
            if (section->options->name == NULL)
            {
                ERR_ALLOC
                free(section->options);
                section->options = NULL;
            }
        }
        return section->options;
    }

    while (current != NULL)
    {
        prev = current;
        if (strcmp(current->name, name) == 0)
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

    prev->next = (cfgparser_option_t *) malloc(sizeof(cfgparser_option_t));
    if (prev->next == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        prev->next->tp = tp;
        prev->next->val = val;
        prev->next->def = def;
        prev->next->next = NULL;
        if ((prev->next->name = strdup(name)) == NULL)
        {
            ERR_ALLOC
            free(prev->next);
            prev->next = NULL;
        }
    }
    return prev->next;
}

/*
 * Destroy cfgparser_section_t. (parsing NULL is allowed)
 */
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

/*
 * Destroy cfgparser_option_t. (parsing NULL is allowed)
 */
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
