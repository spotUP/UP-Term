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

int mn_filter(const mn_item *items, int n, const char *filter, int *idx, int max)
{
    int pass, i, c = 0;
    for (pass = 2; pass >= 1; pass--)
        for (i = 0; i < n && c < max; i++)
            if (mn_match(items[i].label, filter) == pass)
                idx[c++] = i;
    return c;
}

int mn_pick(const mn_item *items, const int *idx, int c, const char *label)
{
    int i;
    for (i = 0; label && i < c; i++)
        if (!strcmp(items[idx[i]].label, label))
            return i;
    return -1;
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
