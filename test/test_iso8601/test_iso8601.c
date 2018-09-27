#include <time.h>
#include <locale.h>
#include "../test.h"
#include <iso8601/iso8601.h>


int main()
{
    test_start("iso8601");

    /* Update local and timezone */
    (void) setlocale(LC_ALL, "");
    putenv("TZ=:UTC");
    tzset();

    iso8601_tz_t amsterdam = iso8601_tz("Europe/Amsterdam");
    _assert(amsterdam > 0);

    iso8601_tz_t utc = iso8601_tz("UTC");
    _assert(utc > 0);

    /* Some extra time zone test for case sensitivity etc. */
    _assert(iso8601_tz("NAIVE") == 0);
    _assert(iso8601_tz("europe/KIEV") > 0);
    _assert(iso8601_tz("Ams") < 0);

    /* Test parsing a year with time-zone information */
    _assert(iso8601_parse_date("2013", amsterdam) == 1356994800);

    /* Customer offset should be preferred over UTC */
    _assert(iso8601_parse_date("2013+01", utc) == 1356994800);

    /* UTC should be preferred over given time-zone (Amsterdam) */
    _assert(iso8601_parse_date("2013Z", amsterdam) == 1356998400);

    /* Test with minute precision */
    _assert(iso8601_parse_date("2013-02-06T13:01:12", utc) == 1360155672);

    /* Test short written */
    _assert(iso8601_parse_date("2013-2-6 13:1:12", utc) == 1360155672);

    /* Test summer time */
    _assert(iso8601_parse_date("2016-04-21", amsterdam) == 1461189600);

    /* Test error */
    _assert(iso8601_parse_date("2016 04 21", amsterdam) == -1);

    /* Test last day in year */
    _assert(iso8601_parse_date("2013-12-31", utc) == 1388448000);

    return test_end();
}
