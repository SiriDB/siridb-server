/*
 * content.h - Log data (string content)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2017, Transceptor Technology
 *
 * changes
 *  - initial version, 11-12-2017
 *
 */
#pragma once


typedef struct siridb_content_s
{
    size_t size;
    char * data;
} siridb_content_t;

