#include <string.h>
#include "menu_core.h"

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

int mn_match(const char *label, const char *filter)
{
    int i, j, any = 0;
    if (!filter || !*filter)
        return 2;
    for (i = 0; label[i]; i++) {
        for (j = 0; filter[j] && label[i + j] && lower(label[i + j]) == lower(filter[j]); j++)
            ;
        if (!filter[j]) {
            if (i == 0)
                return 2;
            any = 1;
        }
    }
    return any;
}

static void cpy(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = s[i];
    d[i] = 0;
}

int mn_children(const keymap *k, const char *node, mn_item *out, int max)
{
    int i, j, n = 0, nl = (int)strlen(node);
    for (i = 0; i < k->nmenu && n < max; i++) {
        const char *id = k->menu[i].id, *rest, *dot;
        int seglen, dup = 0;
        if (nl) {
            if (strncmp(id, node, nl) || id[nl] != '.')
                continue;
            rest = id + nl + 1;
        } else
            rest = id;
        if (!*rest)
            continue;
        dot = strchr(rest, '.');
        seglen = dot ? (int)(dot - rest) : (int)strlen(rest);
        if (seglen >= KM_ID)
            continue;
        if (dot) {
            for (j = 0; j < n; j++)
                if (out[j].row < 0 && !strncmp(out[j].seg, rest, seglen) && !out[j].seg[seglen])
                    dup = 1;
            if (dup)
                continue;
            out[n].row = -1;
            memcpy(out[n].seg, rest, seglen);
            out[n].seg[seglen] = 0;
            out[n].label[0] = (char)((rest[0] >= 'a' && rest[0] <= 'z') ? rest[0] - 32 : rest[0]);
            cpy(out[n].label + 1, rest + 1, seglen < KM_LABEL ? seglen : KM_LABEL);
        } else {
            out[n].row = i;
            cpy(out[n].seg, rest, KM_ID);
            cpy(out[n].label, k->menu[i].label, KM_LABEL);
        }
        n++;
    }
    return n;
}

/* the most entries and the widest text of `node` and below; the node path is bounded by KM_ID * 2 */
static void osd_walk(const keymap *k, const char *node, int *maxn, int *maxw)
{
    mn_item it[MN_MAX_ITEMS];
    char child[KM_ID * 2];
    int i, n = mn_children(k, node, it, MN_MAX_ITEMS);
    if (n > *maxn)
        *maxn = n;
    for (i = 0; i < n; i++) {
        int w = (int)strlen(it[i].label) + 2 + (it[i].row < 0 ? 1 : 0);
        if (w > *maxw)
            *maxw = w;
        if (it[i].row < 0) {
            mn_child_node(node, &it[i], child, sizeof child);
            osd_walk(k, child, maxn, maxw);
        }
    }
}

void mn_osd_size(const keymap *k, const char *node, int *cols, int *rows)
{
    int maxn = 0, maxw = 0, c;
    osd_walk(k, node, &maxn, &maxw);
    c = maxw + 2 * MN_OSD_PAD;
    *cols = c < MN_OSD_MIN_COLS ? MN_OSD_MIN_COLS : c;
    *rows = MN_OSD_HEAD_LINES + maxn;
}

int mn_filter(const mn_item *items, int n, const char *filter, int *idx, int max)
{
    int pass, i, c = 0;
    for (pass = 2; pass >= 1; pass--)
        for (i = 0; i < n && c < max; i++)
            if (mn_match(items[i].label, filter) == pass)
                idx[c++] = i;
    return c;
}

int mn_parent(const char *node, char *out, int n)
{
    const char *dot = strrchr(node, '.');
    int len;
    if (!*node)
        return 0;
    len = dot ? (int)(dot - node) : 0;
    if (len > n - 1)
        len = n - 1;
    memcpy(out, node, len);
    out[len] = 0;
    return 1;
}

void mn_child_node(const char *node, const mn_item *it, char *out, int n)
{
    if (*node) {
        cpy(out, node, n);
        strncat(out, ".", n - strlen(out) - 1);
        strncat(out, it->seg, n - strlen(out) - 1);
    } else
        cpy(out, it->seg, n);
}

static void app(char *out, int n, const char *s)
{
    int l = (int)strlen(out);
    while (*s && l < n - 1)
        out[l++] = *s++;
    out[l] = 0;
}

int mn_spec(const km_bind *b, char *out, int n)
{
    const char *key = keymap_key_name(b->code);
    char cap[2];
    out[0] = 0;
    app(out, n, "Super");
    if (b->qual & KM_Q_SHIFT)
        app(out, n, "+Shift");
    if (b->qual & KM_Q_ALT)
        app(out, n, "+Alt");
    if (b->qual & KM_Q_RSHIFT)
        app(out, n, "+RShift");
    app(out, n, "+");
    if (!key)
        key = "?";
    cap[0] = (key[0] >= 'a' && key[0] <= 'z') ? (char)(key[0] - 32) : key[0];
    cap[1] = 0;
    app(out, n, cap);
    app(out, n, key + 1);
    return (int)strlen(out) < n - 1;
}

int mn_viewer_line(const km_bind *b, char *out, int n)
{
    char spec[48];
    int l, ok = mn_spec(b, spec, sizeof spec);
    out[0] = 0;
    app(out, n, spec);
    for (l = (int)strlen(out); l < MN_SPEC_COL && l < n - 1; l++)
        out[l] = ' ';
    out[l] = 0;
    app(out, n, "  ");
    app(out, n, b->label[0] ? b->label : b->arg);
    if (b->cat[0]) {
        app(out, n, "  (");
        app(out, n, b->cat);
        app(out, n, ")");
    }
    return ok && (int)strlen(out) < n - 1;
}

int mn_viewer_text(const keymap *k, char *buf, long cap)
{
    int i, lines = 0;
    long len = 0;
    char line[160];
    buf[0] = 0;
    for (i = 0; i < k->nbind; i++) {
        long l;
        mn_viewer_line(&k->bind[i], line, sizeof line);
        l = (long)strlen(line);
        if (len + l + 2 > cap)
            break;
        memcpy(buf + len, line, l);
        buf[len + l] = '\n';
        len += l + 1;
        buf[len] = 0;
        lines++;
    }
    return lines;
}

const char *mn_term_default(const char *term)
{
    return term && *term ? 0 : "vtcon";
}
