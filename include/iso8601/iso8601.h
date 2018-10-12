/*
 * iso8601.h - Library to parse ISO 8601 dates. Time-zones are found with
 *             tzset() in either /usr/lib/zoneinfo/ or /usr/share/zoneinfo/.
 */
#ifndef ISO8601_H_
#define ISO8601_H_

#include <inttypes.h>

typedef int16_t iso8601_tz_t;

/* Returns a time-zone identifier from a given name or a negative value
 * in case of an error or when the time-zone is not found.
 * Examples of tzname are: Europe/Amsterdam, UTC etc.
 *
 * (this function is not case-sensitive)
 */
iso8601_tz_t iso8601_tz(const char * tzname);

/* Returns the name for a given timezone */
const char * iso8601_tzname(iso8601_tz_t tz);

/* Returns a time-stamp in seconds for a given date or a negative value in
 * case or an error. Time-zone information can be parsed but is also allowed
 * in the string.
 */
int64_t iso8601_parse_date(const char * str, iso8601_tz_t tz);

#endif  /* ISO8601_H_ */
