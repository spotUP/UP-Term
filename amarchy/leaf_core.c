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

/* index >= 0: a check or cycle value; else the text of an SD_INT or SD_STRING row */
static int lf_set_upterm(const amconf_io *io, const sd_setting *s, int index, const char *text, int n, int *failed)
{
    prefs_fields f;
    long len;
    int r, t;
    if (!prefs_load_writable(lf_load_upterm(io)))
        return LF_SET_READONLY;
    prefs_from_conf(&f, &lf_conf, LF_PROFILE);
    if (text)
        strcpy(sd_string(s, &f, n), text);
    else
        sd_set_index(s, &f, index);
    if (prefs_validate(&f))
        return LF_SET_INVALID;
    len = prefs_stage(&lf_work, &lf_conf, LF_PROFILE, &f, lf_buf, UC_MAX_FILE + 1);
    if (len == PREFS_STAGE_LOSSY)
        return LF_SET_READONLY;
    if (len < 0)
        return LF_SET_FULL;
    r = prefs_save_at(io->fs, io->dir, &prefs_paths_upterm, lf_buf, len, 1, failed);
    if (r != PREFS_INSTALL_OK)
        return r;
    for (t = PREFS_T_ENVARC; t >= PREFS_T_ENV; t--) {
        *failed = t;
        r = prefs_write_hlcat(io->fs, t, f.hlcat);
        if (r != PREFS_INSTALL_OK)
            return r;
    }
    return LF_SET_OK;
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
    return lf_set_upterm(io, s, index, 0, 0, failed);
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
    return lf_set_upterm(io, s, -1, text, 0, failed);
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
    return lf_set_upterm(io, s, -1, hex, s->kind == SD_PALETTE ? entry : 0, failed);
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
            sd_row_label(s, store, it[i].label, KM_LABEL);
        else if (SD_HAS_PICKER(s)) {
            int v = atoi(it[i].seg);
            if (v == SD_PICK_CUSTOM)
                strcpy(it[i].label, "Custom...");
            else if (v >= 0 && v < 16) {
                char hex[16];
                lf_palette_hex(io, v, hex);
                snprintf(it[i].label, KM_LABEL, "%s: #%s", prefs_pal_name(v), hex);
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
