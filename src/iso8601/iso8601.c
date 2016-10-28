/*
 * iso8601.c - Library to parse ISO 8601 dates
 *
 * Note: time-zones are found with tzset() in either /usr/lib/zoneinfo/ or
 *       /usr/share/zoneinfo/
 *
 *       (This library is Linux only)
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 21-04-2016
 *
 */
#define _GNU_SOURCE
#include <iso8601/iso8601.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

static const char * tz_common[] = {
        "TZ=:localtime",
        "TZ=:Africa/Abidjan",
        "TZ=:Africa/Accra",
        "TZ=:Africa/Addis_Ababa",
        "TZ=:Africa/Algiers",
        "TZ=:Africa/Asmara",
        "TZ=:Africa/Bamako",
        "TZ=:Africa/Bangui",
        "TZ=:Africa/Banjul",
        "TZ=:Africa/Bissau",
        "TZ=:Africa/Blantyre",
        "TZ=:Africa/Brazzaville",
        "TZ=:Africa/Bujumbura",
        "TZ=:Africa/Cairo",
        "TZ=:Africa/Casablanca",
        "TZ=:Africa/Ceuta",
        "TZ=:Africa/Conakry",
        "TZ=:Africa/Dakar",
        "TZ=:Africa/Dar_es_Salaam",
        "TZ=:Africa/Djibouti",
        "TZ=:Africa/Douala",
        "TZ=:Africa/El_Aaiun",
        "TZ=:Africa/Freetown",
        "TZ=:Africa/Gaborone",
        "TZ=:Africa/Harare",
        "TZ=:Africa/Johannesburg",
        "TZ=:Africa/Juba",
        "TZ=:Africa/Kampala",
        "TZ=:Africa/Khartoum",
        "TZ=:Africa/Kigali",
        "TZ=:Africa/Kinshasa",
        "TZ=:Africa/Lagos",
        "TZ=:Africa/Libreville",
        "TZ=:Africa/Lome",
        "TZ=:Africa/Luanda",
        "TZ=:Africa/Lubumbashi",
        "TZ=:Africa/Lusaka",
        "TZ=:Africa/Malabo",
        "TZ=:Africa/Maputo",
        "TZ=:Africa/Maseru",
        "TZ=:Africa/Mbabane",
        "TZ=:Africa/Mogadishu",
        "TZ=:Africa/Monrovia",
        "TZ=:Africa/Nairobi",
        "TZ=:Africa/Ndjamena",
        "TZ=:Africa/Niamey",
        "TZ=:Africa/Nouakchott",
        "TZ=:Africa/Ouagadougou",
        "TZ=:Africa/Porto-Novo",
        "TZ=:Africa/Sao_Tome",
        "TZ=:Africa/Tripoli",
        "TZ=:Africa/Tunis",
        "TZ=:Africa/Windhoek",
        "TZ=:America/Adak",
        "TZ=:America/Anchorage",
        "TZ=:America/Anguilla",
        "TZ=:America/Antigua",
        "TZ=:America/Araguaina",
        "TZ=:America/Argentina/Buenos_Aires",
        "TZ=:America/Argentina/Catamarca",
        "TZ=:America/Argentina/Cordoba",
        "TZ=:America/Argentina/Jujuy",
        "TZ=:America/Argentina/La_Rioja",
        "TZ=:America/Argentina/Mendoza",
        "TZ=:America/Argentina/Rio_Gallegos",
        "TZ=:America/Argentina/Salta",
        "TZ=:America/Argentina/San_Juan",
        "TZ=:America/Argentina/San_Luis",
        "TZ=:America/Argentina/Tucuman",
        "TZ=:America/Argentina/Ushuaia",
        "TZ=:America/Aruba",
        "TZ=:America/Asuncion",
        "TZ=:America/Atikokan",
        "TZ=:America/Bahia",
        "TZ=:America/Bahia_Banderas",
        "TZ=:America/Barbados",
        "TZ=:America/Belem",
        "TZ=:America/Belize",
        "TZ=:America/Blanc-Sablon",
        "TZ=:America/Boa_Vista",
        "TZ=:America/Bogota",
        "TZ=:America/Boise",
        "TZ=:America/Cambridge_Bay",
        "TZ=:America/Campo_Grande",
        "TZ=:America/Cancun",
        "TZ=:America/Caracas",
        "TZ=:America/Cayenne",
        "TZ=:America/Cayman",
        "TZ=:America/Chicago",
        "TZ=:America/Chihuahua",
        "TZ=:America/Costa_Rica",
        "TZ=:America/Creston",
        "TZ=:America/Cuiaba",
        "TZ=:America/Curacao",
        "TZ=:America/Danmarkshavn",
        "TZ=:America/Dawson",
        "TZ=:America/Dawson_Creek",
        "TZ=:America/Denver",
        "TZ=:America/Detroit",
        "TZ=:America/Dominica",
        "TZ=:America/Edmonton",
        "TZ=:America/Eirunepe",
        "TZ=:America/El_Salvador",
        "TZ=:America/Fort_Nelson",
        "TZ=:America/Fortaleza",
        "TZ=:America/Glace_Bay",
        "TZ=:America/Godthab",
        "TZ=:America/Goose_Bay",
        "TZ=:America/Grand_Turk",
        "TZ=:America/Grenada",
        "TZ=:America/Guadeloupe",
        "TZ=:America/Guatemala",
        "TZ=:America/Guayaquil",
        "TZ=:America/Guyana",
        "TZ=:America/Halifax",
        "TZ=:America/Havana",
        "TZ=:America/Hermosillo",
        "TZ=:America/Indiana/Indianapolis",
        "TZ=:America/Indiana/Knox",
        "TZ=:America/Indiana/Marengo",
        "TZ=:America/Indiana/Petersburg",
        "TZ=:America/Indiana/Tell_City",
        "TZ=:America/Indiana/Vevay",
        "TZ=:America/Indiana/Vincennes",
        "TZ=:America/Indiana/Winamac",
        "TZ=:America/Inuvik",
        "TZ=:America/Iqaluit",
        "TZ=:America/Jamaica",
        "TZ=:America/Juneau",
        "TZ=:America/Kentucky/Louisville",
        "TZ=:America/Kentucky/Monticello",
        "TZ=:America/Kralendijk",
        "TZ=:America/La_Paz",
        "TZ=:America/Lima",
        "TZ=:America/Los_Angeles",
        "TZ=:America/Lower_Princes",
        "TZ=:America/Maceio",
        "TZ=:America/Managua",
        "TZ=:America/Manaus",
        "TZ=:America/Marigot",
        "TZ=:America/Martinique",
        "TZ=:America/Matamoros",
        "TZ=:America/Mazatlan",
        "TZ=:America/Menominee",
        "TZ=:America/Merida",
        "TZ=:America/Metlakatla",
        "TZ=:America/Mexico_City",
        "TZ=:America/Miquelon",
        "TZ=:America/Moncton",
        "TZ=:America/Monterrey",
        "TZ=:America/Montevideo",
        "TZ=:America/Montserrat",
        "TZ=:America/Nassau",
        "TZ=:America/New_York",
        "TZ=:America/Nipigon",
        "TZ=:America/Nome",
        "TZ=:America/Noronha",
        "TZ=:America/North_Dakota/Beulah",
        "TZ=:America/North_Dakota/Center",
        "TZ=:America/North_Dakota/New_Salem",
        "TZ=:America/Ojinaga",
        "TZ=:America/Panama",
        "TZ=:America/Pangnirtung",
        "TZ=:America/Paramaribo",
        "TZ=:America/Phoenix",
        "TZ=:America/Port-au-Prince",
        "TZ=:America/Port_of_Spain",
        "TZ=:America/Porto_Velho",
        "TZ=:America/Puerto_Rico",
        "TZ=:America/Rainy_River",
        "TZ=:America/Rankin_Inlet",
        "TZ=:America/Recife",
        "TZ=:America/Regina",
        "TZ=:America/Resolute",
        "TZ=:America/Rio_Branco",
        "TZ=:America/Santa_Isabel",
        "TZ=:America/Santarem",
        "TZ=:America/Santiago",
        "TZ=:America/Santo_Domingo",
        "TZ=:America/Sao_Paulo",
        "TZ=:America/Scoresbysund",
        "TZ=:America/Sitka",
        "TZ=:America/St_Barthelemy",
        "TZ=:America/St_Johns",
        "TZ=:America/St_Kitts",
        "TZ=:America/St_Lucia",
        "TZ=:America/St_Thomas",
        "TZ=:America/St_Vincent",
        "TZ=:America/Swift_Current",
        "TZ=:America/Tegucigalpa",
        "TZ=:America/Thule",
        "TZ=:America/Thunder_Bay",
        "TZ=:America/Tijuana",
        "TZ=:America/Toronto",
        "TZ=:America/Tortola",
        "TZ=:America/Vancouver",
        "TZ=:America/Whitehorse",
        "TZ=:America/Winnipeg",
        "TZ=:America/Yakutat",
        "TZ=:America/Yellowknife",
        "TZ=:Antarctica/Casey",
        "TZ=:Antarctica/Davis",
        "TZ=:Antarctica/DumontDUrville",
        "TZ=:Antarctica/Macquarie",
        "TZ=:Antarctica/Mawson",
        "TZ=:Antarctica/McMurdo",
        "TZ=:Antarctica/Palmer",
        "TZ=:Antarctica/Rothera",
        "TZ=:Antarctica/Syowa",
        "TZ=:Antarctica/Troll",
        "TZ=:Antarctica/Vostok",
        "TZ=:Arctic/Longyearbyen",
        "TZ=:Asia/Aden",
        "TZ=:Asia/Almaty",
        "TZ=:Asia/Amman",
        "TZ=:Asia/Anadyr",
        "TZ=:Asia/Aqtau",
        "TZ=:Asia/Aqtobe",
        "TZ=:Asia/Ashgabat",
        "TZ=:Asia/Baghdad",
        "TZ=:Asia/Bahrain",
        "TZ=:Asia/Baku",
        "TZ=:Asia/Bangkok",
        "TZ=:Asia/Beirut",
        "TZ=:Asia/Bishkek",
        "TZ=:Asia/Brunei",
        "TZ=:Asia/Chita",
        "TZ=:Asia/Choibalsan",
        "TZ=:Asia/Colombo",
        "TZ=:Asia/Damascus",
        "TZ=:Asia/Dhaka",
        "TZ=:Asia/Dili",
        "TZ=:Asia/Dubai",
        "TZ=:Asia/Dushanbe",
        "TZ=:Asia/Gaza",
        "TZ=:Asia/Hebron",
        "TZ=:Asia/Ho_Chi_Minh",
        "TZ=:Asia/Hong_Kong",
        "TZ=:Asia/Hovd",
        "TZ=:Asia/Irkutsk",
        "TZ=:Asia/Jakarta",
        "TZ=:Asia/Jayapura",
        "TZ=:Asia/Jerusalem",
        "TZ=:Asia/Kabul",
        "TZ=:Asia/Kamchatka",
        "TZ=:Asia/Karachi",
        "TZ=:Asia/Kathmandu",
        "TZ=:Asia/Khandyga",
        "TZ=:Asia/Kolkata",
        "TZ=:Asia/Krasnoyarsk",
        "TZ=:Asia/Kuala_Lumpur",
        "TZ=:Asia/Kuching",
        "TZ=:Asia/Kuwait",
        "TZ=:Asia/Macau",
        "TZ=:Asia/Magadan",
        "TZ=:Asia/Makassar",
        "TZ=:Asia/Manila",
        "TZ=:Asia/Muscat",
        "TZ=:Asia/Nicosia",
        "TZ=:Asia/Novokuznetsk",
        "TZ=:Asia/Novosibirsk",
        "TZ=:Asia/Omsk",
        "TZ=:Asia/Oral",
        "TZ=:Asia/Phnom_Penh",
        "TZ=:Asia/Pontianak",
        "TZ=:Asia/Pyongyang",
        "TZ=:Asia/Qatar",
        "TZ=:Asia/Qyzylorda",
        "TZ=:Asia/Rangoon",
        "TZ=:Asia/Riyadh",
        "TZ=:Asia/Sakhalin",
        "TZ=:Asia/Samarkand",
        "TZ=:Asia/Seoul",
        "TZ=:Asia/Shanghai",
        "TZ=:Asia/Singapore",
        "TZ=:Asia/Srednekolymsk",
        "TZ=:Asia/Taipei",
        "TZ=:Asia/Tashkent",
        "TZ=:Asia/Tbilisi",
        "TZ=:Asia/Tehran",
        "TZ=:Asia/Thimphu",
        "TZ=:Asia/Tokyo",
        "TZ=:Asia/Ulaanbaatar",
        "TZ=:Asia/Urumqi",
        "TZ=:Asia/Ust-Nera",
        "TZ=:Asia/Vientiane",
        "TZ=:Asia/Vladivostok",
        "TZ=:Asia/Yakutsk",
        "TZ=:Asia/Yekaterinburg",
        "TZ=:Asia/Yerevan",
        "TZ=:Atlantic/Azores",
        "TZ=:Atlantic/Bermuda",
        "TZ=:Atlantic/Canary",
        "TZ=:Atlantic/Cape_Verde",
        "TZ=:Atlantic/Faroe",
        "TZ=:Atlantic/Madeira",
        "TZ=:Atlantic/Reykjavik",
        "TZ=:Atlantic/South_Georgia",
        "TZ=:Atlantic/St_Helena",
        "TZ=:Atlantic/Stanley",
        "TZ=:Australia/Adelaide",
        "TZ=:Australia/Brisbane",
        "TZ=:Australia/Broken_Hill",
        "TZ=:Australia/Currie",
        "TZ=:Australia/Darwin",
        "TZ=:Australia/Eucla",
        "TZ=:Australia/Hobart",
        "TZ=:Australia/Lindeman",
        "TZ=:Australia/Lord_Howe",
        "TZ=:Australia/Melbourne",
        "TZ=:Australia/Perth",
        "TZ=:Australia/Sydney",
        "TZ=:Canada/Atlantic",
        "TZ=:Canada/Central",
        "TZ=:Canada/Eastern",
        "TZ=:Canada/Mountain",
        "TZ=:Canada/Newfoundland",
        "TZ=:Canada/Pacific",
        "TZ=:Europe/Amsterdam",
        "TZ=:Europe/Andorra",
        "TZ=:Europe/Athens",
        "TZ=:Europe/Belgrade",
        "TZ=:Europe/Berlin",
        "TZ=:Europe/Bratislava",
        "TZ=:Europe/Brussels",
        "TZ=:Europe/Bucharest",
        "TZ=:Europe/Budapest",
        "TZ=:Europe/Busingen",
        "TZ=:Europe/Chisinau",
        "TZ=:Europe/Copenhagen",
        "TZ=:Europe/Dublin",
        "TZ=:Europe/Gibraltar",
        "TZ=:Europe/Guernsey",
        "TZ=:Europe/Helsinki",
        "TZ=:Europe/Isle_of_Man",
        "TZ=:Europe/Istanbul",
        "TZ=:Europe/Jersey",
        "TZ=:Europe/Kaliningrad",
        "TZ=:Europe/Kiev",
        "TZ=:Europe/Lisbon",
        "TZ=:Europe/Ljubljana",
        "TZ=:Europe/London",
        "TZ=:Europe/Luxembourg",
        "TZ=:Europe/Madrid",
        "TZ=:Europe/Malta",
        "TZ=:Europe/Mariehamn",
        "TZ=:Europe/Minsk",
        "TZ=:Europe/Monaco",
        "TZ=:Europe/Moscow",
        "TZ=:Europe/Oslo",
        "TZ=:Europe/Paris",
        "TZ=:Europe/Podgorica",
        "TZ=:Europe/Prague",
        "TZ=:Europe/Riga",
        "TZ=:Europe/Rome",
        "TZ=:Europe/Samara",
        "TZ=:Europe/San_Marino",
        "TZ=:Europe/Sarajevo",
        "TZ=:Europe/Simferopol",
        "TZ=:Europe/Skopje",
        "TZ=:Europe/Sofia",
        "TZ=:Europe/Stockholm",
        "TZ=:Europe/Tallinn",
        "TZ=:Europe/Tirane",
        "TZ=:Europe/Uzhgorod",
        "TZ=:Europe/Vaduz",
        "TZ=:Europe/Vatican",
        "TZ=:Europe/Vienna",
        "TZ=:Europe/Vilnius",
        "TZ=:Europe/Volgograd",
        "TZ=:Europe/Warsaw",
        "TZ=:Europe/Zagreb",
        "TZ=:Europe/Zaporozhye",
        "TZ=:Europe/Zurich",
        "TZ=:GMT",
        "TZ=:Indian/Antananarivo",
        "TZ=:Indian/Chagos",
        "TZ=:Indian/Christmas",
        "TZ=:Indian/Cocos",
        "TZ=:Indian/Comoro",
        "TZ=:Indian/Kerguelen",
        "TZ=:Indian/Mahe",
        "TZ=:Indian/Maldives",
        "TZ=:Indian/Mauritius",
        "TZ=:Indian/Mayotte",
        "TZ=:Indian/Reunion",
        "TZ=:Pacific/Apia",
        "TZ=:Pacific/Auckland",
        "TZ=:Pacific/Bougainville",
        "TZ=:Pacific/Chatham",
        "TZ=:Pacific/Chuuk",
        "TZ=:Pacific/Easter",
        "TZ=:Pacific/Efate",
        "TZ=:Pacific/Enderbury",
        "TZ=:Pacific/Fakaofo",
        "TZ=:Pacific/Fiji",
        "TZ=:Pacific/Funafuti",
        "TZ=:Pacific/Galapagos",
        "TZ=:Pacific/Gambier",
        "TZ=:Pacific/Guadalcanal",
        "TZ=:Pacific/Guam",
        "TZ=:Pacific/Honolulu",
        "TZ=:Pacific/Johnston",
        "TZ=:Pacific/Kiritimati",
        "TZ=:Pacific/Kosrae",
        "TZ=:Pacific/Kwajalein",
        "TZ=:Pacific/Majuro",
        "TZ=:Pacific/Marquesas",
        "TZ=:Pacific/Midway",
        "TZ=:Pacific/Nauru",
        "TZ=:Pacific/Niue",
        "TZ=:Pacific/Norfolk",
        "TZ=:Pacific/Noumea",
        "TZ=:Pacific/Pago_Pago",
        "TZ=:Pacific/Palau",
        "TZ=:Pacific/Pitcairn",
        "TZ=:Pacific/Pohnpei",
        "TZ=:Pacific/Port_Moresby",
        "TZ=:Pacific/Rarotonga",
        "TZ=:Pacific/Saipan",
        "TZ=:Pacific/Tahiti",
        "TZ=:Pacific/Tarawa",
        "TZ=:Pacific/Tongatapu",
        "TZ=:Pacific/Wake",
        "TZ=:Pacific/Wallis",
        "TZ=:US/Alaska",
        "TZ=:US/Arizona",
        "TZ=:US/Central",
        "TZ=:US/Eastern",
        "TZ=:US/Hawaii",
        "TZ=:US/Mountain",
        "TZ=:US/Pacific",
        "TZ=:UTC"
};

#define TZ_LEN 433
#define TZ_UTC 432

/* Use this offset because names start with TZ=: */
#define TZ_NAME_OFFSET 4

#define TZ_DIGITS \
    for (len = 0; *pt && isdigit(*pt); pt++, len++);

#define TZ_RESULT(tz_fmt, check_min) \
    if (*pt == 0)                                               \
        return get_ts(str, tz_fmt, tz, 0);                      \
                                                                \
    if (*pt == 'Z' && *(pt + 1) == 0)                           \
        return get_ts(str, tz_fmt "Z", TZ_UTC, 0);              \
                                                                \
    if (*pt == '+' || (check_min && *pt == '-' && (sign = 1)))  \
    {                                                           \
        offset = 0;                                             \
                                                                \
        pt++;                                                   \
        if (!isdigit(*pt))                                      \
            return -1;                                          \
        offset += (*pt - '0') * 36000;                          \
                                                                \
        pt++;                                                   \
        if (!isdigit(*pt))                                      \
            return -1;                                          \
        offset += (*pt - '0') * 3600;                           \
                                                                \
        pt++;                                                   \
        if (*pt == ':')                                         \
            pt++;                                               \
                                                                \
        if (*pt == 0)                                           \
            return get_ts(str, tz_fmt, TZ_UTC, offset * sign);  \
                                                                \
        if (!isdigit(*pt))                                      \
            return -1;                                          \
        offset += (*pt - '0') * 600;                            \
                                                                \
        pt++;                                                   \
        if (!isdigit(*pt))                                      \
            return -1;                                          \
        offset += (*pt - '0') * 60;                             \
                                                                \
        pt++;                                                   \
        if (*pt != 0)                                           \
            return -1;                                          \
                                                                \
        return get_ts(str, tz_fmt, TZ_UTC, offset * sign);      \
    }


static int64_t get_ts(
        const char * str,
        const char * fmt,
        iso8601_tz_t tz,
        int64_t offset);

iso8601_tz_t iso8601_tz(const char * tzname)
{
    size_t i, j;
    size_t len = strlen(tzname);
    char buf[len];

    for (i = 0; i < len; i++)
    {
        buf[i] = tolower(tzname[i]);
    }

    if (strncmp(buf, "naive", len) == 0)
    {
        /* TZ=:localtime */
        return 0;
    }

    for (i = 0; i < TZ_LEN; i++)
    {
        if (strlen(tz_common[i]) != len + TZ_NAME_OFFSET)
        {
            continue;
        }

        for (j = 0; j < len; j++)
        {
            if (buf[j] != tolower(tz_common[i][j + TZ_NAME_OFFSET]))
            {
                break;
            }
        }

        if (j != len)
        {
            continue;
        }

        return i;
    }

    return -1;
}

const char * iso8601_tzname(iso8601_tz_t tz)
{
    return (tz) ? tz_common[tz] + TZ_NAME_OFFSET : "NAIVE";
}

int64_t iso8601_parse_date(const char * str, iso8601_tz_t tz)
{
    size_t len;
    const char * pt = str;
    int64_t offset;
    int sign = -1;
    int use_t = 0;

    /* read year */
    TZ_DIGITS
    if (len != 4)
    {
        return -1;
    }

    TZ_RESULT("%Y", 0)
    if (*pt != '-')
    {
        return -1;
    }
    pt++;

    /* read month */
    TZ_DIGITS
    if (!len || len > 2)
    {
        return -1;
    }

    TZ_RESULT("%Y-%m", 0)
    if (*pt != '-')
    {
        return -1;
    }
    pt++;

    /* read day */
    TZ_DIGITS
    if (!len || len > 2)
    {
        return -1;
    }

    TZ_RESULT("%Y-%m-%d", 1)
    if (*pt != ' ' && (use_t = 1) && *pt != 'T')
    {
        return -1;
    }
    pt++;

    /* read hours */
    for (len = 0; *pt && isdigit(*pt); pt++, len++);

    if (!len || len > 2)
    {
        return -1;
    }

    TZ_RESULT((use_t) ? "%Y-%m-%dT%H" : "%Y-%m-%d %H", 1)
    if (*pt != ':')
    {
        return -1;
    }
    pt++;

    /* read minutes */
    TZ_DIGITS
    if (!len || len > 2)
    {
        return -1;
    }

    TZ_RESULT((use_t) ? "%Y-%m-%dT%H:%M" : "%Y-%m-%d %H:%M", 1)
    if (*pt != ':')
    {
        return -1;
    }
    pt++;

    /* read seconds */
    TZ_DIGITS
    if (!len || len > 2)
    {
        return -1;
    }

    TZ_RESULT((use_t) ? "%Y-%m-%dT%H:%M:%S" : "%Y-%m-%d %H:%M:%S", 1)

    return -1;
}

static int64_t get_ts(
        const char * str,
        const char * fmt,
        iso8601_tz_t tz,
        int64_t offset)
{
    struct tm tm;
    time_t ts;

#ifdef DEBUG
    /* must be a valid timezone */
    assert (tz >= 0 && tz < TZ_LEN);

    /* timezone must be 0 (UTC) */
    assert (timezone == 0);
#endif

    memset(&tm, 0, sizeof(struct tm));

    /* month day should default to 1, not 0 */
    if (!tm.tm_mday)
    {
        tm.tm_mday = 1;
    }

    if (strptime(str, fmt, &tm) == NULL)
    {
        /* parsing date string failed */
        return -1;
    }

    ts = mktime(&tm);

    /* in case of UTO or a custom offset we can simple return */
    if (tz == TZ_UTC || offset)
    {
        return ts + offset;
    }

    /* set custom timezone */
    putenv((char *) tz_common[tz]);
    tzset();

    /* localize the timestamp so we get the correct offset including
     * daylight saving time.
     */
    memset(&tm, 0, sizeof(struct tm));
    localtime_r(&ts, &tm);

    /* set environment back to UTC */
    putenv("TZ=:UTC");
    tzset();

    ts -= tm.tm_gmtoff;

    return ts;
}
