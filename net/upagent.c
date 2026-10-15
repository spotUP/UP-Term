/* See upagent.h. Plain C, no Amiga includes. */
#include <string.h>
#include "tn.h"
#include "upagent.h"

static int put(char *out, int n, int at, const char *s)
{
    int l = (int)strlen(s);
    if (at < 0 || at + l >= n)
        return -1;
    memcpy(out + at, s, (size_t)l);
    out[at + l] = 0;
    return at + l;
}

static void decimal(long v, char *buf)
{
    char tmp[12];
    int i = 0, j = 0;
    if (v < 0)
        v = 0;
    do {
        tmp[i++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i < 11);
    while (i)
        buf[j++] = tmp[--i];
    buf[j] = 0;
}

const char *upagent_pick(int (*exists)(const char *path))
{
    if (exists(UPAGENT_DEFAULT))
        return UPAGENT_DEFAULT;
    return exists(UPAGENT_FALLBACK) ? UPAGENT_FALLBACK : 0;
}

int upagent_script(char *out, int n, const char *agent, long port, const char *keyhex)
{
    char num[12];
    int at = 0;
    decimal(port, num);
    at = put(out, n, at, "Set wasabi.key ");
    at = put(out, n, at, keyhex);
    at = put(out, n, at, "\n\"");
    at = put(out, n, at, agent);
    at = put(out, n, at, "\" ");
    at = put(out, n, at, num);
    at = put(out, n, at, " name session plain\n");
    return at < 0 ? 0 : at;
}
