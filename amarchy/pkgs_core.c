/* The Amarchy Packages menu rows (pkgs_core.h). */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pkgs_core.h"

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int cmp_nocase(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) a++, b++;
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

/* Insert one row in name order (the list stays sorted while it is built). */
static void add_row(pk_list *l, const char *name, int nlen, const char *launch, int llen)
{
    char nm[PK_NAME];
    int i;
    if (l->n >= PK_MAX || nlen <= 0 || nlen >= PK_NAME || llen <= 0 || llen >= PK_PATH)
        return;
    memcpy(nm, name, nlen);
    nm[nlen] = 0;
    for (i = l->n; i > 0 && cmp_nocase(l->name[i - 1], nm) > 0; i--) {
        memcpy(l->name[i], l->name[i - 1], PK_NAME);
        memcpy(l->launch[i], l->launch[i - 1], PK_PATH);
    }
    memcpy(l->name[i], nm, nlen + 1);
    memcpy(l->launch[i], launch, llen);
    l->launch[i][llen] = 0;
    l->n++;
}

int pk_parse(const char *buf, long len, pk_list *l)
{
    long p = 0;
    l->n = 0;
    while (p < len) {
        const char *f[8];
        int flen[8], nf = 0;
        long s = p, e = p;
        while (e < len && buf[e] != '\n') e++;
        p = e + 1;
        if (e > s && buf[e - 1] == '\r') e--;
        while (nf < 8) {          /* fields 1..7 plus the start of the rest */
            long q = s;
            while (q < e && buf[q] != '\t') q++;
            f[nf] = buf + s;
            flen[nf] = (int)(q - s);
            nf++;
            if (q >= e) break;
            s = q + 1;
        }
        if (nf < 7 || flen[2] != 1 || f[2][0] != '1')
            continue;
        add_row(l, f[0], flen[0], f[6], flen[6]);
    }
    return l->n;
}

int pk_row_path(const char *launch, char *out, int cap)
{
    int n = (int)(sizeof PK_ROW_PREFIX - 1), m = (int)strlen(launch);
    if (!m || n + m >= cap)
        return 0;
    memcpy(out, PK_ROW_PREFIX, n);
    memcpy(out + n, launch, m + 1);
    return 1;
}

int pk_path_launch(const char *path, char *out, int cap)
{
    size_t p = sizeof PK_ROW_PREFIX - 1;
    if (strncmp(path, PK_ROW_PREFIX, p) || !path[p] || (int)strlen(path + p) >= cap)
        return 0;
    strcpy(out, path + p);
    return 1;
}

/* ---- PG.1: every row ---- */
static void cpyn(char *d, int cap, const char *s, int len)
{
    if (len >= cap)
        len = cap - 1;
    if (len < 0)
        len = 0;
    memcpy(d, s, len);
    d[len] = 0;
}

int pk_name_ok(const char *name)
{
    int i;
    if (!name || !*name)
        return 0;
    for (i = 0; name[i]; i++) {
        char ch = name[i];
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                 ch == '.' || ch == '-' || ch == '+' || ch == '_';
        if (!ok || i >= PK_NAME - 1)
            return 0;
    }
    return 1;
}

static int row_cmp(const void *a, const void *b)
{
    return cmp_nocase(((const pk_row *)a)->name, ((const pk_row *)b)->name);
}

void pk_rows_free(pk_rows *l)
{
    free(l->r);
    l->r = 0;
    l->n = l->over = 0;
}

int pk_parse_all(const char *buf, long len, pk_rows *l)
{
    long p = 0;
    int cap = 0;
    pk_rows_free(l);
    while (p < len) {
        const char *f[9];
        int flen[9], nf = 0;
        long s = p, e = p;
        pk_row *r;
        char nm[PK_NAME + 1];
        while (e < len && buf[e] != '\n') e++;
        p = e + 1;
        if (e > s && buf[e - 1] == '\r') e--;
        while (nf < 9) {
            long q = s;
            while (nf < 8 && q < e && buf[q] != '\t') q++;
            if (nf == 8)
                q = e;               /* the description keeps any further tab */
            f[nf] = buf + s;
            flen[nf] = (int)(q - s);
            nf++;
            if (q >= e) break;
            s = q + 1;
        }
        if (nf < 6 || flen[0] <= 0 || flen[0] > PK_NAME - 1)
            continue;
        cpyn(nm, sizeof nm, f[0], flen[0]);
        if (!pk_name_ok(nm))
            continue;
        if (l->n >= PK_ROWS_MAX) {
            l->over = 1;
            continue;
        }
        if (l->n >= cap) {
            pk_row *nr;
            cap = cap ? cap * 2 : 64;
            nr = (pk_row *)realloc(l->r, (size_t)cap * sizeof *nr);
            if (!nr) {
                pk_rows_free(l);
                return -1;
            }
            l->r = nr;
        }
        r = &l->r[l->n++];
        memset(r, 0, sizeof *r);
        strcpy(r->name, nm);
        cpyn(r->version, PK_VER, f[1], flen[1]);
        r->installed = flen[2] == 1 && f[2][0] == '1';
        r->upgradable = flen[3] == 1 && f[3][0] == '1';
        cpyn(r->section, PK_SEC, f[5], flen[5]);
        if (nf > 6) cpyn(r->launch, PK_PATH, f[6], flen[6]);
        if (nf > 7) cpyn(r->src, PK_SRC, f[7], flen[7]);
        if (nf > 8) cpyn(r->desc, PK_DESC, f[8], flen[8]);
    }
    if (l->n > 1)
        qsort(l->r, (size_t)l->n, sizeof *l->r, row_cmp);
    return l->n;
}

static const char *sec_of(const pk_row *r) { return r->section[0] ? r->section : "other"; }

int pk_sections(const pk_rows *l, char out[][PK_SEC], int max)
{
    int i, j, n = 0;
    for (i = 0; i < l->n; i++) {
        const char *s = sec_of(&l->r[i]);
        int k;
        for (j = 0; j < n && cmp_nocase(out[j], s); j++)
            ;
        if (j < n || n >= max)
            continue;
        for (k = n; k > 0 && cmp_nocase(out[k - 1], s) > 0; k--)
            memcpy(out[k], out[k - 1], PK_SEC);
        strcpy(out[k], s);
        n++;
    }
    return n;
}

int pk_by_section(const pk_rows *l, const char *sec, int *idx, int max)
{
    int i, n = 0;
    for (i = 0; i < l->n && n < max; i++)
        if (!cmp_nocase(sec_of(&l->r[i]), sec))
            idx[n++] = i;
    return n;
}

static int contains_nocase(const char *h, const char *needle)
{
    int nl = (int)strlen(needle), i, j;
    for (i = 0; h[i]; i++) {
        for (j = 0; j < nl && h[i + j] && lower((unsigned char)h[i + j]) == lower((unsigned char)needle[j]); j++)
            ;
        if (j == nl)
            return 1;
    }
    return !nl;
}

int pk_filter(const pk_rows *l, const char *text, int *idx, int max)
{
    int i, n = 0;
    for (i = 0; i < l->n && n < max; i++)
        if (!text || contains_nocase(l->r[i].name, text) || contains_nocase(l->r[i].desc, text))
            idx[n++] = i;
    return n;
}

int pk_select(const pk_rows *l, int upgradable, int *idx, int max)
{
    int i, n = 0;
    for (i = 0; i < l->n && n < max; i++)
        if (upgradable ? l->r[i].upgradable : l->r[i].installed)
            idx[n++] = i;
    return n;
}

void pk_state(const pk_row *r, char *out, int cap)
{
    if (r->installed && r->upgradable)
        snprintf(out, cap, "Upgrade to %s", r->version);
    else
        snprintf(out, cap, "%s", r->installed ? "Installed" : "");
}

void pk_label(const pk_row *r, char *out, int cap)
{
    char st[PK_VER + 16];
    int n;
    pk_state(r, st, sizeof st);
    n = snprintf(out, cap, "[%s] %s  %s  %s", !cmp_nocase(r->src, "uhc") ? "UHC" : "upkg", r->name, r->version, st);
    while (n > 0 && n < cap && out[n - 1] == ' ')
        out[--n] = 0;
}

int pk_actions(const pk_row *r, char out[4])
{
    int n = 0;
    if (!pk_name_ok(r->name))
        return 0;
    if (!r->installed)
        out[n++] = PK_ACT_INSTALL;
    else {
        out[n++] = PK_ACT_REMOVE;
        if (r->launch[0])
            out[n++] = PK_ACT_LAUNCH;
        if (r->upgradable)
            out[n++] = PK_ACT_UPGRADE;
    }
    return n;
}

const char *pk_action_label(int act)
{
    return act == PK_ACT_INSTALL ? "Install" : act == PK_ACT_REMOVE ? "Remove" : act == PK_ACT_LAUNCH ? "Launch" :
           act == PK_ACT_UPGRADE ? "Upgrade" : "";
}

int pk_action_line(const pk_row *r, int act, char *out, int cap)
{
    const char *verb = act == PK_ACT_INSTALL ? "install" : act == PK_ACT_REMOVE ? "remove" :
                       act == PK_ACT_UPGRADE ? "upgrade" : 0;
    int n;
    if (!verb || !pk_name_ok(r->name))
        return 0;
    n = snprintf(out, cap, "upkg %s %s%s", verb,
                 act == PK_ACT_INSTALL && !cmp_nocase(r->src, "uhc") ? "uhc:" : "", r->name);
    return n > 0 && n < cap;
}
