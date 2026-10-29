#include "status_core.h"
#include "../claude/util.h"
#include <string.h>

static const char *const status_key[STATUS_NVALS] = { "hits=", " rows=", " enabled=", " active=", " wins=", " snaps=",
    " bar=", " wbpens=", " wbrefused=", " shots=", " notes=", " ticks=" };
static const char *const bar_key[BARSTATUS_NVALS] = { "bar=", " draws=", " panels=", " launches=", " raises=", " ticks=", " net=" };

long status_line(int bar_status, const long *val, char *out, long cap)
{
    const char *const *key = bar_status ? bar_key : status_key;
    int i, n = bar_status ? BARSTATUS_NVALS : STATUS_NVALS;
    long need = 0;
    char num[12];
    if (cap > 0)
        out[0] = 0;
    for (i = 0; i < n; i++) {
        cl_ltoa(val[i], num);
        need += (long)strlen(key[i]) + (long)strlen(num);
        if (cap > 0) {
            cl_cat(out, key[i], cap);
            cl_cat(out, num, cap);
        }
    }
    return need + 1;
}

static int word(const char *s, const char *w)
{
    while (*w) {
        if ((*s++ | 0x20) != (*w++ | 0x20))
            return 0;
    }
    while (*s == ' ' || *s == '\t')
        s++;
    return *s == 0;
}

int bar_plan(int verb, int held, int open, int *want)
{
    int now = held ? *want : open;
    if (verb == BARV_TOGGLE)
        verb = now ? BARV_OFF : BARV_ON;
    if (verb != BARV_ON && verb != BARV_OFF)
        return BARA_NONE;
    if (held) {
        *want = verb == BARV_ON;
        return BARA_NONE;
    }
    *want = verb == BARV_ON;
    if (verb == BARV_ON)
        return open ? BARA_NONE : BARA_OPEN;
    return open ? BARA_CLOSE : BARA_NONE;
}

int bar_verb(const char *args)
{
    while (*args == ' ' || *args == '\t')
        args++;
    if (word(args, "status"))
        return BARV_STATUS;
    if (word(args, "on"))
        return BARV_ON;
    if (word(args, "off"))
        return BARV_OFF;
    if (word(args, "toggle"))
        return BARV_TOGGLE;
    if (word(args, "text"))
        return BARV_TEXT;
    if (word(args, "pens"))
        return BARV_PENS;
    return BARV_BAD;
}

int tw_theme_changed(const char *have, const char *cur)
{
    return strcmp(have, cur) != 0;
}
