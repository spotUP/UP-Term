/* sse -- see sse.h. */
#include <stdlib.h>
#include <string.h>
#include "sse.h"
#include "util.h"

void sse_init(sse *s, sse_fn fn, void *u)
{
    memset(s, 0, sizeof(*s));
    s->fn = fn;
    s->u = u;
}

void sse_free(sse *s)
{
    free(s->line);
    free(s->data);
    s->line = s->data = 0;
    s->lcap = s->dcap = 0;
}

static int grow(sse *s, char **p, long *cap, long need)
{
    long c = *cap ? *cap : 256;
    char *q;
    if (need <= *cap)
        return 1;
    while (c < need)
        c *= 2;
    q = (char *)realloc(*p, (size_t)c);
    if (!q) {
        s->oom = 1;
        return 0;
    }
    *p = q;
    *cap = c;
    return 1;
}

static void line_end(sse *s)
{
    const char *v;
    long vl, fl;
    if (!s->ll) {
        /* the blank line: dispatch */
        if (s->has_data && s->fn) {
            s->data[s->dl] = 0;
            s->fn(s->u, s->event[0] ? s->event : "message", s->data, s->dl);
        }
        s->event[0] = 0;
        s->dl = 0;
        s->has_data = 0;
        return;
    }
    if (s->line[0] == ':')
        return;
    for (fl = 0; fl < s->ll && s->line[fl] != ':'; fl++)
        ;
    v = s->line + fl;
    vl = s->ll - fl;
    if (vl) {                       /* skip the colon and one space */
        v++;
        vl--;
        if (vl && *v == ' ') {
            v++;
            vl--;
        }
    }
    if (fl == 5 && !memcmp(s->line, "event", 5)) {
        long n = vl < (long)sizeof(s->event) - 1 ? vl : (long)sizeof(s->event) - 1;
        memcpy(s->event, v, (size_t)n);
        s->event[n] = 0;
    } else if (fl == 4 && !memcmp(s->line, "data", 4)) {
        if (!grow(s, &s->data, &s->dcap, s->dl + vl + 2))
            return;
        if (s->has_data)
            s->data[s->dl++] = '\n';
        memcpy(s->data + s->dl, v, (size_t)vl);
        s->dl += vl;
        s->has_data = 1;
    }
}

void sse_feed(sse *s, const char *p, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        char c = p[i];
        if (c == '\n' && s->cr) {
            s->cr = 0;
            continue;
        }
        s->cr = c == '\r';
        if (c == '\n' || c == '\r') {
            line_end(s);
            s->ll = 0;
            continue;
        }
        if (!grow(s, &s->line, &s->lcap, s->ll + 1))
            continue;
        s->line[s->ll++] = c;
    }
}
