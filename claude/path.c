/* path -- see path.h. */
#include <string.h>
#include "path.h"
#include "util.h"

int path_parent(const char *p, char *out, long cap)
{
    long l = (long)strlen(p), k, c = -1, i;
    if (!l || l >= cap)
        return -1;
    if (l > 1 && p[l - 1] == '/' && p[l - 2] != ':')
        l--;                        /* "dir/" is "dir" */
    if (p[l - 1] == ':' || (l == 1 && p[0] == '/'))
        return -1;
    for (i = 0; i < l; i++)
        if (p[i] == ':') {
            c = i;
            break;
        }
    for (k = l - 1; k >= 0 && p[k] != '/'; k--)
        ;
    if (k > c) {
        if (k == 0)
            k = 1;                  /* a host path's "/" */
        memcpy(out, p, (size_t)k);
        out[k] = 0;
    } else if (c >= 0) {
        memcpy(out, p, (size_t)c + 1);
        out[c + 1] = 0;
    } else
        out[0] = 0;
    return 0;
}

static int append(char *cur, const char *seg, long sl, long cap)
{
    long l = (long)strlen(cur);
    int sep = l && cur[l - 1] != ':' && cur[l - 1] != '/';
    if (l + sep + sl + 1 > cap)
        return -1;
    if (sep)
        cur[l++] = '/';
    memcpy(cur + l, seg, (size_t)sl);
    cur[l + sl] = 0;
    return 0;
}

int path_join(const char *base, const char *rel, char *out, long cap)
{
    const char *colon = strchr(rel, ':'), *rest;
    long i, n;
    char tmp[512];
    if (colon) {
        n = (long)(colon - rel) + 1;
        if (n >= cap)
            return -1;
        memcpy(out, rel, (size_t)n);
        out[n] = 0;
        rest = colon + 1;
    } else {
        if ((long)strlen(base) >= cap)
            return -1;
        strcpy(out, base);
        rest = rel;
    }
    n = (long)strlen(rest);
    i = 0;
    while (i < n) {
        long j = i;
        while (j < n && rest[j] != '/')
            j++;
        if ((j == i && j < n) || (j - i == 2 && rest[i] == '.' && rest[i + 1] == '.')) {
            if ((long)strlen(out) >= (long)sizeof(tmp) || path_parent(out, tmp, sizeof(tmp)))
                return -1;
            strcpy(out, tmp);
        } else if (!(j - i == 1 && rest[i] == '.') && j > i) {
            if (append(out, rest + i, j - i, cap))
                return -1;
        }
        i = j + 1;
    }
    return 0;
}

int path_inside(const char *root, const char *p)
{
    long n = (long)strlen(root);
    if (!n || !cl_strnieq(p, root, n))
        return 0;
    if (!p[n])
        return 1;
    if (root[n - 1] == ':' || root[n - 1] == '/')
        return 1;
    return p[n] == '/';
}
