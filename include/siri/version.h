/*
 * version.h - contains SiriDB version info.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#pragma once

#define SIRIDB_VERSION_MAJOR 2
#define SIRIDB_VERSION_MINOR 0
#define SIRIDB_VERSION_PATCH 0

#define SIRIDB_VERSION "2.0.0"
#define SIRIDB_MAINTAINER "Jeroen van der Heijden <jeroen@transceptor.technology>"
#define SIRIDB_HOME_PAGE "http://siridb.net"

int siri_version_cmp(const char * version_a, const char * version_b);

/* SiriDB can only connect with servers having at least this version. */
#define SIRIDB_MINIMAL_VERSION "2.0.0"
