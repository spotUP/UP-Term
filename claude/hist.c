/* hist -- see hist.h. */
#include <stdlib.h>
#include <string.h>
#include "hist.h"
#include "json.h"

void hist_init(cl_hist *h)
{
    memset(h, 0, sizeof(*h));
}

void hist_free(cl_hist *h)
{
    int i;
    for (i = 0; i < h->n; i++) {
        free(h->d[i]);
        free(h->p[i]);
    }
    free(h->d);
    free(h->p);
    memset(h, 0, sizeof(*h));
}

static char *dupz(const char *s)
{
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (d)
        memcpy(d, s, n + 1);
    return d;
}

static void drop(cl_hist *h, int i)
{
    free(h->d[i]);
    free(h->p[i]);
    memmove(h->d + i, h->d + i + 1, sizeof(char *) * (size_t)(h->n - i - 1));
    memmove(h->p + i, h->p + i + 1, sizeof(char *) * (size_t)(h->n - i - 1));
    h->n--;
}

/* takes d and p (malloc'ed); 0, -1 out of memory (both freed) */
static int push(cl_hist *h, char *d, char *p)
{
    if (!d || !p) {
        free(d);
        free(p);
        return -1;
    }
    if (h->n == h->cap) {
        int nc = h->cap ? h->cap * 2 : 64;
        char **nd = (char **)realloc(h->d, sizeof(char *) * (size_t)nc), **np;
        if (!nd) {
            free(d);
            free(p);
            return -1;
        }
        h->d = nd;
        np = (char **)realloc(h->p, sizeof(char *) * (size_t)nc);
        if (!np) {
            free(d);
            free(p);
            return -1;
        }
        h->p = np;
        h->cap = nc;
    }
    h->d[h->n] = d;
    h->p[h->n] = p;
    h->n++;
    while (h->n > HIST_MAX)
        drop(h, 0);
    return 0;
}

int hist_load(cl_hist *h, cl_sys *sys, const char *path)
{
    char *b = 0;
    long n = 0, a = 0, i;
    if (!path || !*path || sys->kind(sys->u, path) != 1 || sys->read(sys->u, path, 4L * 1024 * 1024, &b, &n))
        return h->n;
    for (i = 0; i <= n; i++) {
        jv v, x;
        if (i < n && b[i] != '\n')
            continue;
        if (i > a && json_parse(b + a, i - a, &v) == 0 && json_get(v, "display", &x)) {
            long l;
            char *d = json_strdup(x, &l), *p;
            p = json_get(v, "project", &x) ? json_strdup(x, &l) : dupz("");
            push(h, d, p);
        }
        a = i + 1;
    }
    free(b);
    return h->n;
}

void hist_fill(const cl_hist *h, cl_edit *e, const char *project)
{
    int i, first = h->n, k = 0;
    for (i = h->n - 1; i >= 0 && k < ED_HIST; i--)
        if (!strcmp(h->p[i], project)) {
            first = i;
            k++;
        }
    for (i = first; i < h->n; i++)
        if (!strcmp(h->p[i], project))
            ed_remember(e, h->d[i]);
}

int hist_add(cl_hist *h, cl_sys *sys, const char *path, const char *display, const char *project)
{
    int i;
    jw w;
    if (!*display)
        return 0;
    for (i = h->n - 1; i >= 0; i--)
        if (!strcmp(h->d[i], display) && !strcmp(h->p[i], project))
            drop(h, i);
    if (push(h, dupz(display), dupz(project)))
        return -1;
    if (!path || !*path)
        return 0;
    jw_init(&w);
    for (i = 0; i < h->n; i++) {
        jw_rawz(&w, "{\"display\":");
        jw_strz(&w, h->d[i]);
        jw_rawz(&w, ",\"project\":");
        jw_strz(&w, h->p[i]);
        jw_rawz(&w, "}\n");
    }
    i = w.oom ? -1 : sys->write(sys->u, path, w.p, w.n);
    jw_free(&w);
    return i ? -1 : 0;
}

int hist_find(const cl_hist *h, const char *q, int before)
{
    int i, j;
    if (before > h->n)
        before = h->n;
    for (i = before - 1; i >= 0; i--) {
        if (!strstr(h->d[i], q))
            continue;
        for (j = i + 1; j < h->n; j++)
            if (!strcmp(h->d[j], h->d[i]))
                break;
        if (j == h->n)
            return i;
    }
    return -1;
}
