/*
 * siri/evars.c
 */

#include <siri/evars.h>

static void evars__u8(const char * evar, uint8_t * u8)
{
    char * u8str = getenv(evar);
    if (!u8str)
        return;

    *u8 = (uint8_t) strtoul(u8str, NULL, 10);
}

static void evars__u16(const char * evar, uint16_t * u16)
{
    char * u16str = getenv(evar);
    if (!u16str)
        return;

    *u16 = (uint16_t) strtoul(u16str, NULL, 10);
}

static void evars__sizet(const char * evar, size_t * sz)
{
    char * sizetstr = getenv(evar);
    if (!sizetstr)
        return;

    *sz = (size_t) strtoull(sizetstr, NULL, 10);
}

static void evars__double(const char * evar, double * d)
{
    char * doublestr = getenv(evar);
    if (!doublestr)
        return;

    *d = strtod(doublestr, NULL);
}

static void evars__str(const char * evar, char ** straddr)
{
    char * str = getenv(evar);
    if (!str || !(str = strdup(str)))
        return;

    free(*straddr);
    *straddr = str;
}


void siri_evars_parse(siri_t * siri)
{

}
