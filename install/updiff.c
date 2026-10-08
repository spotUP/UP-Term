/* updiff -- the installed kit's manifest against the new kit's (updiff.h). */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "updiff.h"

typedef struct {
    char *part, *path;
    unsigned long size, crc;
} ud_line;

typedef struct {
    ud_line *l;
    int n;
} ud_list;

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* split text into lines and fields in place; -1 for a bad line or one out of order */
static int parse(char *text, ud_list *out)
{
    char *p;
    int n = 0, i = 0;
    for (p = text; *p; p++)
        if (*p == '\n') n++;
    out->l = (ud_line *)malloc(sizeof(ud_line) * (n + 1));
    out->n = 0;
    if (!out->l) return -1;
    p = text;
    while (*p) {
        char *e = strchr(p, '\n'), *f;
        ud_line *l = &out->l[i];
        int d;
        if (e) *e = 0;
        if (*p) {
            l->part = p;
            if (!(f = strchr(p, ' '))) return -1;
            *f++ = 0;
            l->size = 0;
            if (*f < '0' || *f > '9') return -1;
            while (*f >= '0' && *f <= '9') l->size = l->size * 10 + (unsigned long)(*f++ - '0');
            if (*f++ != ' ') return -1;
            l->crc = 0;
            for (d = 0; d < 8; d++) {
                int h = hexval((unsigned char)*f++);
                if (h < 0) return -1;
                l->crc = (l->crc << 4) | (unsigned long)h;
            }
            if (*f++ != ' ' || !*f) return -1;
            l->path = f;
            if (i > 0 && strcmp(out->l[i - 1].path, l->path) >= 0) return -1;
            i++;
        }
        if (!e) break;
        p = e + 1;
    }
    out->n = i;
    return 0;
}

static ud_part *part_of(ud_result *r, const char *name)
{
    int i;
    for (i = 0; i < r->nparts; i++)
        if (!strcmp(r->part[i].name, name)) return &r->part[i];
    if (r->nparts >= UD_MAXPARTS || strlen(name) >= UD_NAME) return 0;
    memset(&r->part[r->nparts], 0, sizeof(ud_part));
    strcpy(r->part[r->nparts].name, name);
    return &r->part[r->nparts++];
}

/* "copy#<from>#<drawer>" into the part; 0 when it is not such a line */
static int copy_line(const char *path, char *from, char *drawer)
{
    const char *h;
    if (strncmp(path, "copy#", 5)) return 0;
    h = strchr(path + 5, '#');
    if (!h || h - (path + 5) >= UD_DRAWER || strlen(h + 1) >= UD_DRAWER) return -1;
    memcpy(from, path + 5, h - (path + 5));
    from[h - (path + 5)] = 0;
    strcpy(drawer, h + 1);
    return 1;
}

/* the path below a copy part's kit drawer, 0 when it is not below it */
static const char *below(const char *path, const char *from)
{
    size_t n = strlen(from);
    return (!strncmp(path, from, n) && path[n] == '/') ? path + n + 1 : 0;
}

typedef struct {
    ud_emit emit;
    void *ctx;
    char madedir[UD_MAXPARTS][256];
} ud_out;

/* the strings up to a 0 joined into buf; 0 when they do not fit */
static int join(char *buf, size_t n, ...)
{
    va_list ap;
    const char *s;
    size_t k = 0;
    va_start(ap, n);
    while ((s = va_arg(ap, const char *)) != 0) {
        size_t m = strlen(s);
        if (k + m >= n) {
            va_end(ap);
            return 0;
        }
        memcpy(buf + k, s, m);
        k += m;
    }
    va_end(ap);
    buf[k] = 0;
    return 1;
}

static void say(ud_out *o, const ud_part *p, const char *line)
{
    if (o->emit) o->emit(o->ctx, p->name, line);
}

/* the drawers a file needs (each once in a row), then the copy */
static void emit_copy(ud_out *o, ud_result *r, ud_part *p, const char *kit, const char *path, const char *rest)
{
    char line[600], dir[256];
    const char *s;
    size_t k = strlen(kit);
    int pi = (int)(p - r->part);
    for (s = strchr(rest, '/'); s; s = strchr(s + 1, '/')) {
        size_t n = (size_t)(s - rest);
        if (n >= sizeof(dir)) return;
        memcpy(dir, rest, n);
        dir[n] = 0;
        if (!strncmp(o->madedir[pi], dir, n) && (o->madedir[pi][n] == 0 || o->madedir[pi][n] == '/'))
            continue;   /* made for an earlier file of this drawer */
        if (!join(line, sizeof(line), "If NOT EXISTS \"UP-Term:", p->drawer, "/", dir, "\"", (char *)0)) return;
        say(o, p, line);
        join(line, sizeof(line), "  MakeDir \"UP-Term:", p->drawer, "/", dir, "\"", (char *)0);
        say(o, p, line);
        say(o, p, "EndIf");
    }
    s = strrchr(rest, '/');
    if (s && (size_t)(s - rest) < sizeof(o->madedir[pi])) {
        memcpy(o->madedir[pi], rest, (size_t)(s - rest));
        o->madedir[pi][s - rest] = 0;
    }
    if (join(line, sizeof(line), "Copy \"", kit, (k && kit[k - 1] != ':' && kit[k - 1] != '/') ? "/" : "",
             path, "\" \"UP-Term:", p->drawer, "/", rest, "\" CLONE QUIET", (char *)0))
        say(o, p, line);
}

static void emit_delete(ud_out *o, ud_part *p, const char *rest)
{
    char line[600];
    if (!join(line, sizeof(line), "If EXISTS \"UP-Term:", p->drawer, "/", rest, "\"", (char *)0)) return;
    say(o, p, line);
    join(line, sizeof(line), "  Delete \"UP-Term:", p->drawer, "/", rest, "\" QUIET", (char *)0);
    say(o, p, line);
    say(o, p, "EndIf");
}

static int is_section(const char *path, const char **name)
{
    if (strncmp(path, "install.dos#", 12)) return 0;
    *name = path + 12;
    return 1;
}

int ud_compare(char *oldtext, char *newtext, const char *kit, ud_result *r, ud_emit emit, void *ctx)
{
    ud_list a, b;
    ud_out *o;
    char oldfrom[UD_MAXPARTS][UD_DRAWER], olddrawer[UD_MAXPARTS][UD_DRAWER], oldname[UD_MAXPARTS][UD_NAME];
    int nold = 0, i = 0, j = 0, all = 0, rc = -1;
    char from[UD_DRAWER], drawer[UD_DRAWER];
    const char *sec;

    memset(r, 0, sizeof(*r));
    a.l = b.l = 0;
    o = (ud_out *)calloc(1, sizeof(ud_out));
    if (!o || parse(oldtext, &a) || parse(newtext, &b)) goto out;
    o->emit = emit;
    o->ctx = ctx;
    /* the copy parts: the new kit's drawers; the old ones to tell a moved drawer */
    for (i = 0; i < b.n; i++) {
        int c = copy_line(b.l[i].path, from, drawer);
        ud_part *p;
        if (c < 0) goto out;
        if (!c) continue;
        if (!(p = part_of(r, b.l[i].part))) goto out;
        p->copy = 1;
        strcpy(p->from, from);
        strcpy(p->drawer, drawer);
    }
    for (i = 0; i < a.n && nold < UD_MAXPARTS; i++) {
        if (copy_line(a.l[i].path, oldfrom[nold], olddrawer[nold]) == 1 && strlen(a.l[i].part) < UD_NAME)
            strcpy(oldname[nold++], a.l[i].part);
    }
    i = j = 0;
    while (i < a.n || j < b.n) {
        int c = (i >= a.n) ? 1 : (j >= b.n) ? -1 : strcmp(a.l[i].path, b.l[j].path);
        if (c == 0 && !strcmp(a.l[i].part, b.l[j].part) && a.l[i].size == b.l[j].size && a.l[i].crc == b.l[j].crc) {
            ud_part *p = part_of(r, b.l[j].part);
            int moved = 0, k;
            /* a copy part whose drawers moved copies every file again */
            if (p && p->copy && !strncmp(b.l[j].path, "Files/", 6)) {
                for (k = 0; k < nold && strcmp(oldname[k], p->name); k++)
                    ;
                moved = k == nold || strcmp(oldfrom[k], p->from) || strcmp(olddrawer[k], p->drawer);
            }
            if (!moved) {
                i++, j++;
                continue;
            }
            c = 1;   /* as a new file */
            i++;
        }
        if (c > 0 || c == 0) {   /* new, or changed: the new kit's line */
            ud_line *l = &b.l[j];
            ud_part *p;
            if (c == 0) i++;
            j++;
            if (!strcmp(l->part, "-") || !strncmp(l->path, "copy#", 5)) continue;
            if (is_section(l->path, &sec)) {
                if (!strcmp(sec, "all")) all = 1;
                else if (strncmp(sec, "copy-", 5)) {   /* the Installer copies those itself */
                    if (!(p = part_of(r, l->part))) goto out;
                    p->run = 1;
                }
                continue;
            }
            if (!(p = part_of(r, l->part))) goto out;
            p->run = 1;
            p->bytes += l->size;
            r->bytes += l->size;
            r->files++;
            if (p->copy) {
                const char *rest = below(l->path, p->from);
                if (rest) emit_copy(o, r, p, kit, l->path, rest);
            }
        } else {                 /* gone: the old kit's line */
            ud_line *l = &a.l[i++];
            ud_part *p;
            int k;
            if (!strcmp(l->part, "-") || !strncmp(l->path, "copy#", 5) || is_section(l->path, &sec)) continue;
            r->gone++;
            if (!(p = part_of(r, l->part))) goto out;
            for (k = 0; k < nold && strcmp(oldname[k], l->part); k++)
                ;
            if (k < nold) {      /* from an old copy part's drawer */
                const char *rest = below(l->path, oldfrom[k]);
                ud_part q = *p;
                strcpy(q.drawer, olddrawer[k]);
                if (rest) emit_delete(o, &q, rest);
                p->run = 1;
            } else
                p->run = 1;      /* a step part: its input set changed */
        }
    }
    if (all)   /* the script's head changed: every step part of the new kit runs */
        for (i = 0; i < b.n; i++)
            if (is_section(b.l[i].path, &sec) && strcmp(sec, "all") && strncmp(sec, "copy-", 5)) {
                ud_part *p = part_of(r, sec);
                if (!p) goto out;
                p->run = 1;
            }
    rc = 0;
out:
    free(a.l);
    free(b.l);
    free(o);
    return rc;
}

unsigned long ud_kb(const ud_part *p)
{
    unsigned long kb = (p->bytes + 1023) / 1024;
    return (p->run && kb == 0) ? 1 : kb;
}
