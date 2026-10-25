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
    return ix_level_t(k, x, node, filter, out, max, 1);
}

int ix_level_t(const keymap *k, const ix_index *x, const char *node, const char *filter, mn_item *out,
               int max, int term)
{
    int n = mn_children_t(k, node, out, max, term);
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
    if (st == IX_ST_VOLUMES || st == IX_ST_WALK || st == IX_ST_ICON || st == IX_ST_WRITE) {
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
