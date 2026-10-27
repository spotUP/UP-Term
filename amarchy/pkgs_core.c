/* The Amarchy Packages menu rows (pkgs_core.h). */
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
