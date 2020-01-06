/*
 * base64.c - Base64 encoding/decoding
 */
#include <stdlib.h>


static const int base64__idx[256] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
        7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25,  0, 0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

static const unsigned char base64__table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * base64_decode(const void * data, size_t n, size_t * size)
{
    const unsigned char * p = data;
    int pad = n > 0 && (n % 4 || p[n - 1] == '=');
    size_t i, j, L = ((n + 3) / 4 - pad) * 4;
    char * out = malloc((L / 4 * 3) + (pad
            ? (n > 1 && (n % 4 == 3 || p[n - 2] != '=')) + 2
            : 1));
    if (!out)
        return NULL;

    for (i = 0, j = 0; i < L; i += 4)
    {
        int nn = base64__idx[p[i]] << 18 | \
                base64__idx[p[i + 1]] << 12 | \
                base64__idx[p[i + 2]] << 6 | \
                base64__idx[p[i + 3]];
        out[j++] = nn >> 16;
        out[j++] = nn >> 8 & 0xFF;
        out[j++] = nn & 0xFF;
    }

    if (pad)
    {
        int nn = base64__idx[p[L]] << 18 | base64__idx[p[L + 1]] << 12;
        out[j++] = nn >> 16;

        if (n > L + 2 && p[L + 2] != '=')
        {
            nn |= base64__idx[p[L + 2]] << 6;
            out[j++] = nn >> 8 & 0xFF;
        }
    }

    *size = j;
    out[*size] = '\0';
    return out;
}


char * base64_encode(const void * data, size_t n, size_t * size)
{
    unsigned char * out;
    unsigned char * pos;
    const unsigned char * end, * in;
    size_t olen = 4 * ((n + 2) / 3); /* 3-byte blocks to 4-byte */

    if (olen < n || !(out = malloc(olen + 1)))
        /* integer overflow or allocation error */
        return NULL;

    end = data + n;
    in = data;
    pos = out;

    while (end - in >= 3)
    {
        *pos++ = base64__table[in[0] >> 2];
        *pos++ = base64__table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *pos++ = base64__table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *pos++ = base64__table[in[2] & 0x3f];
        in += 3;
    }

    if (end - in)
    {
        *pos++ = base64__table[in[0] >> 2];
        if (end - in == 1)
        {
            *pos++ = base64__table[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else
        {
            *pos++ = base64__table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64__table[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    *size = pos - out;
    out[*size] = '\0';
    return (char *) out;
}


