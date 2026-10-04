/* util -- see util.h. */
#include <string.h>
#include "util.h"

int cl_ltoa(long v, char *out)
{
    char t[12];
    int n = 0, i = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (v < 0)
        out[i++] = '-';
    while (n)
        out[i++] = t[--n];
    out[i] = 0;
    return i;
}

char *cl_copy(char *dst, const char *src, long cap)
{
    long n = (long)strlen(src);
    if (cap <= 0)
        return dst;
    if (n > cap - 1)
        n = cap - 1;
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
    return dst;
}

char *cl_cat(char *dst, const char *src, long cap)
{
    long l = (long)strlen(dst);
    if (l < cap)
        cl_copy(dst + l, src, cap - l);
    return dst;
}

static int low(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int cl_strnieq(const char *a, const char *b, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        if (low((unsigned char)a[i]) != low((unsigned char)b[i]))
            return 0;
        if (!a[i])
            return 1;
    }
    return 1;
}

int cl_strieq(const char *a, const char *b)
{
    while (*a && low((unsigned char)*a) == low((unsigned char)*b)) {
        a++;
        b++;
    }
    return low((unsigned char)*a) == low((unsigned char)*b);
}
