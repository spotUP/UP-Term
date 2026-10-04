/* termurl -- see termurl.h. */
#include "termurl.h"
#include <string.h>

static int unreserved(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '.' || c == '_' || c == '~' || c == '/';
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* no character a quoted AmigaDOS argument cannot hold as it is */
static int quotable(const char *s)
{
    for (; *s; s++)
        if (*s == '"' || *s == '*' || (unsigned char)*s < 0x20 || *s == 0x7F)
            return 0;
    return 1;
}

long termurl_osc7(const char *dir, const char *host, char *out, long max)
{
    static const char hex[] = "0123456789ABCDEF";
    long k = 0;
    const char *p;
    int vol = 1;
#define PUT(c) do { if (k + 1 >= max) return 0; out[k++] = (char)(c); } while (0)
    PUT(0x1B);
    PUT(']');
    PUT('7');
    PUT(';');
    for (p = "file://"; *p; p++)
        PUT(*p);
    for (p = host ? host : ""; *p; p++)
        PUT(*p);
    PUT('/');
    for (p = dir; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (vol && c == ':') { /* Vol: -> /Vol/ */
            vol = 0;
            if (p[1])
                PUT('/');
            continue;
        }
        if (unreserved(c)) {
            PUT(c);
        } else {
            PUT('%');
            PUT(hex[c >> 4]);
            PUT(hex[c & 15]);
        }
    }
    PUT(0x1B);
    PUT('\\');
#undef PUT
    out[k] = 0;
    return k;
}

int termurl_cwd_dir(const char *uri, const char *myhost, char *out, int cap)
{
    const char *h, *p;
    int k = 0, vol = 1, n;
    if (strncmp(uri, "file://", 7))
        return 0;
    h = uri + 7;
    for (p = h; *p && *p != '/'; p++)
        ;
    n = (int)(p - h);
    if (n) { /* a host: this one? */
        int i, mine = myhost && (int)strlen(myhost) == n;
        for (i = 0; mine && i < n; i++)
            mine = lower(h[i]) == lower(myhost[i]);
        if (!mine && !(n == 9 && !strncmp(h, "localhost", 9)))
            return 0;
    }
    if (*p != '/' || !p[1])
        return 0; /* "/" alone is no AmigaDOS directory */
    for (p++; *p; p++) {
        int c = (unsigned char)*p;
        if (c == '%') {
            int a = hexval(p[1]), b = a < 0 ? -1 : hexval(p[2]);
            if (b < 0)
                return 0;
            c = a * 16 + b;
            p += 2;
        } else if (vol && c == '/') {
            c = ':'; /* /Vol/path -> Vol:path */
            vol = 0;
        }
        if (k + 2 >= cap)
            return 0;
        out[k++] = (char)c;
    }
    if (vol) { /* only the volume: Vol: */
        if (k + 2 >= cap)
            return 0;
        out[k++] = ':';
    } else if (k > 1 && out[k - 1] == '/') {
        k--; /* a trailing / is no name of its own */
    }
    out[k] = 0;
    return quotable(out);
}

int termurl_link_command(const char *tmpl, const char *uri, char *out, int cap)
{
    const char *s = strstr(tmpl, "%s");
    int pre = s ? (int)(s - tmpl) : (int)strlen(tmpl);
    int ul = (int)strlen(uri), post = s ? (int)strlen(s + 2) : 0;
    if (!*uri || !quotable(uri) || pre + 1 + 2 + ul + post + 1 > cap)
        return 0;
    memcpy(out, tmpl, pre);
    if (!s)
        out[pre++] = ' ';
    out[pre++] = '"';
    memcpy(out + pre, uri, ul);
    pre += ul;
    out[pre++] = '"';
    if (s)
        memcpy(out + pre, s + 2, post);
    out[pre + post] = 0;
    return 1;
}
