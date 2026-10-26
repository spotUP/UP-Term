/* index_core.c -- see index_core.h. */
#include <stdio.h>
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

int ix_net_handler(const char *handler)
{
    static const char *const known[] = {"smb", "nfs", "ftp", 0};
    const char *base = handler, *p;
    int i;
    if (!handler)
        return 0;
    for (p = handler; *p; p++)
        if (*p == ':' || *p == '/')
            base = p + 1;
    for (i = 0; known[i]; i++) {
        int k = (int)strlen(known[i]), j;
        for (j = 0; j < k && base[j] && lower((unsigned char)base[j]) == known[i][j]; j++)
            ;
        if (j == k)
            return 1;
    }
    return 0;
}

int ix_volume_wanted(const char *vol, int network, const char *volumes)
{
    const char *p = volumes;
    int vl = (int)strlen(vol);
    if (!network)
        return 1;
    while (p && *p) {
        const char *e = strchr(p, ','), *s = p, *f;
        int len, i;
        if (!e)
            e = p + strlen(p);
        f = e;
        while (s < f && (*s == ' ' || *s == '\t'))
            s++;
        while (f > s && (f[-1] == ' ' || f[-1] == '\t' || f[-1] == ':'))
            f--;
        len = (int)(f - s);
        if (len > 0 && len == vl) {
            for (i = 0; i < len && lower((unsigned char)s[i]) == lower((unsigned char)vol[i]); i++)
                ;
            if (i == len)
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
    memset(x->kstart, 0, sizeof x->kstart);
}

int ix_add_clips(ix_index *x, const char *const *labels, int n)
{
    int base = x->kstart[IX_NKINDS + 1] ? x->kstart[IX_NKINDS] : x->n, i, add = n > 0 ? n : 1;
    ix_row *rows = (ix_row *)realloc(x->rows, (size_t)(base + add) * sizeof *rows);
    if (!rows)
        return 0;
    x->rows = rows;
    for (i = 0; i < add; i++) {
        ix_row *r = &rows[base + i];
        memset(r, 0, sizeof *r);
        r->kind = IX_KIND_CLIP;
        if (n > 0) {
            strncpy(r->label, labels[i], IX_LABEL - 1);
            snprintf(r->path, sizeof r->path, "clip:%d", i);
        } else {
            strcpy(r->label, "Nothing copied yet");
            strcpy(r->path, IX_CLIP_NONE);
        }
    }
    x->kstart[IX_NKINDS] = base;
    x->kstart[IX_NKINDS + 1] = base + add;
    x->kstart[IX_NKINDS + 2] = x->kstart[IX_NKINDS + 3] = 0;   /* the lister rows sat after the ring: gone */
    x->n = base + add;
    return 1;
}

int ix_add_listers(ix_index *x, const ix_row *lrows, int n)
{
    int base = x->kstart[IX_NKINDS + 3] ? x->kstart[IX_NKINDS + 2] : x->n;
    ix_row *rows;
    if (n <= 0) {
        x->n = base;
        x->kstart[IX_NKINDS + 2] = x->kstart[IX_NKINDS + 3] = 0;
        return 1;
    }
    rows = (ix_row *)realloc(x->rows, (size_t)(base + n) * sizeof *rows);
    if (!rows)
        return 0;
    x->rows = rows;
    memcpy(rows + base, lrows, (size_t)n * sizeof *rows);
    x->kstart[IX_NKINDS + 2] = base;
    x->kstart[IX_NKINDS + 3] = base + n;
    x->n = base + n;
    return 1;
}

int ix_lister_handles(const char *answer, unsigned long *out, int max)
{
    int n = 0;
    const char *p = answer;
    while (p && *p && n < max) {
        char *end;
        unsigned long v;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (!*p)
            break;
        if (*p >= '0' && *p <= '9') {
            v = strtoul(p, &end, 10);
            if (*end == 0 || *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
                out[n++] = v;
                p = end;
                continue;
            }
        }
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            p++;
    }
    return n;
}

int ix_lister_row(unsigned long handle, const char *path_answer, ix_row *out)
{
    char buf[IX_PATH];
    const char *s = path_answer ? path_answer : "", *label;
    size_t len;
    if (!out)
        return 0;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        len--;
    if (len >= sizeof buf)
        len = sizeof buf - 1;
    memcpy(buf, s, len);
    buf[len] = 0;
    memset(out, 0, sizeof *out);
    out->kind = IX_KIND_LISTER;
    if (!len) {
        strcpy(out->label, "Device list");
    } else {
        size_t e = len;
        if (buf[e - 1] == '/' )
            buf[--e] = 0;                       /* "Work:Dev/" names Dev */
        label = buf;
        while (e > 0 && buf[e - 1] != '/' && buf[e - 1] != ':')
            e--;
        if (e > 0 && buf[e - 1] == '/')
            label = buf + e;
        else if (e > 0)
            label = buf + e;                    /* "Work:Dev" gives Dev */
        if (!*label)
            label = buf;                        /* volume root "Work:" */
        strncpy(out->label, label, IX_LABEL - 1);
    }
    snprintf(out->path, sizeof out->path, "lister:%lu", handle);
    return 1;
}

long ix_lister_of(const char *path)
{
    char *end;
    unsigned long v;
    if (!path || strncmp(path, "lister:", 7) != 0 || path[7] < '0' || path[7] > '9')
        return -1;
    v = strtoul(path + 7, &end, 10);
    return *end ? -1 : (long)v;
}

int ix_level_cap(const ix_index *x)
{
    return MN_MAX_ITEMS + (x ? x->n : 0) + 2;
}

static char extra_music[128], extra_demo[128], extra_hidden[128];

void ix_set_drawers(const char *music, const char *demo)
{
    strncpy(extra_music, music ? music : "", sizeof extra_music - 1);
    strncpy(extra_demo, demo ? demo : "", sizeof extra_demo - 1);
}

void ix_set_hidden_drawers(const char *list)
{
    strncpy(extra_hidden, list ? list : "", sizeof extra_hidden - 1);
}

/* Is drawer (len chars) one of the names in the comma separated list (blanks around a name ignored)? */
static int in_list(const char *list, const char *drawer, int len)
{
    while (*list) {
        const char *e = strchr(list, ',');
        int l = e ? (int)(e - list) : (int)strlen(list), a = 0, b = l;
        while (a < b && list[a] == ' ')
            a++;
        while (b > a && list[b - 1] == ' ')
            b--;
        if (b - a == len && len > 0) {
            int i = 0;
            while (i < len && lower((unsigned char)list[a + i]) == lower((unsigned char)drawer[i]))
                i++;
            if (i == len)
                return 1;
        }
        list += l + (e ? 1 : 0);
    }
    return 0;
}

int ix_row_kind(const ix_row *r)
{
    char seg[KM_ID], lab[KM_LABEL];
    int len;
    if (!ix_category(r->path, seg, sizeof seg, lab, sizeof lab))
        return r->kind == IX_KIND_GAME ? IX_KIND_GAME : IX_KIND_APP;
    len = (int)strlen(lab);
    if (in_list("Music,Musicdisks,Musicdisk,Mods,Tunes", lab, len) || in_list(extra_music, lab, len))
        return IX_KIND_MUSIC;
    if (in_list("Demos,Demo,Scene,Intros,Cracktros", lab, len) || in_list(extra_demo, lab, len))
        return IX_KIND_DEMO;
    if (r->kind == IX_KIND_APP && (in_list("C,Storage,Utilities,WBStartup,Libs,Devs,MUI", lab, len) || in_list(extra_hidden, lab, len)))
        return IX_KIND_HIDDEN;
    if (r->kind == IX_KIND_APP)
        return in_list("Prefs", lab, len) ? IX_KIND_SETTINGS : IX_KIND_APP;
    return IX_KIND_GAME;
}

static const char *const kind_node[IX_NKINDS] = { "apps", "games", "demos", "music", "settings" };
static const char *const kind_label[IX_NKINDS] = { "Apps", "Games", "Demos", "Music", "Settings" };

void ix_load_text(const keymap *k, ix_index *x, const char *text)
{
    ix_row *all = 0;
    const char *t;
    int cap = 1, n = 0, i, na = 0, kd;
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
        all[i].kind = ix_row_kind(&all[i]);
    for (kd = 0; kd < IX_NKINDS; kd++) {         /* rows grouped by effective kind, each group still sorted; IX_KIND_HIDDEN rows match no kind and drop out */
        int cnt = 0;
        x->kstart[kd] = na;
        for (i = 0; i < n; i++)
            if (all[i].kind == kd)
                x->rows[na + cnt++] = all[i];
        na += ix_drop_shadowed(k, kind_node[kd], x->rows + na, cnt);
    }
    x->kstart[IX_NKINDS] = na;
    x->napps = x->kstart[1];
    x->n = na;
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
        if (MN_IS_GROUP(&it[i]) && !cmp_ci(it[i].seg, seg))
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

/* add_index for the rows of [from, to) that fall in category cat (NULL: the rows with no category) */
static int add_index_cat(const ix_index *x, int from, int to, const char *cat, mn_item *out, int n, int max)
{
    int i;
    for (i = from; i < to && n < max; i++) {
        char seg[KM_ID];
        int has = ix_category(x->rows[i].path, seg, sizeof seg, 0, 0);
        if (cat ? (has && !cmp_ci(seg, cat)) : !has)
            n = add_index(x, i, i + 1, out, n, max);
    }
    return n;
}

int ix_category(const char *path, char *seg, int n, char *label, int ln)
{
    const char *c = strchr(path, ':'), *e;
    int len, i;
    if (!c || !(e = strchr(c + 1, '/')) || e == c + 1)
        return 0;                                  /* no volume, or an app at the volume root */
    len = (int)(e - (c + 1));
    if (seg && n > 0) {
        int m = len < n - 1 ? len : n - 1;
        for (i = 0; i < m; i++)
            seg[i] = c[1 + i] == '.' ? '_' : c[1 + i];   /* a dot would break the node path */
        seg[m] = 0;
    }
    if (label && ln > 0) {
        int m = len < ln - 1 ? len : ln - 1;
        memcpy(label, c + 1, m);
        label[m] = 0;
    }
    return 1;
}

/* Do the rows [from, to) all lie in one and the same category, or all at the volume root? */
static int one_category(const ix_index *x, int from, int to)
{
    char first[KM_ID], seg[KM_ID];
    int i, has0;
    if (from >= to)
        return 1;
    has0 = ix_category(x->rows[from].path, first, sizeof first, 0, 0);
    for (i = from + 1; i < to; i++) {
        int has = ix_category(x->rows[i].path, seg, sizeof seg, 0, 0);
        if (has != has0 || (has && cmp_ci(seg, first)))
            return 0;
    }
    return 1;
}

int ix_level(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
             int max)
{
    return ix_level_t(k, x, node, filter, out, max, MN_COND_ALL);
}

int ix_level_t(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
               int max, int cond)
{
    int from = 0, to = 0, kd, kl = 0;
    const char *cat;
    int n = mn_children_t(k, node, out, max, cond);
    if (!x)
        return n;
    for (kd = 0; kd < n; kd++)                      /* Trigger > Clipboard history is a group of ring rows */
        if (out[kd].row >= 0 && (!strcmp(k->menu[out[kd].row].id, IX_CLIPS_NODE) ||
                                 !strcmp(k->menu[out[kd].row].id, IX_LISTERS_NODE))) {
            out[kd].row = MN_ROW_GROUP;
            out[kd].ix = -1;
        }
    if (!strcmp(node, IX_CLIPS_NODE))
        return add_index(x, x->kstart[IX_NKINDS], x->kstart[IX_NKINDS + 1], out, n, max);
    if (!strcmp(node, IX_LISTERS_NODE))
        return add_index(x, x->kstart[IX_NKINDS + 2], x->kstart[IX_NKINDS + 3], out, n, max);
    for (kd = 0; kd < IX_NKINDS; kd++) {
        kl = (int)strlen(kind_node[kd]);
        if (!strncmp(node, kind_node[kd], kl) && (!node[kl] || node[kl] == '.'))
            break;
    }
    if (kd < IX_NKINDS) {
        from = x->kstart[kd];
        to = x->kstart[kd + 1];
        cat = node[kl] ? node + kl + 1 : 0;
        if (cat) {                                  /* a category: its apps, from every volume */
            n = add_index_cat(x, from, to, cat, out, n, max);
        } else if (kd == IX_KIND_SETTINGS || one_category(x, from, to)) {
            n = add_index(x, from, to, out, n, max);   /* flat: one category (or none) would be a lone group */
        } else {                                    /* the categories, then the volume-root apps */
            char last[KM_ID], seg[KM_ID], lab[KM_LABEL], best[KM_ID], blab[KM_LABEL];
            int i, any = 0, found;
            for (;;) {
                found = 0;
                for (i = from; i < to; i++)
                    if (ix_category(x->rows[i].path, seg, sizeof seg, lab, sizeof lab) &&
                        (!any || cmp_ci(seg, last) > 0) && (!found || cmp_ci(seg, best) < 0)) {
                        strcpy(best, seg);
                        strcpy(blab, lab);
                        found = 1;
                    }
                if (!found)
                    break;
                strcpy(last, best);
                any = 1;
                if (!has_group(out, n, best))
                    n = add_group(out, n, max, best, blab);
            }
            if (filter && *filter)                  /* typing searches every app, whatever its category */
                n = add_index(x, from, to, out, n, max);
            else
                n = add_index_cat(x, from, to, 0, out, n, max);
        }
    } else if (!*node) {
        for (kd = 0; kd < IX_NKINDS; kd++)
            if (x->kstart[kd + 1] > x->kstart[kd] && !has_group(out, n, kind_node[kd]))
                n = add_group(out, n, max, kind_node[kd], kind_label[kd]);
        if (filter && *filter)
            n = add_index(x, 0, x->kstart[IX_NKINDS], out, n, max);
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

int ix_same_handler_task(const void *vol_task, const void *dev_task)
{
    return vol_task != 0 && vol_task == dev_task;
}

const char *const ix_prog_old_names[] = { "Amarchy.index.2", "Amarchy.index", 0 };

void ix_prog_begin(ix_prog *p)
{
    long runs = p->version == IX_PROG_VERSION ? p->runs : 0;   /* a fresh or other-layout record counts from 0 */
    memset(p, 0, sizeof *p);
    p->runs = runs + 1;
    p->version = IX_PROG_VERSION;
    p->stage = IX_ST_VOLUMES;
    p->seq = 1;
}

void ix_prog_note(ix_prog *p, int stage, const char *path)
{
    size_t n = strlen(path);
    if (n >= IX_PATH)
        n = IX_PATH - 1;
    memcpy(p->path, path, n);
    p->path[n] = 0;
    p->stage = stage;
    if (stage == IX_ST_WALK)
        p->dirs++;
    else if (stage == IX_ST_ICON)
        p->icons++;
    p->seq++;
}

void ix_prog_done(ix_prog *p, long napps, long ngames, long nvols)
{
    p->stage = IX_ST_DONE;
    p->napps = napps;
    p->ngames = ngames;
    p->nvols = nvols;
    p->seq++;
}

void ix_prog_stopped(ix_prog *p)
{
    p->stage = IX_ST_STOPPED;
    p->seq++;
}

void ix_prog_write_failed(ix_prog *p)
{
    p->stage = IX_ST_WRITE_FAILED;
    p->seq++;
}

const char *ix_stage_name(int stage)
{
    static const char *const names[] = {"idle", "volumes", "walk", "icon", "write", "done", "stopped",
                                        "write failed"};
    return (stage >= 0 && stage <= IX_ST_WRITE_FAILED) ? names[stage] : "unknown";
}

/* a bounded string builder: no printf family, so vbcc's plain DOS programs can take it */
typedef struct pbuf {
    char *s;
    int n, len, over;
} pbuf;

static void pb_str(pbuf *b, const char *t)
{
    for (; *t; t++) {
        if (b->len + 1 >= b->n) {
            b->over = 1;
            return;
        }
        b->s[b->len++] = *t;
    }
}

static void pb_num(pbuf *b, long v)
{
    char t[24], one[2];
    int i = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do
        t[i++] = (char)('0' + u % 10);
    while ((u /= 10) != 0);
    if (v < 0)
        t[i++] = '-';
    one[1] = 0;
    while (i) {
        one[0] = t[--i];
        pb_str(b, one);
    }
}

int ix_prog_other_text(char *out, int n)
{
    static const char t[] = "amarchy-index: record from another amarchy-index build, reboot to clear\n";
    if (n < (int)sizeof t)
        return -1;
    memcpy(out, t, sizeof t);
    return (int)sizeof t - 1;
}

int ix_prog_text(const ix_prog *p, int task, int moving, char *out, int n)
{
    char vol[IX_VOL];
    const char *state;
    pbuf b;
    b.s = out;
    b.n = n;
    b.len = 0;
    b.over = 0;
    if (n < 1)
        return -1;
    if (p->version != IX_PROG_VERSION)
        return ix_prog_other_text(out, n);
    if (p->stage == IX_ST_DONE)
        state = "done";
    else if (p->stage == IX_ST_WRITE_FAILED)
        state = "cache write failed (cannot write the cache file, the old cache is kept)";
    else if (p->stage == IX_ST_STOPPED)
        state = "stopped by Ctrl-C, no cache written";
    else if (p->stage == IX_ST_IDLE)
        state = "no scan has run";
    else if (!task)
        state = "scanner task is gone before the scan finished (it died)";
    else if (moving)
        state = "running";
    else
        state = "scanner task exists but made no progress (stuck, or stopped at a requester)";
    ix_volume_of(p->path, vol, sizeof vol);
    pb_str(&b, "amarchy-index: ");
    pb_str(&b, state);
    pb_str(&b, "\nstage: ");
    pb_str(&b, ix_stage_name(p->stage));
    pb_str(&b, "\n");
    if (p->stage == IX_ST_DONE) {
        pb_num(&b, p->napps);
        pb_str(&b, " apps, ");
        pb_num(&b, p->ngames);
        pb_str(&b, " games on ");
        pb_num(&b, p->nvols);
        pb_str(&b, " volumes\n");
    } else {
        pb_str(&b, "volume: ");
        pb_str(&b, vol);
        pb_str(&b, "\n");
    }
    pb_str(&b, "path: ");
    pb_str(&b, p->path);
    pb_str(&b, "\ndrawers: ");
    pb_num(&b, p->dirs);
    pb_str(&b, ", icons: ");
    pb_num(&b, p->icons);
    pb_str(&b, "\n");
    out[b.len] = 0;
    return b.over ? -1 : b.len;
}

/* "<head><path>" into out, the path cut from the left (a leading "...") when it does not fit n. */
static void watch_text(char *out, int n, const char *head, const char *path)
{
    int hl = (int)strlen(head), pl = (int)strlen(path), room = n - 1 - hl;
    if (room < 4) {
        out[0] = 0;
        return;
    }
    memcpy(out, head, hl);
    if (pl <= room) {
        memcpy(out + hl, path, pl + 1);
        return;
    }
    memcpy(out + hl, "...", 3);
    memcpy(out + hl + 3, path + pl - (room - 3), room - 3 + 1);
}

static void watch_done(char *out, int n, const ix_prog *p)
{
    pbuf pb;
    pb.s = out;
    pb.n = n;
    pb.len = 0;
    pb.over = 0;
    pb_str(&pb, "Scan done: ");
    pb_num(&pb, p->napps);
    pb_str(&pb, " apps, ");
    pb_num(&pb, p->ngames);
    pb_str(&pb, " games");
    out[pb.len < n ? pb.len : n - 1] = 0;
}

static int stage_running(int st)
{
    return st == IX_ST_VOLUMES || st == IX_ST_WALK || st == IX_ST_ICON || st == IX_ST_WRITE;
}

int ix_scan_live(int have, const ix_prog *p, int alive)
{
    return have && alive && p->version == IX_PROG_VERSION && stage_running(p->stage);
}

int ix_watch_row(ix_watch *w, int have, const ix_prog *p, int alive, char *out, int n)
{
    int st;
    if (n < 1)
        return IXW_NONE;
    out[0] = 0;
    if (have && p->version != IX_PROG_VERSION)
        have = 0;                                   /* another layout: its fields are not read */
    if (w->runs0 >= 0 && (!have || p->runs <= w->runs0)) {
        if (++w->polls > IXW_START_POLLS) {
            strncpy(out, "Scan did not start", n - 1);
            out[n - 1] = 0;
            return IXW_FAILED;
        }
        strncpy(out, "Scanning... starting", n - 1);
        out[n - 1] = 0;
        return IXW_STARTING;
    }
    if (!have)
        return IXW_NONE;
    st = p->stage;
    if (stage_running(st)) {
        if (w->runs0 < 0) {
            if (!alive)
                return IXW_NONE;                    /* a scan that died long ago is not news */
            w->runs0 = p->runs - 1;                 /* adopt the running scan: its end is shown */
        }
        watch_text(out, n, alive ? "Scanning... " : "Scan stopped working at ", p->path);
        return alive ? IXW_RUNNING : IXW_FAILED;
    }
    if (w->runs0 < 0)
        return IXW_NONE;                            /* an old result is not news */
    if (st == IX_ST_STOPPED) {
        watch_text(out, n, "Scan stopped at ", p->path);
        return IXW_FAILED;
    }
    if (st == IX_ST_WRITE_FAILED) {
        strncpy(out, "Scan could not save the list", n - 1);
        out[n - 1] = 0;
        return IXW_FAILED;
    }
    if (st == IX_ST_DONE) {
        watch_done(out, n, p);
        return IXW_DONE;
    }
    return IXW_NONE;
}

void ix_watcher_begin(ix_watcher *x, int started, int have, const ix_prog *p, int alive)
{
    memset(x, 0, sizeof *x);
    if (started) {
        x->w.runs0 = have && p->version == IX_PROG_VERSION ? p->runs : 0;
        x->on = 1;
        x->state = IXW_STARTING;
        strcpy(x->row, "Scanning... starting");
        return;
    }
    x->w.runs0 = -1;
    ix_watcher_step(x, have, p, alive);
    x->on = x->state == IXW_RUNNING;
    if (!x->on)
        x->row[0] = 0;
}

int ix_watcher_adopt_live(ix_watcher *x, int have, const ix_prog *p, int alive)
{
    if (!ix_scan_live(have, p, alive))
        return 0;
    ix_watcher_begin(x, 0, have, p, alive);
    return 1;
}

int ix_watcher_step(ix_watcher *x, int have, const ix_prog *p, int alive)
{
    char row[IXW_ROW];
    int st = ix_watch_row(&x->w, have, p, alive, row, sizeof row), changed;
    changed = strcmp(row, x->row) != 0 || st != x->state;
    memcpy(x->row, row, sizeof row);
    x->state = st;
    if (st == IXW_DONE || st == IXW_FAILED)
        x->on = 0;
    return changed;
}
