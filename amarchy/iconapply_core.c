/* iconapply_core -- see iconapply_core.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iconapply_core.h"

static uint32_t crc_upd(uint32_t c, const uint8_t *p, size_t n)
{
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c;
}

uint32_t ia_crc32(const uint8_t *p, size_t n)
{
    return ~crc_upd(0xFFFFFFFFu, p, n);
}

uint32_t ia_imgcrc(const li_file *f)
{
    uint32_t c = crc_upd(0xFFFFFFFFu, f->buf + f->img_off, f->img_end - f->img_off);
    if (f->form_len) c = crc_upd(c, f->buf + f->form_off, f->form_len);
    return ~c;
}

/* ---- manifest ---- */
int ia_man_parse(ia_manifest *m, const char *text, size_t len)
{
    size_t i = 0;
    m->n = 0;
    while (i < len && m->n < IA_MAXENT) {
        char line[IA_MAXPATH + 40], *a, *b;
        size_t s = i, l;
        ia_entry e;
        while (i < len && text[i] != '\n') i++;
        l = i - s;
        if (i < len) i++;
        if (l == 0 || l >= sizeof line || text[s] == '#') continue;
        memcpy(line, text + s, l);
        line[l] = 0;
        a = strstr(line, " ; ");
        if (!a) continue;
        b = strstr(a + 3, " ; ");
        if (!b || a == line || a - line >= IA_MAXPATH) continue;
        memset(&e, 0, sizeof e);
        memcpy(e.path, line, (size_t)(a - line));
        *b = 0;
        if (strcmp(a + 3, "absent") == 0) e.absent = 1;
        else e.ocrc = (uint32_t)strtoul(a + 3, NULL, 16);
        e.wcrc = (uint32_t)strtoul(b + 3, NULL, 16);
        ia_man_put(m, &e);
    }
    return m->n;
}

size_t ia_man_format(const ia_manifest *m, char *out, size_t cap)
{
    size_t n;
    int i, w = snprintf(out, cap, "# amarchy-icons manifest 1\n");
    if (w < 0 || (size_t)w >= cap) return 0;
    n = (size_t)w;
    for (i = 0; i < m->n; i++) {
        const ia_entry *e = &m->e[i];
        if (e->absent) w = snprintf(out + n, cap - n, "%s ; absent ; %08lx\n", e->path, (unsigned long)e->wcrc);
        else w = snprintf(out + n, cap - n, "%s ; %08lx ; %08lx\n", e->path, (unsigned long)e->ocrc, (unsigned long)e->wcrc);
        if (w < 0 || (size_t)w >= cap - n) return 0;
        n += (size_t)w;
    }
    return n;
}

int ia_man_find(const ia_manifest *m, const char *path)
{
    int i;
    for (i = 0; i < m->n; i++)
        if (strcmp(m->e[i].path, path) == 0) return i;
    return -1;
}

void ia_man_put(ia_manifest *m, const ia_entry *e)
{
    int i = ia_man_find(m, e->path);
    if (i < 0) {
        if (m->n >= IA_MAXENT) return;
        i = m->n++;
    }
    m->e[i] = *e;
}

void ia_man_del(ia_manifest *m, int i)
{
    if (i < 0 || i >= m->n) return;
    memmove(&m->e[i], &m->e[i + 1], (size_t)(m->n - i - 1) * sizeof m->e[0]);
    m->n--;
}

/* ---- atomic write ---- */
int ia_atomic_names(const char *path, char *tmp, char *old, size_t cap)
{
    size_t n = strlen(path);
    if (n + 5 > cap) return -1;
    memcpy(tmp, path, n); memcpy(tmp + n, ".amt", 5);
    memcpy(old, path, n); memcpy(old + n, ".amo", 5);
    return 0;
}

int ia_recover(int has_name, int ok_name, int has_tmp, int ok_tmp, int has_old, int ok_old)
{
    int name_good = has_name && ok_name;
    if (has_old && ok_old && !name_good) return IA_REC_OLD_TO_NAME;   /* .amo first */
    if (has_tmp && ok_tmp && !name_good) return IA_REC_TMP_TO_NAME;
    if (has_tmp) return IA_REC_DEL_TMP;
    return IA_REC_NONE;
}

/* ---- targets ---- */
static const char *const g_defs[] = { "def_disk", "def_drawer", "def_tool", "def_project", "def_trashcan", "def_RAM" };
#define NDEF ((int)(sizeof g_defs / sizeof g_defs[0]))

int ia_target(int i, char *path, size_t cap, const char **match)
{
    int dir = i / NDEF, k = i % NDEF;
    if (i < 0 || dir > 1) return 0;
    if ((size_t)snprintf(path, cap, "%s/%s.info", dir ? "ENV:Sys" : "ENVARC:Sys", g_defs[k]) >= cap) return 0;
    *match = g_defs[k];
    return 1;
}

/* a lucide.dat row that names a file: "Vol:Dir/Name". The type fallbacks (type:disk, type:drawer, ...) also carry a colon but
 * name no volume; taken as a path, Lock("type:disk.info") opened the "Please insert volume TYPE:" requester at every APPLY. */
int ia_row_is_path(const char *nm)
{
    static const char ty[] = "type:";
    size_t i;
    if (!strchr(nm, ':')) return 0;
    for (i = 0; ty[i]; i++)
        if ((nm[i] | 0x20) != ty[i]) return 1;
    return 0;
}

/* ---- all-volume walk decisions (L12) ---- */
static int ends_with(const char *s, const char *suf)
{
    size_t n = strlen(s), m = strlen(suf);
    if (n < m) return 0;
    for (s += n - m; *suf; s++, suf++)
        if ((*s | 0x20) != (*suf | 0x20)) return 0;
    return 1;
}

const char *ia_walk_fallback_match(int dotype)
{
    switch (dotype) {
    case 1: return "def_disk";
    case 2: return "def_drawer";
    case 3: return "def_tool";
    case 4: return "def_project";
    case 5: return "def_trashcan";
    default: return NULL;
    }
}

int ia_walk_decide(const ia_walk_entry *e)
{
    if (ends_with(e->name, ".uaem")) return IA_DO_SKIP_UAEM;
    if (e->vol_readonly) return IA_DO_SKIP_READONLY;
    if (e->prot & (IA_PROT_WRITE | IA_PROT_DELETE)) return IA_DO_SKIP_LOCKED;
    if (!e->specific) return IA_DO_FALLBACK;
    return IA_DO_WRITE;
}

int ia_walk_count(ia_ctx *c, int d)
{
    if (d == IA_DO_SKIP_READONLY) c->skipped_readonly++;
    else if (d == IA_DO_SKIP_LOCKED) c->skipped_locked++;
    else if (d == IA_DO_SKIP_UAEM) c->skipped_uaem++;
    else if (d == IA_DO_FALLBACK) c->fallback++;
    else if (d == IA_DO_SKIP_DEEP) c->skipped_deep++;
    return d;
}

int ia_walk_fits(ia_ctx *c, size_t plen, size_t nl, size_t cap, unsigned depth, unsigned maxdepth, int is_dir)
{
    if (plen + nl + 2 >= cap || (is_dir && depth >= maxdepth)) { ia_walk_count(c, IA_DO_SKIP_DEEP); return 0; }
    return 1;
}

/* ---- apply / restore ---- */
static void man_load(ia_ctx *c)
{
    long n = c->io->read(c->io->ctx, IA_MANIFEST, c->bak, IA_FILECAP);
    c->man.n = 0;
    if (n > 0) ia_man_parse(&c->man, (const char *)c->bak, (size_t)n);
}

static int man_save(ia_ctx *c)
{
    size_t n;
    if (c->man.n == 0) {
        c->io->remove(c->io->ctx, IA_MANIFEST);
        return 0;
    }
    n = ia_man_format(&c->man, (char *)c->bak, IA_FILECAP);
    return n ? c->io->write(c->io->ctx, IA_MANIFEST, c->bak, n) : -1;
}

/* Undo the manifest line of a write that did not happen: the previous line comes back, or the path goes. Saved best effort. */
static void man_rollback(ia_ctx *c, const char *path, const ia_entry *prev)
{
    if (prev) ia_man_put(&c->man, prev);
    else ia_man_del(&c->man, ia_man_find(&c->man, path));
    man_save(c);
}

int ia_skip_note(ia_ctx *c, unsigned *ctr, const char *path, const char *reason)
{
    char line[IA_MAXPATH + 48];
    (*ctr)++;
    if (c->io->note) {
        snprintf(line, sizeof line, "skip %s: %s", reason, path);   /* an over-long path is cut, never left silent */
        c->io->note(c->io->ctx, line);
    }
    return 0;
}

/* One icon file: backup, manifest line, icon, with the images of lucide.dat entry `match`. 1 written, 0 not (a counter says why) */
static int apply_one(ia_ctx *c, const char *path, const char *match)
{
    char bpath[IA_MAXPATH + 40];
    long n = c->io->read(c->io->ctx, path, c->cur, IA_FILECAP);
    int absent = 0, e_i, own;
    li_file src, wr;
    li_edit ed;
    li_image nrm, sel;
    size_t outn;
    ia_entry ne, prev;

    if (n == -1) {
        /* only a defaults match has a system default object; a table path that is not on this volume is skipped */
        n = c->io->dflt && strncmp(match, "def_", 4) == 0 ? c->io->dflt(c->io->ctx, match, c->cur, IA_FILECAP) : -1;
        if (n <= 0) { return ia_skip_note(c, &c->skipped_nodefault, path, "nodefault"); }
        absent = 1;
    } else if (n <= 0) { return ia_skip_note(c, &c->skipped_io, path, "io"); }
    if (li_parse(c->cur, (size_t)n, &src) != 0) { return ia_skip_note(c, &c->skipped_bad, path, "bad"); }
    e_i = ia_man_find(&c->man, path);
    own = !absent && e_i >= 0 && ia_imgcrc(&src) == c->man.e[e_i].wcrc;     /* carries an image we wrote: done, or the other icon set */
    if (li_dat_find(c->dat, match, c->variant, &nrm, &sel) != 0) { return ia_skip_note(c, &c->skipped_nodat, path, "nodat"); }
    if (e_i < 0 && c->man.n >= IA_MAXENT) { return ia_skip_note(c, &c->skipped_io, path, "io"); }    /* no manifest line, no write: RESTORE must cover it */
    li_edit_apply(&ed, &src, &nrm, &sel);
    if (c->form_rgb) {
        size_t fn = li_form_build(&nrm, &sel, c->form_rgb, c->form, sizeof c->form);
        if (!fn) { return ia_skip_note(c, &c->skipped_bad, path, "bad"); }
        ed.form_new = c->form;
        ed.form_new_len = fn;
    }
    outn = li_splice(&src, &ed, c->out, IA_FILECAP);
    if (!outn || li_parse(c->out, outn, &wr) != 0) { return ia_skip_note(c, &c->skipped_bad, path, "bad"); }
    if (own && ia_imgcrc(&wr) == c->man.e[e_i].wcrc) { c->skipped_done++; return 0; }

    memset(&ne, 0, sizeof ne);
    strcpy(ne.path, path);
    if (own) {                       /* the set changed: the backup stays the ORIGINAL, only the written-image crc moves on */
        ne.absent = c->man.e[e_i].absent;
        ne.ocrc = c->man.e[e_i].ocrc;
    } else {
        ne.absent = absent;
        ne.ocrc = absent ? 0 : ia_crc32(c->cur, (size_t)n);
    }
    ne.wcrc = ia_imgcrc(&wr);
    if (!absent && !own) {
        if (li_backup_path(path, bpath, sizeof bpath) != 0 || c->io->write(c->io->ctx, bpath, c->cur, (size_t)n) != 0) {
            return ia_skip_note(c, &c->skipped_io, path, "io");
        }
    }
    if (e_i >= 0) prev = c->man.e[e_i];
    ia_man_put(&c->man, &ne);
    if (man_save(c) != 0) { man_rollback(c, path, e_i >= 0 ? &prev : NULL); return ia_skip_note(c, &c->skipped_io, path, "io"); }
    if (c->io->write(c->io->ctx, path, c->out, outn) != 0) {
        /* the manifest lists only icons Amarchy replaced: a failed write takes its line (and a backup nothing refers to) back */
        man_rollback(c, path, e_i >= 0 ? &prev : NULL);
        if (e_i < 0 && !absent && !own) c->io->remove(c->io->ctx, bpath);
        return ia_skip_note(c, &c->skipped_io, path, "io");
    }
    c->written++;
    return 1;
}

const char *ia_dat_for_style(const char *style)
{
    return style && strcmp(style, "outline") == 0 ? IA_DAT_OUTLINE : IA_DAT;
}

void ia_man_count(const ia_manifest *m, const li_dat *dat, ia_man_stats *st)
{
    int i, j;
    unsigned k;
    char path[IA_MAXPATH];
    const char *match;
    memset(st, 0, sizeof *st);
    for (i = 0; i < m->n; i++) {
        const char *ep = m->e[i].path;
        int kind = 2;                       /* 0 default, 1 table path, 2 walked */
        for (j = 0; ia_target(j, path, sizeof path, &match); j++)
            if (strcmp(path, ep) == 0) { kind = 0; break; }
        for (k = 0; kind == 2 && dat && k < dat->count; k++) {
            const char *nm = li_dat_name(dat, k);
            size_t len = strlen(nm);
            if (ia_row_is_path(nm) && len + 6 <= sizeof path && strncmp(ep, nm, len) == 0 && strcmp(ep + len, ".info") == 0) kind = 1;
        }
        st->total++;
        if (kind == 0) st->defaults++; else if (kind == 1) st->paths++; else st->walked++;
    }
}

size_t ia_status(ia_ctx *c, char *out, size_t cap)
{
    ia_man_stats st;
    int w;
    if (c->io->read(c->io->ctx, IA_MANIFEST, c->bak, IA_FILECAP) == -1) {
        w = snprintf(out, cap, "icons=original\n");
    } else {
        man_load(c);
        ia_man_count(&c->man, c->dat, &st);
        w = snprintf(out, cap, "icons=applied entries=%u defaults=%u paths=%u walked=%u\n", st.total, st.defaults, st.paths, st.walked);
    }
    return w < 0 || (size_t)w >= cap ? 0 : (size_t)w;
}

int ia_apply(ia_ctx *c)
{
    char path[IA_MAXPATH];
    const char *match;
    unsigned k;
    int i, wrote = 0;

    man_load(c);
    for (i = 0; ia_target(i, path, sizeof path, &match); i++)
        wrote += apply_one(c, path, match);
    /* the table's path rows (L10: SYS:System, SYS:Utilities, SYS:Prefs, SYS:Tools, ...): "Vol:Dir/Name" -> "Vol:Dir/Name.info" */
    for (k = 0; k < c->dat->count; k++) {
        const char *nm = li_dat_name(c->dat, k);
        size_t len = strlen(nm);
        if (!ia_row_is_path(nm) || len + 6 > sizeof path) continue;
        memcpy(path, nm, len);
        memcpy(path + len, ".info", 6);
        wrote += apply_one(c, path, nm);
    }
    return wrote;
}

int ia_info_type(const uint8_t *buf, size_t len)
{
    return len > 48 ? buf[48] : 0;      /* DiskObject: magic 2, version 2, Gadget 44, then do_Type */
}

int ia_walk_skip_dir(const char *path)
{
    return ends_with(path, "Storage/Icons/Amarchy") || ends_with(path, "Storage/Icons/Amarchy/");
}

int ia_walk_file(ia_ctx *c, const char *path, int vol_readonly, unsigned long prot)
{
    const char *name = path, *p, *match;
    char base[IA_MAXPATH];
    ia_walk_entry e;
    li_file src;
    li_image nrm, sel;
    long n;
    size_t bl;
    int i, d, own;
    unsigned done0;

    for (p = path; *p; p++)
        if (*p == '/' || *p == ':') name = p + 1;
    memset(&e, 0, sizeof e);
    e.name = name;
    e.vol_readonly = vol_readonly;
    e.prot = prot;
    if (ends_with(name, ".uaem") || vol_readonly || (prot & (IA_PROT_WRITE | IA_PROT_DELETE)))
    {
        d = ia_walk_decide(&e);                          /* decided without reading the file */
        if (d == IA_DO_SKIP_UAEM) ia_skip_note(c, &c->skipped_uaem, path, "uaem");
        else if (d == IA_DO_SKIP_READONLY) ia_skip_note(c, &c->skipped_readonly, path, "readonly");
        else if (d == IA_DO_SKIP_LOCKED) ia_skip_note(c, &c->skipped_locked, path, "locked");
        else ia_walk_count(c, d);
        return d;
    }
    n = c->io->read(c->io->ctx, path, c->cur, IA_FILECAP);
    if (n <= 0) { ia_skip_note(c, &c->skipped_io, path, "io"); return IA_DO_SKIP_OTHER; }
    if (li_parse(c->cur, (size_t)n, &src) != 0) { ia_skip_note(c, &c->skipped_bad, path, "bad"); return IA_DO_SKIP_OTHER; }
    own = ia_man_find(&c->man, path) >= 0;
    if (!own)
        for (i = 0; i < c->man.n; i++)                   /* already carries a icon set image we wrote (ENVARC:Sys seen again by its volume path) */
            if (ia_imgcrc(&src) == c->man.e[i].wcrc) { c->skipped_done++; return IA_DO_SKIP_OTHER; }
    bl = strlen(name) - 5;                               /* ".info" */
    if (bl >= sizeof base) { ia_skip_note(c, &c->skipped_bad, path, "bad"); return IA_DO_SKIP_OTHER; }
    memcpy(base, name, bl);
    base[bl] = 0;
    e.dotype = ia_info_type(c->cur, (size_t)n);
    e.specific = li_dat_find(c->dat, base, c->variant, &nrm, &sel) == 0;
    d = ia_walk_decide(&e);
    if (d == IA_DO_FALLBACK) {
        match = ia_walk_fallback_match(e.dotype);
        if (!match) { ia_walk_count(c, d); ia_skip_note(c, &c->skipped_nodat, path, "nodat"); return d; }
    } else {
        match = base;
    }
    done0 = c->skipped_done;
    apply_one(c, path, match);
    if (own && c->skipped_done != done0) return IA_DO_SKIP_OTHER;   /* our own line and already this set: not decided again */
    return ia_walk_count(c, d);
}

void ia_walk_begin(ia_ctx *c) { man_load(c); }

int ia_restore(ia_ctx *c)
{
    int done = 0, i = 0;
    man_load(c);
    while (i < c->man.n) {                                  /* a failed entry stays and the loop goes on: one bad entry must not strand the walked ones behind it */
        ia_entry e = c->man.e[i];
        char bpath[IA_MAXPATH + 40];
        long n = c->io->read(c->io->ctx, e.path, c->cur, IA_FILECAP), bn = 0;
        li_file cur, back;
        li_edit ed;
        size_t outn = 0;
        int ok = 0;

        if (e.absent) {
            /* the file did not exist before APPLY: delete it unless the user changed it since */
            if (n > 0 && li_parse(c->cur, (size_t)n, &cur) == 0 && ia_imgcrc(&cur) == e.wcrc) ok = c->io->remove(c->io->ctx, e.path) == 0;
            else ok = 1;                    /* gone, unreadable (APPLY's write never landed: dir missing) or the user's own file: nothing of ours to remove, drop the entry */
        } else if (li_backup_path(e.path, bpath, sizeof bpath) == 0 &&
                   (bn = c->io->read(c->io->ctx, bpath, c->bak, IA_FILECAP)) > 0 &&
                   li_parse(c->bak, (size_t)bn, &back) == 0) {
            if (n > 0 && li_parse(c->cur, (size_t)n, &cur) == 0) {
                li_edit_restore(&ed, &cur, &back);          /* images only: later Snapshot / Information edits survive */
                outn = li_splice(&cur, &ed, c->out, IA_FILECAP);
            }
            if (!outn) {                                    /* the current file does not parse: whole-file copy */
                memcpy(c->out, c->bak, (size_t)bn);
                outn = (size_t)bn;
            }
            ok = c->io->write(c->io->ctx, e.path, c->out, outn) == 0;
            if (ok) c->io->remove(c->io->ctx, bpath);
        }
        if (!ok) {                                          /* keep the entry (and the backup) for a later run */
            c->restore_failed++;
            i++;
            continue;
        }
        ia_man_del(&c->man, i);
        if (man_save(c) != 0) { c->restore_failed++; break; }
        c->restored++;
        done++;
    }
    return done;
}

static int ci_prefix(const char *a, const char *w)
{
    for (; *w; a++, w++) {
        char x = *a, y = *w;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y) return 0;
    }
    return *a == 0 || *a == ' ' || *a == '\t' || *a == '\n';
}

int ia_parse_cmd(const char *a)
{
    static const struct { const char *w; int c; } t[] = {
        { "APPLY", IA_CMD_APPLY }, { "RESTORE", IA_CMD_RESTORE }, { "STATUS", IA_CMD_STATUS }, { "?", IA_CMD_HELP }, { "--help", IA_CMD_HELP }
    };
    size_t i;
    while (*a == ' ' || *a == '\t') a++;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (ci_prefix(a, t[i].w)) return t[i].c;
    return IA_CMD_NONE;
}

void ia_walk_volume(ia_ctx *c, const char *vol)
{
    char path[IA_MAXPATH], line[IA_MAXPATH + 64];
    const char *what;
    if (!c->io->note || strlen(vol) + 10 >= sizeof path) return;
    snprintf(path, sizeof path, "%sDisk.info", vol);
    if (c->io->read(c->io->ctx, path, c->cur, IA_FILECAP) == -1) what = "Disk.info missing, shows def_disk";
    else if (ia_man_find(&c->man, path) >= 0) what = "Disk.info written";
    else what = "Disk.info present, not written (see its skip line)";
    snprintf(line, sizeof line, "volume %s %s", vol, what);
    c->io->note(c->io->ctx, line);
}
