/* index_core.c -- see index_core.h. */
#include <stdlib.h>
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

void ix_free(ix_index *x)
{
    free(x->rows);
    x->rows = 0;
    x->n = x->napps = 0;
}

int ix_level_cap(const ix_index *x)
{
    return MN_MAX_ITEMS + (x ? x->n : 0) + 2;
}

void ix_load_text(const keymap *k, ix_index *x, const char *text)
{
    ix_row *all = 0;
    const char *t;
    int cap = 1, n = 0, i, na = 0, ng;
    ix_free(x);
    for (t = text; t && *t; t++)                /* a row per line at most */
        cap += *t == '\n';
    if (!text || !*text || !(all = malloc(cap * sizeof *all)) || !(x->rows = malloc(cap * sizeof *all))) {
        free(all);
        return;
    }
    while (*text && n < cap) {
        const char *nl = strchr(text, '\n');
        char line[IX_LABEL + IX_PATH + 16];
        int len = nl ? (int)(nl - text) : (int)strlen(text);
        if (len < (int)sizeof line) {
            memcpy(line, text, len);
            line[len] = 0;
            if (ix_parse_row(line, &all[n]))
                n++;
        }
        text += len + (nl ? 1 : 0);
    }
    ix_sort_dedupe(all, n);
    for (i = 0; i < n; i++)
        if (all[i].kind == IX_KIND_APP)
            x->rows[na++] = all[i];
    x->napps = ix_drop_shadowed(k, "apps", x->rows, na);
    for (ng = 0, i = 0; i < n; i++)
        if (all[i].kind != IX_KIND_APP)
            x->rows[x->napps + ng++] = all[i];
    ng = ix_drop_shadowed(k, "games", x->rows + x->napps, ng);
    x->n = x->napps + ng;
    free(all);
}

static int add_index(const ix_index *x, int from, int to, mn_item *out, int n, int max)
{
    int i;
    for (i = from; i < to && n < max; i++) {
        const ix_row *r = &x->rows[i];
        char lab[IX_LABEL + IX_VOL + 4];
        strcpy(lab, r->label);
        if (r->suffix[0]) {
            strcat(lab, " (");
            strcat(lab, r->suffix);
            strcat(lab, ")");
        }
        out[n].row = MN_ROW_INDEX;
        out[n].ix = i;
        out[n].seg[0] = 0;
        strncpy(out[n].label, lab, KM_LABEL - 1);
        out[n].label[KM_LABEL - 1] = 0;
        n++;
    }
    return n;
}

static int has_group(const mn_item *it, int n, const char *seg)
{
    int i;
    for (i = 0; i < n; i++)
        if (MN_IS_GROUP(&it[i]) && !strcmp(it[i].seg, seg))
            return 1;
    return 0;
}

static int add_group(mn_item *out, int n, int max, const char *seg, const char *label)
{
    if (n < max) {
        out[n].row = MN_ROW_GROUP;
        out[n].ix = -1;
        strcpy(out[n].seg, seg);
        strcpy(out[n].label, label);
        n++;
    }
    return n;
}

int ix_level(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
             int max)
{
    int n = mn_children(k, node, out, max);
    if (!x)
        return n;
    if (!strcmp(node, "apps")) {
        n = add_index(x, 0, x->napps, out, n, max);
    } else if (!strcmp(node, "games")) {
        n = add_index(x, x->napps, x->n, out, n, max);
    } else if (!*node) {
        if (x->napps && !has_group(out, n, "apps"))
            n = add_group(out, n, max, "apps", "Apps");
        if (x->n > x->napps && !has_group(out, n, "games"))
            n = add_group(out, n, max, "games", "Games");
        if (filter && *filter)
            n = add_index(x, 0, x->n, out, n, max);
    }
    return n;
}

int ix_launch_path(const ix_index *x, const mn_item *it, char *out, int n)
{
    if (n > 0)
        out[0] = 0;
    if (!MN_IS_INDEX(it) || it->ix < 0 || it->ix >= x->n || (int)strlen(x->rows[it->ix].path) >= n)
        return 0;
    strcpy(out, x->rows[it->ix].path);
    return 1;
}
