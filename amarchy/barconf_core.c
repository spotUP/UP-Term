/* barconf_core: see barconf_core.h. */
#include <string.h>
#include "barconf_core.h"

static const char *const seg_names[BC_NSEGS] = { "screens", "clock", "cpu", "ram", "network", "agents", "system", "tray" };

void barconf_defaults(barconf *c)
{
    int i;
    memset(c, 0, sizeof *c);
    c->start = 1;
    for (i = 0; i < BC_NSEGS; i++)
        c->seg_on[i] = 1;
}

int barconf_seg(const char *name)
{
    int i;
    for (i = 0; i < BC_NSEGS; i++)
        if (!strcmp(name, seg_names[i]))
            return i;
    return -1;
}

static int blank(char ch) { return ch == ' ' || ch == '\t'; }

/* the next word of *p (cut at blanks) into out; 0 at the end of the row */
static int word(const char **p, const char *end, char *out, int cap)
{
    int n = 0;
    while (*p < end && blank(**p))
        (*p)++;
    if (*p >= end)
        return 0;
    while (*p < end && !blank(**p)) {
        if (n < cap - 1)
            out[n++] = **p;
        (*p)++;
    }
    out[n] = 0;
    return 1;
}

/* the rest of the row, trimmed, into out; 0 when empty or longer than cap-1 */
static int rest(const char *p, const char *end, char *out, int cap)
{
    long n;
    while (p < end && blank(*p))
        p++;
    while (end > p && blank(end[-1]))
        end--;
    n = end - p;
    if (n <= 0 || n >= cap)
        return 0;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    return 1;
}

static int onoff(const char *w)
{
    if (!strcmp(w, "on"))
        return 1;
    if (!strcmp(w, "off"))
        return 0;
    return -1;
}

/* 1 the row was taken (an empty or comment row too), 0 it is bad */
static int row(const char *p, const char *end, barconf *c)
{
    char key[16], w[BC_CMD];
    int g, v, n, s;
    if (!word(&p, end, key, sizeof key) || key[0] == ';' || key[0] == '#')
        return 1;
    if (!strcmp(key, "start")) {
        if (!word(&p, end, w, sizeof w) || (v = onoff(w)) < 0)
            return 0;
        c->start = v;
        return 1;
    }
    if (!strcmp(key, "clock"))
        return rest(p, end, c->clock, BC_FMT);
    if (!strcmp(key, "clockalt"))
        return rest(p, end, c->clockalt, BC_FMT);
    for (g = 0; g < BC_NGROUPS; g++)
        if (!strcmp(key, g == BC_G_LEFT ? "left" : g == BC_G_CENTRE ? "centre" : "right"))
            break;
    if (g < BC_NGROUPS) {
        int tmp[BC_ORDER];
        n = 0;
        while (word(&p, end, w, sizeof w)) {
            s = barconf_seg(w);
            if (s < 0 || n >= BC_ORDER)
                return 0;
            tmp[n++] = s;
        }
        if (!n)
            return 0;
        memcpy(c->order[g], tmp, sizeof(int) * (size_t)n);
        c->norder[g] = n;
        return 1;
    }
    if (!strcmp(key, "segment")) {
        if (!word(&p, end, w, sizeof w) || (s = barconf_seg(w)) < 0)
            return 0;
        if (!word(&p, end, w, sizeof w) || (v = onoff(w)) < 0)
            return 0;
        c->seg_on[s] = v;
        return 1;
    }
    if (!strcmp(key, "run")) {
        bc_run *r;
        if (c->nrun >= BC_RUNS)
            return 0;
        r = &c->run[c->nrun];
        if (!word(&p, end, r->label, BC_LABEL) || !rest(p, end, r->cmd, BC_CMD))
            return 0;
        c->nrun++;
        return 1;
    }
    return 0;
}

int barconf_parse(const char *buf, long len, barconf *c)
{
    const char *p = buf, *stop = buf + len;
    barconf_defaults(c);
    while (p < stop) {
        const char *e = p, *rend;
        while (e < stop && *e != '\n')
            e++;
        rend = e > p && e[-1] == '\r' ? e - 1 : e;
        if (!row(p, rend, c))
            c->bad++;
        p = e + 1;
    }
    return 1;
}
