/*
 * version.h - SiriDB version info.
 */
#ifndef SIRI_VERSION_H_
#define SIRI_VERSION_H_

#define SIRIDB_VERSION_MAJOR 2
#define SIRIDB_VERSION_MINOR 0
#define SIRIDB_VERSION_PATCH 44

/*
 * Use SIRIDB_VERSION_PRE_RELEASE for alpha release versions.
 * This should be an empty string when building a final release.
 * Examples: "-alpha-0" "-alpha-1", ""
 * Note that debian alpha packages should use versions like this:
 *   2.0.34-0alpha0
 */
#define SIRIDB_VERSION_PRE_RELEASE ""

#ifndef NDEBUG
#define SIRIDB_VERSION_BUILD_RELEASE "+debug"
#else
#define SIRIDB_VERSION_BUILD_RELEASE ""
#endif

#define SIRIDB_STRINGIFY(num) #num
#define SIRIDB_VERSION_STR(major,minor,patch) \
    SIRIDB_STRINGIFY(major) "." \
    SIRIDB_STRINGIFY(minor) "." \
    SIRIDB_STRINGIFY(patch)

#define SIRIDB_VERSION SIRIDB_VERSION_STR( \
        SIRIDB_VERSION_MAJOR, \
        SIRIDB_VERSION_MINOR, \
        SIRIDB_VERSION_PATCH) \
        SIRIDB_VERSION_PRE_RELEASE \
        SIRIDB_VERSION_BUILD_RELEASE

#define SIRIDB_CONTRIBUTERS \
    "https://github.com/SiriDB/siridb-server/contributors"
#define SIRIDB_HOME_PAGE \
    "https://siridb.net"

int siri_version_cmp(const char * version_a, const char * version_b);

/* SiriDB can only connect with servers having at least this version. */
#define SIRIDB_MINIMAL_VERSION "2.0.0"

#endif  /* SIRI_VERSION_H_ */
