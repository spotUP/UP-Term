/* index_core.c -- see index_core.h. */
#include <string.h>
#include "index_core.h"
#include "menu_core.h"

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

static int cmp_ci(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
        a++, b++;
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int copy_field(char *d, int dn, const char *s, int len)
{
    if (len <= 0 || len >= dn)
        return 0;
    memcpy(d, s, len);
    d[len] = 0;
    return 1;
}

int ix_parse_row(const char *line, ix_row *out)
{
    const char *t1, *t2, *end;
    ix_row r;
    memset(&r, 0, sizeof r);
    t1 = strchr(line, '\t');
    if (!t1)
        return 0;
    t2 = strchr(t1 + 1, '\t');
    if (!t2)
        return 0;
    end = t2 + 1 + strcspn(t2 + 1, "\r\n");
    if (memchr(t2 + 1, '\t', (size_t)(end - t2 - 1)))
        return 0;
    if (t1 - line == 3 && !strncmp(line, "app", 3))
        r.kind = IX_KIND_APP;
    else if (t1 - line == 4 && !strncmp(line, "game", 4))
        r.kind = IX_KIND_GAME;
    else
        return 0;
    if (!copy_field(r.label, IX_LABEL, t1 + 1, (int)(t2 - t1 - 1)))
        return 0;
    if (!copy_field(r.path, IX_PATH, t2 + 1, (int)(end - t2 - 1)))
        return 0;
    *out = r;
    return 1;
}

int ix_format_row(const ix_row *r, char *out, int n)
{
    const char *k = r->kind == IX_KIND_GAME ? "game" : "app";
    int need = (int)(strlen(k) + 1 + strlen(r->label) + 1 + strlen(r->path) + 1);
    if (need + 1 > n)
        return -1;
    strcpy(out, k);
    strcat(out, "\t");
    strcat(out, r->label);
    strcat(out, "\t");
    strcat(out, r->path);
    strcat(out, "\n");
    return need;
}

void ix_label_from_name(const char *name, char *out, int n)
{
    int len = (int)strlen(name);
    if (len > 5 && !cmp_ci(name + len - 5, ".info"))
        len -= 5;
    if (len >= n)
        len = n - 1;
    memcpy(out, name, len);
    out[len] = 0;
}

int ix_skip_match(const char *skip, const char *path)
{
    const char *p = skip;
    while (*p) {
        const char *e = strchr(p, ',');
        const char *s = p, *f;
        int len, i;
        if (!e)
            e = p + strlen(p);
        f = e;
        while (s < f && (*s == ' ' || *s == '\t'))
            s++;
        while (f > s && (f[-1] == ' ' || f[-1] == '\t'))
            f--;
        len = (int)(f - s);
        if (len > 0) {
            for (i = 0; i < len && path[i] && lower((unsigned char)s[i]) == lower((unsigned char)path[i]); i++)
                ;
            /* whole elements: the prefix ends at the path's end or at a slash, or itself ends in : or / */
            if (i == len && (!path[len] || path[len] == '/' || s[len - 1] == ':' || s[len - 1] == '/'))
                return 1;
        }
        p = *e ? e + 1 : e;
    }
    return 0;
}

void ix_volume_of(const char *path, char *out, int n)
{
    const char *c = strchr(path, ':');
    int len = c ? (int)(c - path) : 0;
    if (len >= n)
        len = n - 1;
    memcpy(out, path, len);
    out[len] = 0;
}

static int row_less(const ix_row *a, const ix_row *b)
{
    int c = cmp_ci(a->label, b->label);
    return c ? c < 0 : cmp_ci(a->path, b->path) < 0;
}

void ix_sort_dedupe(ix_row *rows, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        ix_row t = rows[i];
        for (j = i; j > 0 && row_less(&t, &rows[j - 1]); j--)
            rows[j] = rows[j - 1];
        rows[j] = t;
    }
    for (i = 0; i < n; i++) {
        int shared = (i > 0 && !cmp_ci(rows[i].label, rows[i - 1].label)) ||
                     (i + 1 < n && !cmp_ci(rows[i].label, rows[i + 1].label));
        rows[i].suffix[0] = 0;
        if (shared)
            ix_volume_of(rows[i].path, rows[i].suffix, IX_VOL);
    }
}

int ix_drop_shadowed(const keymap *k, const char *node, ix_row *rows, int n)
{
    mn_item it[MN_MAX_ITEMS];
    int have = mn_children(k, node, it, MN_MAX_ITEMS), i, j, out = 0;
    for (i = 0; i < n; i++) {
        int hit = 0;
        for (j = 0; j < have && !hit; j++)
            hit = !cmp_ci(it[j].label, rows[i].label);
        if (!hit)
            rows[out++] = rows[i];
    }
    return out;
}
