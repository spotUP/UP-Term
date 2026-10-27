/* The portable decision of leaf.h: which way a leaf's kind runs. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "leaf.h"

lf_how lf_plan(int kind)
{
    switch (kind) {
    case KA_RUN:
        return LF_WINDOW;
    case KA_JOB:
        return LF_JOB;
    case KA_DETACH:
        return LF_DETACH;
    case KA_SCAN:
        return LF_SCAN;
    case KA_REXX:
        return LF_REXX;
    case KA_OPEN:
        return LF_OPEN;
    case KA_VIEWER:
        return LF_VIEWER;
    case KA_WIN:
        return LF_WIN;
    case KA_GTHEME:
        return LF_GTHEME;
    default:
        return LF_REQUEST;
    }
}

int lf_pkg_search_row(const char *node, const mn_item *it)
{
    return node && it && MN_IS_GROUP(it) && !strcmp(node, IX_PKGS_NODE) && !strcmp(it->seg, "search");
}

int lf_pkg_search(ix_index *x, const lf_ui *ui)
{
    char text[64];
    text[0] = 0;
    if (!x || !ui || !ui->ask_value || !ui->ask_value(ui->ctx, "Search packages", "text", text, sizeof text) || !text[0])
        return 0;
    ix_pkg_query(x, text);
    return 1;
}

int lf_pkg_back_levels(const mn_item *it)
{
    if (!it || !MN_IS_PKGACT(it) || it->ix < 0)
        return 0;
    return it->seg[0] == PK_ACT_REMOVE ? 2 : it->seg[0] == PK_ACT_NO || it->seg[0] == PK_ACT_INSTALL ||
           it->seg[0] == PK_ACT_UPGRADE ? 1 : 0;
}

void lf_job_done(ix_index *x, const char *line, int rc)
{
    (void)rc;                           /* the exit code does not matter: even a failed job may have changed packages */
    if (x && line && !strncmp(line, "upkg ", 5))
        ix_pkg_mark_stale(x);
}

int lf_job_view(const lf_ui *ui)
{
    return ui && ui->run_job;
}

lf_how lf_plan_t(int kind, int term)
{
    return kind == KA_SLASH && !term ? LF_OPEN_THEN_REQUEST : lf_plan(kind);
}

lf_how lf_plan_ta(int kind, int term, int needs_value)
{
    return kind == KA_SLASH && !term && needs_value ? LF_ASK_THEN_OPEN_REQUEST : lf_plan_t(kind, term);
}

const char *lf_rexx_notice(const char *script, long rc)
{
    const char *base = script ? script : "";
    const char *p;
    for (p = base; *p; p++)
        if (*p == '/' || *p == ':')
            base = p + 1;
    if (rc == 5 && strncmp(base, "dopus-", 6) == 0)
        return "Directory Opus is not running";
    return 0;
}

int lf_rexx_notice_line(char *out, int cap, const char *script, const char *spec)
{
    const char *note = lf_rexx_notice(script, 5);
    const char *p;
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!note || !spec || !*spec)
        return 0;
    for (p = script; *p; p++)
        if (*p == '\'' || *p == '"' || (unsigned char)*p < ' ')
            return 0;
    for (p = spec; *p; p++)
        if (*p == '\'' || *p == '"' || (unsigned char)*p < ' ')
            return 0;
    n = snprintf(out, (size_t)cap,
                 "RX \"r = '%s'(); if r = 5 then do; if open(w, '%s', 'W') then do; "
                 "call writech(w, '%s'); address command 'Wait 3 SECS'; call close(w); end; end\"",
                 script, spec, note);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_screen_spec(char *out, int cap, const char *spec, const char *screen)
{
    static const char opt[] = "/SCREEN ";
    int n = 0, i;
    if (cap < 1)
        return 0;
    out[0] = 0;
    for (i = 0; spec[i]; i++) {
        if (n >= cap - 1)
            return 0;
        out[n++] = spec[i];
    }
    if (screen && screen[0]) {
        for (i = 0; opt[i]; i++) {
            if (n >= cap - 1) { out[0] = 0; return 0; }
            out[n++] = opt[i];
        }
        for (i = 0; screen[i]; i++) {
            if (screen[i] == '/' || n >= cap - 1) { out[0] = 0; return 0; }
            out[n++] = screen[i];
        }
    }
    out[n] = 0;
    return 1;
}

/* Directory Opus 5 has no per-lister raise (DOpusM2_ARexx.guide): `dopus front` moves the whole Opus screen forward,
 * and `lister set <h> visible off` then `visible on` re-opens that one lister on top of the others. */
int lf_lister_front_command(char *out, int cap, int step, unsigned long handle)
{
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (step == 0)
        n = snprintf(out, (size_t)cap, "dopus front");
    else if (step == 1 || step == 2)
        n = snprintf(out, (size_t)cap, "lister set %lu visible %s", handle, step == 1 ? "off" : "on");
    else
        return 0;
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_viewer_command(char *out, int cap, const char *dir)
{
    int n;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!dir || !*dir || strchr(dir, '"'))
        return 0;
    n = snprintf(out, (size_t)cap, "\"%s%samarchy-gui\" viewer", dir,
                 dir[strlen(dir) - 1] == ':' || dir[strlen(dir) - 1] == '/' ? "" : "/");
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_shell_command(char *out, int cap, const char *vsh, const char *cmd)
{
    int n;
    const char *p;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!cmd || !*cmd)
        return 0;
    if (!vsh || !*vsh) {                /* an older install: the AmigaDOS command path */
        if ((int)strlen(cmd) >= cap)
            return 0;
        strcpy(out, cmd);
        return 1;
    }
    for (p = cmd; *p; p++)              /* a quote would end the argument; no control characters */
        if (*p == '"' || (unsigned char)*p < ' ')
            return 0;
    n = snprintf(out, (size_t)cap, "\"%s\" -c \"%s\"", vsh, cmd);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

int lf_command_program(const char *line, char *out, int cap)
{
    const char *p = line, *e;
    int q;
    while (*p == ' ' || *p == '\t')
        p++;
    q = *p == '"';
    p += q;
    for (e = p; *e && (q ? *e != '"' : *e != ' ' && *e != '\t'); e++)
        ;
    if (e == p || (q && *e != '"') || e - p >= cap)
        return 0;
    memcpy(out, p, (size_t)(e - p));
    out[e - p] = 0;
    return 1;
}

/* the n characters at s end in suffix, any case (vbcc has no strncasecmp) */
static int ends_ci(const char *s, int n, const char *suffix)
{
    int k = (int)strlen(suffix), i;
    if (n <= k)
        return 0;
    for (i = 0; i < k; i++)
        if (tolower((unsigned char)s[n - k + i]) != suffix[i])
            return 0;
    return 1;
}

/* the first word of arg names a script file rather than starting ARexx program text */
static int rexx_is_script(const char *arg)
{
    int n = 0;
    while (arg[n] && arg[n] != ' ' && arg[n] != '\t') {
        if (arg[n] == ':' || arg[n] == '/')
            return 1;
        n++;
    }
    return ends_ci(arg, n, ".rexx") || ends_ci(arg, n, ".rx");
}

int lf_rexx_command(char *out, int cap, const char *arg)
{
    int n = 0, script;
    const char *p;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!arg || !*arg)
        return 0;
    for (p = arg; *p; p++)
        if ((unsigned char)*p < ' ' && *p != '\t')
            return 0;
    script = rexx_is_script(arg);
    n = snprintf(out, (size_t)cap, script ? "RX %s" : "RX \"%s\"", arg);
    if (n < 0 || n >= cap) {
        out[0] = 0;
        return 0;
    }
    if (!script)                        /* a '"' inside the string would end it early */
        for (n = 4; out[n + 1]; n++)
            if (out[n] == '"')
                out[n] = '\'';
    return 1;
}

int lf_launch_fill(lf_launch *b, const char *path)
{
    memset(b, 0, sizeof *b);
    if (!path || !*path || strlen(path) >= LF_LAUNCH_PATH)
        return 0;
    strcpy(b->path, path);
    return 1;
}

int lf_clip_entry(const char *path)
{
    int n = 0;
    if (strncmp(path, "clip:", 5))
        return -2;
    path += 5;
    if (!*path || !strcmp(path, "none"))
        return -1;
    for (; *path; path++) {
        if (*path < '0' || *path > '9' || n > 1000)
            return -1;
        n = n * 10 + (*path - '0');
    }
    return n;
}

/* ---- LF_SETTING ---- */

int lf_setting_next(const sd_setting *s, int cur)
{
    if (s->kind == SD_CHECK)
        return cur ? 0 : 1;
    if (s->kind == SD_CYCLE && s->nvalues > 0)
        return (cur + 1) % s->nvalues;
    return -1;
}

/* One 16 KiB file at a time: static, the Amiga callers run on a small stack. */
static upconf lf_conf, lf_work;
static char lf_buf[UC_MAX_FILE + 2];
static char lf_tbuf[UC_MAX_FILE + 2];   /* a theme file */
static char lf_active[UC_NAME] = "default";

const char *lf_profile(void)
{
    return lf_active;
}

/* lf_conf and the buffers are re-read on every call; the active profile name is the one thing that outlives
 * an open. The warm menu server calls this at the start of each open. */
void lf_reset(void)
{
    strcpy(lf_active, "default");
}

/* The UP-Term file as the Prefs window reads it: ENV:, else ENVARC:. The prefs_load result. */
static int lf_load_upterm(const amconf_io *io)
{
    long got = io->read(io->fs->ctx, prefs_conf_path[PREFS_T_ENV], lf_buf, UC_MAX_FILE + 1);
    if (got <= 0)
        got = io->read(io->fs->ctx, prefs_conf_path[PREFS_T_ENVARC], lf_buf, UC_MAX_FILE + 1);
    if (got < 0)
        got = 0;
    lf_buf[got] = 0;
    return prefs_load(&lf_conf, lf_buf, got, UC_MAX_FILE);
}

/* Stage f over the loaded table (lf_conf, loaded by the caller) as profile `prof` and write both files
 * (ENVARC: then ENV:) and the highlight category variables, like Prefs Save. */
static int lf_put_fields(const amconf_io *io, const char *prof, const prefs_fields *f, int *failed)
{
    long len;
    int r, t;
    len = prefs_stage(&lf_work, &lf_conf, prof, f, lf_buf, UC_MAX_FILE + 1);
    if (len == PREFS_STAGE_LOSSY)
        return LF_SET_READONLY;
    if (len < 0)
        return LF_SET_FULL;
    r = prefs_save_at(io->fs, io->dir, &prefs_paths_upterm, lf_buf, len, 1, failed);
    if (r != PREFS_INSTALL_OK)
        return r;
    for (t = PREFS_T_ENVARC; t >= PREFS_T_ENV; t--) {
        *failed = t;
        r = prefs_write_hlcat(io->fs, t, f->hlcat);
        if (r != PREFS_INSTALL_OK)
            return r;
    }
    return LF_SET_OK;
}

/* index >= 0: a check or cycle value; else the text of an SD_INT or SD_STRING row */
static int lf_set_upterm(const amconf_io *io, const sd_setting *s, int index, const char *text, int n, const char *text2, int *failed)
{
    prefs_fields f;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    if (text)
        strcpy(sd_string(s, &f, n), text);
    else
        sd_set_index(s, &f, index);
    if (text2 && s->off2)
        strcpy((char *)&f + s->off2, text2);
    if (prefs_validate(&f))
        return LF_SET_INVALID;
    return lf_put_fields(io, LF_PROFILE, &f, failed);
}

int lf_setting_set(const amconf_io *io, const sd_setting *s, int index, int *failed)
{
    *failed = 0;
    if (s->kind != SD_CHECK && s->kind != SD_CYCLE)
        return LF_SET_KIND;
    if (index < 0 || index >= s->nvalues)
        return LF_SET_KIND;
    if (s->store == SD_AMARCHY) {
        am_prefs a;
        amconf_load_with(io, &a);
        sd_set_index(s, &a, index);
        return amconf_save_with(io, &a, 1, failed);
    }
    return lf_set_upterm(io, s, index, 0, 0, 0, failed);
}

int lf_setting_set_text(const amconf_io *io, const sd_setting *s, const char *text, int *failed)
{
    *failed = 0;
    if (sd_text_check(s, text) == SD_TEXT_KIND)
        return LF_SET_KIND;
    if (sd_text_check(s, text))
        return LF_SET_INVALID;
    if (s->store == SD_AMARCHY) {
        am_prefs a;
        amconf_load_with(io, &a);
        strcpy(sd_string(s, &a, 0), text);
        return amconf_save_with(io, &a, 1, failed);
    }
    return lf_set_upterm(io, s, -1, text, 0, 0, failed);
}

/* xterm's default 16, what an unset palette entry shows */
static const unsigned long lf_ansi_rgb[16] = {
    0x000000UL, 0xCD0000UL, 0x00CD00UL, 0xCDCD00UL, 0x0000EEUL, 0xCD00CDUL, 0x00CDCDUL, 0xE5E5E5UL,
    0x7F7F7FUL, 0xFF0000UL, 0x00FF00UL, 0xFFFF00UL, 0x5C5CFFUL, 0xFF00FFUL, 0x00FFFFUL, 0xFFFFFFUL
};

static void lf_setting_text_raw(const amconf_io *io, const sd_setting *s, char *out, int n);

void lf_palette_hex(const amconf_io *io, int n, char *out)
{
    prefs_fields f;
    out[0] = 0;
    if (n < 0 || n > 15)
        return;
    lf_load_upterm(io);
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    if (f.pal[n][0])
        snprintf(out, 8, "%s", f.pal[n]);
    else
        prefs_hex_format((uc_u32)lf_ansi_rgb[n], out);
}

void lf_colour_current(const amconf_io *io, const sd_setting *s, int entry, char *out)
{
    out[0] = 0;
    if (s->kind == SD_PALETTE)
        lf_palette_hex(io, entry, out);
    else if (s->kind == SD_COLOUR)
        lf_setting_text_raw(io, s, out, 8);
}

int lf_setting_set_colour(const amconf_io *io, const sd_setting *s, int entry, const char *text, int *failed)
{
    char hex[16];
    uc_u32 rgb;
    *failed = 0;
    if (!SD_HAS_PICKER(s) || s->store != SD_UPTERM)
        return LF_SET_KIND;
    if (s->kind == SD_PALETTE && (entry < 0 || entry > 15 || !text))
        return LF_SET_KIND;
    if (s->kind == SD_COLOUR && entry != SD_PICK_CUSTOM && (entry < 0 || entry > 15))
        return LF_SET_KIND;
    if (s->kind == SD_COLOUR && entry != SD_PICK_CUSTOM) {
        lf_palette_hex(io, entry, hex);
    } else if (!text) {
        return LF_SET_KIND;
    } else if (!*text) {
        hex[0] = 0;                     /* empty: back to the built-in */
    } else if (prefs_hex_parse(text, &rgb)) {
        prefs_hex_format(rgb, hex);
    } else
        return LF_SET_INVALID;
    return lf_set_upterm(io, s, -1, hex, s->kind == SD_PALETTE ? entry : 0, 0, failed);
}


/* ---- font and screen mode pickers (M6) ---- */

/* the current text of a string row (and its second key, the screen depth), from the file */
static void lf_current_str(const amconf_io *io, const sd_setting *s, char *out, int n, char *out2, int n2)
{
    const char *t, *t2 = 0;
    out[0] = 0;
    if (out2)
        out2[0] = 0;
    if (s->store == SD_AMARCHY) {
        am_prefs a;
        amconf_load_with(io, &a);
        t = sd_string(s, &a, 0);
        snprintf(out, (size_t)n, "%s", t ? t : "");
    } else {
        prefs_fields f;
        lf_load_upterm(io);
        prefs_from_conf(&f, &lf_conf, LF_PROFILE);
        t = sd_string(s, &f, 0);
        if (s->off2)
            t2 = (const char *)&f + s->off2;
        snprintf(out, (size_t)n, "%s", t ? t : "");
        if (out2 && t2)
            snprintf(out2, (size_t)n2, "%s", t2);
    }
}

int lf_setting_pick(const amconf_io *io, const sd_setting *s, const lf_asl *asl, int *failed)
{
    char cur[UC_MAX_VALUE], depth[UC_MAX_VALUE], spec[UC_MAX_VALUE];
    *failed = 0;
    if (s->kind == SD_FONT && s->id != SDI_FALLBACK) {
        char name[UC_MAX_VALUE], iname[UC_MAX_VALUE];
        long size = 0, isize = 0;
        lf_current_str(io, s, cur, sizeof cur, 0, 0);
        iname[0] = 0;
        prefs_font_parse(cur, iname, sizeof iname, &isize);
        if (!asl->font || !asl->font(asl->ctx, iname, isize, s->id == SDI_FONT, name, sizeof name, &size))
            return LF_SET_CANCELLED;
        if (!prefs_font_compose(name, size, spec, sizeof spec))
            return LF_SET_INVALID;      /* a colon in the name: not usable */
        if (s->store == SD_AMARCHY) {
            am_prefs a;
            amconf_load_with(io, &a);
            strcpy(sd_string(s, &a, 0), spec);
            return amconf_save_with(io, &a, 1, failed);
        }
        return lf_set_upterm(io, s, -1, spec, 0, 0, failed);
    }
    if (s->kind == SD_FONT) {           /* the fallback: an outline font file, stored as its base name */
        char path[300], name[UC_MAX_VALUE];
        if (!asl->otag || !asl->otag(asl->ctx, path, sizeof path))
            return LF_SET_CANCELLED;
        if (!prefs_otag_basename(path, name, sizeof name))
            return LF_SET_INVALID;      /* not an .otag file */
        return lf_set_upterm(io, s, -1, name, 0, 0, failed);
    }
    if (s->kind == SD_SCREENMODE) {
        unsigned long id = 0, nid = 0;
        int nd = 0;
        char dtext[UC_MAX_VALUE];
        lf_current_str(io, s, cur, sizeof cur, depth, sizeof depth);
        if (!prefs_mode_parse(cur, &id))
            id = 0;
        if (!asl->mode || !asl->mode(asl->ctx, id, prefs_depth_parse(depth), &nid, &nd))
            return LF_SET_CANCELLED;
        prefs_mode_format(nid, spec);
        prefs_depth_format(nd, dtext);
        return lf_set_upterm(io, s, -1, spec, 0, dtext, failed);
    }
    return LF_SET_KIND;
}

/* the current value index of row s, read from its file */
static int lf_current(const amconf_io *io, const sd_setting *s)
{
    if (s->store == SD_AMARCHY) {
        am_prefs a;
        amconf_load_with(io, &a);
        return sd_index(s, &a);
    } else {
        prefs_fields f;
        lf_load_upterm(io);
        prefs_from_conf(&f, &lf_conf, LF_PROFILE);
        return sd_index(s, &f);
    }
}

static void lf_setting_text_raw(const amconf_io *io, const sd_setting *s, char *out, int n)
{
    prefs_fields f;
    const char *t;
    lf_load_upterm(io);
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    t = sd_string(s, &f, 0);
    snprintf(out, (size_t)n, "%s", t ? t : "");
}

int lf_setting_text(const amconf_io *io, const sd_setting *s, char *out, int n)
{
    const char *t;
    if (n < 1)
        return 0;
    out[0] = 0;
    if (s->kind != SD_INT && s->kind != SD_STRING)
        return 0;
    if (s->store == SD_AMARCHY) {
        am_prefs a;
        amconf_load_with(io, &a);
        t = sd_string(s, &a, 0);
        if (t)
            snprintf(out, (size_t)n, "%s", t);
    } else {
        prefs_fields f;
        lf_load_upterm(io);
        prefs_from_conf(&f, &lf_conf, LF_PROFILE);
        t = sd_string(s, &f, 0);
        if (t)
            snprintf(out, (size_t)n, "%s", t);
    }
    return t != 0;
}

int lf_setting_step(const amconf_io *io, const sd_setting *s, int *failed)
{
    int next = lf_setting_next(s, lf_current(io, s));
    *failed = 0;
    if (next < 0)
        return LF_SET_KIND;
    return lf_setting_set(io, s, next, failed);
}

void lf_setting_labels(const amconf_io *io, mn_item *it, int n)
{
    prefs_fields f;
    am_prefs a;
    int i, have_u = 0, have_a = 0;
    for (i = 0; i < n; i++) {
        const sd_setting *s;
        const void *store;
        if (!MN_IS_SETTING(&it[i]) && !MN_IS_CHOICE(&it[i]))
            continue;
        s = &sd_settings[it[i].ix];
        if (s->store == SD_AMARCHY) {
            if (!have_a++)
                amconf_load_with(io, &a);
            store = &a;
        } else {
            if (!have_u++) {
                lf_load_upterm(io);
                prefs_from_conf(&f, &lf_conf, LF_PROFILE);
            }
            store = &f;
        }
        if (MN_IS_SETTING(&it[i]))
            sd_row_label(s, store, it[i].label, MN_LABEL);
        else if (SD_HAS_LIST(s)) {
            /* the names and actions come labelled from the tree */
        } else if (SD_HAS_PICKER(s)) {
            int v = atoi(it[i].seg);
            if (v == SD_PICK_CUSTOM)
                strcpy(it[i].label, "Custom...");
            else if (v >= 0 && v < 16) {
                char hex[16];
                lf_palette_hex(io, v, hex);
                snprintf(it[i].label, MN_LABEL, "%s: #%s", prefs_pal_name(v), hex);
            }
        } else {
            char txt[KM_LABEL];
            int v = atoi(it[i].seg);
            strncpy(txt, v >= 0 && v < s->nvalues ? s->values[v].text : "", KM_LABEL - 3);
            txt[KM_LABEL - 3] = 0;
            strcpy(it[i].label, v == sd_index(s, store) ? "* " : "  ");
            strcat(it[i].label, txt);
        }
    }
}

/* ---- M7: profiles and themes ---- */

/* The loaded table written back whole (ENVARC: then ENV:), like Prefs Save of an edited table. */
static int lf_write_conf(const amconf_io *io, int *failed)
{
    long len = upconf_save(&lf_conf, lf_buf, UC_MAX_FILE);
    if (len < 0)
        return LF_SET_FULL;
    lf_buf[len] = 0;
    return prefs_save_at(io->fs, io->dir, &prefs_paths_upterm, lf_buf, len, 1, failed);
}

int lf_profile_names(const amconf_io *io, char *names, int cap)
{
    int i, len = 0, n = 0;
    lf_load_upterm(io);
    for (i = 0; i < lf_conf.nprof; i++) {
        int l = (int)strlen(lf_conf.prof[i]);
        if (len + l + 1 > cap)
            break;
        memcpy(names + len, lf_conf.prof[i], (size_t)l + 1);
        len += l + 1;
        n++;
    }
    return n;
}

int lf_profile_use(const amconf_io *io, const char *name)
{
    int i;
    lf_load_upterm(io);
    for (i = 0; i < lf_conf.nprof; i++)
        if (!strcmp(lf_conf.prof[i], name)) {
            strcpy(lf_active, lf_conf.prof[i]);
            return LF_SET_OK;
        }
    return LF_SET_MISSING;
}

int lf_profile_new(const amconf_io *io, const char *name, int *failed)
{
    char clean[UC_NAME];
    prefs_fields f;
    int r;
    *failed = 0;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    prefs_clean_name(name, clean, sizeof clean);
    if (!clean[0])
        return LF_SET_INVALID;
    if (prefs_profile_exists(&lf_conf, clean))
        return LF_SET_EXISTS;
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    r = lf_put_fields(io, clean, &f, failed);
    if (r == LF_SET_OK)
        strcpy(lf_active, clean);
    return r;
}

int lf_profile_rename(const amconf_io *io, const char *from, const char *to, int *failed)
{
    char clean[UC_NAME];
    int r;
    *failed = 0;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    r = prefs_rename_profile(&lf_conf, from, to);
    if (r == PREFS_RENAME_MISSING)
        return LF_SET_MISSING;
    if (r == PREFS_RENAME_EMPTY)
        return LF_SET_INVALID;
    if (r == PREFS_RENAME_EXISTS)
        return LF_SET_EXISTS;
    prefs_clean_name(to, clean, sizeof clean);
    r = lf_write_conf(io, failed);
    if (r == LF_SET_OK && !strcmp(lf_active, from))
        strcpy(lf_active, clean);
    return r;
}

int lf_profile_delete(const amconf_io *io, const char *name, int *failed)
{
    int r;
    *failed = 0;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    if (!prefs_profile_exists(&lf_conf, name))
        return LF_SET_MISSING;
    if (lf_conf.nprof < 2)
        return LF_SET_LAST;
    upconf_rmprof(&lf_conf, name);
    r = lf_write_conf(io, failed);
    if (r == LF_SET_OK && !strcmp(lf_active, name))
        strcpy(lf_active, lf_conf.prof[0]);
    return r;
}

typedef struct lf_tlist {
    char *names;
    int len, cap, n;
} lf_tlist;

static void lf_tlist_add(void *ud, const char *file)
{
    lf_tlist *t = (lf_tlist *)ud;
    if (prefs_theme_add(t->names, &t->len, t->cap, file) > 0)
        t->n++;
}

int lf_theme_names(const amconf_io *io, char *names, int cap)
{
    lf_tlist t;
    t.names = names;
    t.len = 0;
    t.cap = cap;
    t.n = 0;
    if (io->list)
        io->list(PREFS_THEMES_DIR, lf_tlist_add, &t);
    return t.n;
}

/* the theme's file in the ENVARC: drawer (envarc 1) or the ENV: copy (0); 0 when the name is no theme name */
static int lf_theme_path(const char *name, int envarc, char *out, int cap)
{
    char clean[UC_NAME];
    prefs_clean_name(name, clean, sizeof clean);
    if (!clean[0] || strchr(clean, ':') || strchr(clean, '/'))
        return 0;
    prefs_theme_file(envarc ? PREFS_THEMES_DIR : PREFS_THEMES_ENV, clean, out, cap);
    return 1;
}

int lf_theme_apply(const amconf_io *io, const char *name, int *failed)
{
    char path[160];
    prefs_fields f;
    long got;
    *failed = 0;
    if (!lf_theme_path(name, 1, path, sizeof path))
        return LF_SET_INVALID;
    got = io->read(io->fs->ctx, path, lf_tbuf, UC_MAX_FILE + 1);
    if (got <= 0 || got > UC_MAX_FILE)
        return LF_SET_MISSING;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    if (!prefs_apply_theme(&f, &lf_work, lf_tbuf, got))
        return LF_SET_INVALID;
    if (prefs_validate(&f))
        return LF_SET_INVALID;
    return lf_put_fields(io, LF_PROFILE, &f, failed);
}

int lf_theme_new(const amconf_io *io, const char *name, int *failed)
{
    static const char *const keys[] = { "fg", "bg", "cursor-color", "selection-fg", "selection-bg", "palette" };
    char path[160], clean[UC_NAME];
    long len;
    int i, t;
    *failed = 0;
    if (!lf_theme_path(name, 1, path, sizeof path))
        return LF_SET_INVALID;
    if (io->fs->exists(io->fs->ctx, path))
        return LF_SET_EXISTS;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    prefs_clean_name(name, clean, sizeof clean);
    upconf_clear(&lf_work);
    for (i = 0; i < 6; i++) {
        const char *v = upconf_get(&lf_conf, LF_PROFILE, keys[i]);
        if (v)
            upconf_set(&lf_work, clean, keys[i], v);
    }
    len = upconf_save(&lf_work, lf_tbuf, UC_MAX_FILE);
    if (len < 0 || lf_work.overflow)
        return LF_SET_FULL;
    for (t = 1; t >= 0; t--) {
        lf_theme_path(name, t, path, sizeof path);
        if (!io->fs->write_file(io->fs->ctx, path, lf_tbuf, len) && t == 1) {
            *failed = PREFS_T_ENVARC;
            return PREFS_INSTALL_WRITE;
        }
    }
    return LF_SET_OK;
}

int lf_theme_rename(const amconf_io *io, const char *from, const char *to, int *failed)
{
    char a[160], b[160];
    int t;
    *failed = 0;
    if (!lf_theme_path(from, 1, a, sizeof a) || !lf_theme_path(to, 1, b, sizeof b))
        return LF_SET_INVALID;
    if (!io->fs->exists(io->fs->ctx, a))
        return LF_SET_MISSING;
    if (io->fs->exists(io->fs->ctx, b))
        return LF_SET_EXISTS;
    for (t = 1; t >= 0; t--) {
        lf_theme_path(from, t, a, sizeof a);
        lf_theme_path(to, t, b, sizeof b);
        if (!io->fs->exists(io->fs->ctx, a))
            continue;               /* the ENV: copy is made at boot: no copy, nothing to move */
        if (!io->fs->rename(io->fs->ctx, a, b) && t == 1) {
            *failed = PREFS_T_ENVARC;
            return PREFS_INSTALL_PLACE;
        }
    }
    return LF_SET_OK;
}

int lf_theme_delete(const amconf_io *io, const char *name, int *failed)
{
    char a[160];
    int t;
    *failed = 0;
    if (!lf_theme_path(name, 1, a, sizeof a))
        return LF_SET_INVALID;
    if (!io->fs->exists(io->fs->ctx, a))
        return LF_SET_MISSING;
    for (t = 1; t >= 0; t--) {
        lf_theme_path(name, t, a, sizeof a);
        if (!io->fs->remove(io->fs->ctx, a) && t == 1) {
            *failed = PREFS_T_ENVARC;
            return PREFS_INSTALL_WRITE;
        }
    }
    return LF_SET_OK;
}

/* The colour chip of each row of a level: out[i] is 0xRRGGBB for a swatch row with a colour set and for a picker
 * row of palette entry 0..15, else -1 (no chip: "Custom...", an unset swatch, every other row). */
long lf_row_chip(const amconf_io *io, const mn_item *it)
{
    char hex[16];
    uc_u32 rgb;
    const sd_setting *s;
    hex[0] = 0;
    if (MN_IS_SETTING(it)) {
        s = &sd_settings[it->ix];
        if (s->kind != SD_COLOUR)
            return -1;
        lf_colour_current(io, s, 0, hex);
    } else if (MN_IS_CHOICE(it) && SD_HAS_PICKER(&sd_settings[it->ix])) {
        int v = atoi(it->seg);
        if (v < 0 || v > 15)
            return -1;
        lf_palette_hex(io, v, hex);
    } else
        return -1;
    return prefs_hex_parse(hex, &rgb) ? (long)rgb : -1;
}

void lf_setting_chips(const amconf_io *io, const mn_item *it, int n, long *out)
{
    int i;
    for (i = 0; i < n; i++)
        out[i] = lf_row_chip(io, &it[i]);
}
