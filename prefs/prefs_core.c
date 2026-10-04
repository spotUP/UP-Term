/* prefs_core: the Prefs editor's model (see prefs_core.h). No OS calls. */
#include <string.h>

#include "prefs_core.h"

static int pc_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* Copy s into a field of cap bytes, cut to fit (the fields are upconf's own
 * sizes, so a value from the table always fits). */
static void pc_copy(char *dst, const char *s, int cap)
{
    int i = 0;
    while (s[i] && i < cap - 1) {
        dst[i] = s[i];
        i++;
    }
    dst[i] = 0;
}

/* "RRGGBB" of a colour, for a palette field. */
static void pc_rgb_chars(uc_u32 rgb, char *out)
{
    int i;
    for (i = 5; i >= 0; i--) {
        out[i] = "0123456789ABCDEF"[rgb & 0xF];
        rgb >>= 4;
    }
    out[6] = 0;
}

void prefs_defaults(prefs_fields *f)
{
    memset(f, 0, sizeof(*f));
    f->cursor = PREFS_CURSOR_BLOCK;
    f->bell = PREFS_BELL_BEEP;
    f->bold = 1;
    f->wheel = 1;
    f->reflow = 1;
    f->kccache = 1;
}

void prefs_from_conf(prefs_fields *f, const upconf *c, const char *p)
{
    const char *v;
    uc_u32 pal[16];
    int i;
    prefs_defaults(f);
    pc_copy(f->font, upconf_str(c, p, "font", ""), sizeof(f->font));
    pc_copy(f->fallback, upconf_str(c, p, "font-fallback", ""), sizeof(f->fallback));
    pc_copy(f->screen, upconf_str(c, p, "screen", ""), sizeof(f->screen));
    pc_copy(f->screenmode, upconf_str(c, p, "screen-mode", ""), sizeof(f->screenmode));
    pc_copy(f->screendepth, upconf_str(c, p, "screen-depth", ""), sizeof(f->screendepth));
    pc_copy(f->sb, upconf_str(c, p, "scrollback", ""), sizeof(f->sb));
    pc_copy(f->curcol, upconf_str(c, p, "cursor-color", ""), sizeof(f->curcol));
    pc_copy(f->fg, upconf_str(c, p, "fg", ""), sizeof(f->fg));
    pc_copy(f->bg, upconf_str(c, p, "bg", ""), sizeof(f->bg));
    pc_copy(f->selfg, upconf_str(c, p, "selection-fg", ""), sizeof(f->selfg));
    pc_copy(f->selbg, upconf_str(c, p, "selection-bg", ""), sizeof(f->selbg));
    pc_copy(f->clipboard, upconf_str(c, p, "program-clipboard", ""), sizeof(f->clipboard));
    pc_copy(f->linkopen, upconf_str(c, p, "link-open", ""), sizeof(f->linkopen));
    v = upconf_str(c, p, "cursor", "block");
    f->cursor = pc_ieq(v, "underline") ? PREFS_CURSOR_UNDERLINE
              : pc_ieq(v, "bar") ? PREFS_CURSOR_BAR : PREFS_CURSOR_BLOCK;
    f->blink = pc_ieq(upconf_str(c, p, "cursor-blink", "off"), "on");
    v = upconf_str(c, p, "bell", "beep");
    f->bell = pc_ieq(v, "none") ? PREFS_BELL_NONE
            : pc_ieq(v, "visual") ? PREFS_BELL_VISUAL : PREFS_BELL_BEEP;
    f->bold = pc_ieq(upconf_str(c, p, "bold-bright", "on"), "on");
    f->meta_alt = pc_ieq(upconf_str(c, p, "meta", "amiga"), "alt");
    f->copy_sel = pc_ieq(upconf_str(c, p, "copy-on-select", "off"), "on");
    f->wheel = !pc_ieq(upconf_str(c, p, "wheel", "scroll"), "ignore");
    f->reflow = !pc_ieq(upconf_str(c, p, "reflow", "on"), "off");
    f->completion = pc_ieq(upconf_str(c, p, "completion", "unix"), "kingcon")
                  ? PREFS_COMPLETE_KINGCON : PREFS_COMPLETE_UNIX;
    pc_copy(f->kcmode, upconf_str(c, p, "kingcon-mode", ""), sizeof(f->kcmode));
    f->kcinfo = pc_ieq(upconf_str(c, p, "kingcon-info", "hide"), "show");
    f->kccache = !pc_ieq(upconf_str(c, p, "kingcon-cache", "on"), "off");
    upconf_palette_parse(upconf_get(c, p, "palette"), pal);
    for (i = 0; i < 16; i++)
        if (pal[i] & 0x01000000UL)
            pc_rgb_chars(pal[i] & 0xFFFFFFUL, f->pal[i]);
}

int prefs_apply_theme(prefs_fields *f, upconf *work, const char *text, long len)
{
    const char *p, *keys[5];
    uc_u32 pal[16];
    int i, any = 0;
    upconf_parse(work, text, len);
    if (work->nprof < 1)
        return 0;
    p = work->prof[0]; /* a theme file is one section; its name is the theme's */
    keys[0] = "fg";
    keys[1] = "bg";
    keys[2] = "cursor-color";
    keys[3] = "selection-fg";
    keys[4] = "selection-bg";
    for (i = 0; i < 5; i++)
        any |= upconf_get(work, p, keys[i]) != 0;
    any |= upconf_get(work, p, "palette") != 0;
    if (!any)
        return 0;
    pc_copy(f->fg, upconf_str(work, p, "fg", ""), sizeof(f->fg));
    pc_copy(f->bg, upconf_str(work, p, "bg", ""), sizeof(f->bg));
    pc_copy(f->curcol, upconf_str(work, p, "cursor-color", ""), sizeof(f->curcol));
    pc_copy(f->selfg, upconf_str(work, p, "selection-fg", ""), sizeof(f->selfg));
    pc_copy(f->selbg, upconf_str(work, p, "selection-bg", ""), sizeof(f->selbg));
    upconf_palette_parse(upconf_get(work, p, "palette"), pal);
    for (i = 0; i < 16; i++) {
        f->pal[i][0] = 0;
        if (pal[i] & 0x01000000UL)
            pc_rgb_chars(pal[i] & 0xFFFFFFUL, f->pal[i]);
    }
    return 1;
}

int prefs_validate(const prefs_fields *f)
{
    uc_u32 rgb;
    int i;
    for (i = 0; i < 16; i++)
        if (f->pal[i][0] && !upconf_hex6(f->pal[i], &rgb))
            return PREFS_BAD_PAL + i;
    /* strictly six digits: not something the lenient file parser would cut */
    if (f->selfg[0] && !upconf_hex6(f->selfg, &rgb))
        return PREFS_BAD_SELFG;
    if (f->selbg[0] && !upconf_hex6(f->selbg, &rgb))
        return PREFS_BAD_SELBG;
    return 0;
}

int prefs_profile_exists(const upconf *c, const char *profile)
{
    int i;
    for (i = 0; i < c->nprof; i++)
        if (pc_ieq(c->prof[i], profile))
            return 1;
    return 0;
}

void prefs_clean_name(const char *p, char *out, int cap)
{
    char *o = out;
    while (*p == ' ' || *p == '\t')
        p++;
    while (*p) {
        if (*p != '[' && *p != ']' && (int)(o - out) < cap - 1)
            *o++ = *p;
        p++;
    }
    while (o > out && (o[-1] == ' ' || o[-1] == '\t'))
        o--;
    *o = 0;
}

int prefs_load(upconf *c, const char *buf, long got, long cap)
{
    upconf_clear(c);
    if (got < 0)
        return PREFS_LOAD_ERROR;
    if (got > cap)
        return PREFS_LOAD_TOOBIG;
    if (got == 0 || !buf)
        return PREFS_LOAD_NONE;
    upconf_parse(c, buf, got);
    if (c->overflow) {
        /* what was dropped would be lost on the next save */
        upconf_clear(c);
        return PREFS_LOAD_LOSSY;
    }
    return PREFS_LOAD_OK;
}

int prefs_load_writable(int r)
{
    return r == PREFS_LOAD_OK || r == PREFS_LOAD_NONE;
}

/* One of the editor's keys, in place: a value replaces the key's (a new
 * key goes after the profile's last), an empty one deletes it so the
 * handler's built-in stands. */
static void pc_put(upconf *w, const char *p, const char *key, const char *v)
{
    if (v[0])
        upconf_set(w, p, key, v);
    else
        upconf_del(w, p, key);
}

long prefs_stage(upconf *w, const upconf *cur, const char *p,
                 const prefs_fields *f, char *buf, long cap)
{
    uc_u32 pal[16];
    char palstr[UC_MAX_VALUE];
    long len;
    int i;
    memcpy(w, cur, sizeof(*w));
    w->overflow = 0;
    /* Only the editor's own keys change, each where it stands: whatever
     * else the profile holds -- keys only the handler reads, keys typed by
     * hand, comments -- is written back as it was read. (The profile was
     * removed and refilled here, and all of that went with every save.) */
    pc_put(w, p, "font", f->font);
    pc_put(w, p, "font-fallback", f->fallback);
    pc_put(w, p, "screen", f->screen);
    pc_put(w, p, "screen-mode", f->screenmode);
    pc_put(w, p, "screen-depth", f->screendepth);
    pc_put(w, p, "scrollback", f->sb);
    pc_put(w, p, "cursor-color", f->curcol);
    pc_put(w, p, "fg", f->fg);
    pc_put(w, p, "bg", f->bg);
    /* blank keeps the swap for that half: the key is not written at all */
    pc_put(w, p, "selection-fg", f->selfg);
    pc_put(w, p, "selection-bg", f->selbg);
    pc_put(w, p, "program-clipboard", f->clipboard);
    pc_put(w, p, "link-open", f->linkopen);
    upconf_set(w, p, "cursor", f->cursor == PREFS_CURSOR_UNDERLINE ? "underline"
                               : f->cursor == PREFS_CURSOR_BAR ? "bar" : "block");
    upconf_set(w, p, "cursor-blink", f->blink ? "on" : "off");
    upconf_set(w, p, "bell", f->bell == PREFS_BELL_NONE ? "none"
                             : f->bell == PREFS_BELL_VISUAL ? "visual" : "beep");
    upconf_set(w, p, "bold-bright", f->bold ? "on" : "off");
    upconf_set(w, p, "meta", f->meta_alt ? "alt" : "amiga");
    upconf_set(w, p, "copy-on-select", f->copy_sel ? "on" : "off");
    upconf_set(w, p, "wheel", f->wheel ? "scroll" : "ignore");
    upconf_set(w, p, "reflow", f->reflow ? "on" : "off");
    upconf_set(w, p, "completion", f->completion == PREFS_COMPLETE_KINGCON ? "kingcon" : "unix");
    pc_put(w, p, "kingcon-mode", f->kcmode);
    upconf_set(w, p, "kingcon-info", f->kcinfo ? "show" : "hide");
    upconf_set(w, p, "kingcon-cache", f->kccache ? "on" : "off");
    for (i = 0; i < 16; i++) {
        uc_u32 rgb;
        pal[i] = 0;
        if (f->pal[i][0] && upconf_hex6(f->pal[i], &rgb))
            pal[i] = 0x01000000UL | rgb;
    }
    /* a full 16-entry palette is 159 bytes: it always fits the slot */
    len = upconf_palette_str(pal, palstr, sizeof(palstr) - 1);
    if (len < 0)
        return PREFS_STAGE_FULL;
    palstr[len] = 0;
    pc_put(w, p, "palette", palstr);
    if (w->overflow)
        return PREFS_STAGE_FULL; /* a key or the profile itself had no room */
    len = upconf_save(w, buf, cap - 1);
    if (len < 0)
        return PREFS_STAGE_SIZE;
    buf[len] = 0;
    return len;
}

int prefs_install(const prefs_fs *fs, const char *path, const char *tmp,
                  const char *orig, const char *buf, long len)
{
    int had_old;
    fs->remove(fs->ctx, tmp); /* a leftover from an interrupted save */
    if (!fs->write_file(fs->ctx, tmp, buf, len)) {
        fs->remove(fs->ctx, tmp);
        return PREFS_INSTALL_WRITE;
    }
    had_old = fs->exists(fs->ctx, path);
    if (had_old) {
        /* the old file is moved aside, never deleted: until the new one is
         * in place, one of the two is always at path or at orig */
        if (!fs->remove(fs->ctx, orig) || !fs->rename(fs->ctx, path, orig)) {
            fs->remove(fs->ctx, tmp);
            return PREFS_INSTALL_BACKUP;
        }
    }
    if (!fs->rename(fs->ctx, tmp, path)) {
        if (had_old && !fs->rename(fs->ctx, orig, path))
            return PREFS_INSTALL_STRANDED; /* keep tmp: it is the new file */
        fs->remove(fs->ctx, tmp);
        return PREFS_INSTALL_PLACE;
    }
    return PREFS_INSTALL_OK;
}
