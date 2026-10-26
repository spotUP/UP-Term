/* prefs_core: the Prefs editor's model (see prefs_core.h). No OS calls. */
#include <string.h>

#include "prefs_core.h"
#include "settings_def.h"
#include "../amarchy/amconf_core.h"
#include "../handler/kcmode.h"
#include "../handler/slash.h"

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
    f->scrollbar = 1;
    f->kccache = 1;
    f->aspect = 1;
    f->amiga_keys = 1;
    f->meta_alt = 1;
    f->dither = 1;
    f->outline_aa = 1;
    f->hlcat = 1;
}

/* The palette key's entries into the 16 hex fields; an entry the key does not name stays empty. */
static void pc_palette_from_conf(prefs_fields *f, const upconf *c, const char *p, const sd_setting *s)
{
    uc_u32 pal[16];
    int i;
    upconf_palette_parse(upconf_get(c, p, s->key), pal);
    for (i = 0; i < 16; i++)
        if (pal[i] & 0x01000000UL)
            pc_rgb_chars(pal[i] & 0xFFFFFFUL, f->pal[i]);
}

/* One row of the table into prefs_fields: a check or cycle is the index of its file text in values[] (a text
 * no value names takes the default's), a text key is copied. The KingCON rows share one key and one string. */
void prefs_from_conf(prefs_fields *f, const upconf *c, const char *p)
{
    int i;
    prefs_defaults(f);
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        char *at = (char *)f + s->off;
        if (s->store != SD_UPTERM || !s->key || s->ftype == SDF_KC_COMMON || s->ftype == SDF_KC_SILENT)
            continue;
        if (s->kind == SD_PALETTE) {
            pc_palette_from_conf(f, c, p, s);
        } else if (s->ftype == SDF_INT) {
            *(int *)at = sd_conf_index(s, c, p);
        } else {
            pc_copy(at, upconf_str(c, p, s->key, ""), s->size);
            if (s->key2)
                pc_copy((char *)f + s->off2, upconf_str(c, p, s->key2, ""), s->size);
        }
    }
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

static const char *const pc_pal_names[16] = {
    "Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White",
    "Bright Black", "Bright Red", "Bright Green", "Bright Yellow",
    "Bright Blue", "Bright Magenta", "Bright Cyan", "Bright White"
};

const char *prefs_pal_name(int i)
{
    return i >= 0 && i < 16 ? pc_pal_names[i] : "";
}

static int pc_hexval(char h)
{
    if (h >= '0' && h <= '9')
        return h - '0';
    if (h >= 'a' && h <= 'f')
        return h - 'a' + 10;
    if (h >= 'A' && h <= 'F')
        return h - 'A' + 10;
    return -1;
}

int prefs_hex_parse(const char *s, uc_u32 *rgb)
{
    uc_u32 r = 0;
    int i, v;
    if (!s || !rgb)
        return 0;
    if (*s == '#')
        s++;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    for (i = 0; i < 6; i++) {
        v = pc_hexval(s[i]);
        if (v < 0)
            return 0;
        r = (r << 4) | (uc_u32)v;
    }
    if (s[6])
        return 0;
    *rgb = r;
    return 1;
}

void prefs_hex_format(uc_u32 rgb, char *out)
{
    pc_rgb_chars(rgb, out);
}

static int pc_colour_ok(const char *s)
{
    uc_u32 rgb;
    return !s[0] || prefs_hex_parse(s, &rgb);
}

int prefs_validate(const prefs_fields *f)
{
    int i;
    for (i = 0; i < 16; i++)
        if (!pc_colour_ok(f->pal[i]))
            return PREFS_BAD_PAL + i;
    if (!pc_colour_ok(f->selfg))
        return PREFS_BAD_SELFG;
    if (!pc_colour_ok(f->selbg))
        return PREFS_BAD_SELBG;
    if (!pc_colour_ok(f->fg))
        return PREFS_BAD_FG;
    if (!pc_colour_ok(f->bg))
        return PREFS_BAD_BG;
    if (!pc_colour_ok(f->curcol) && !pc_ieq(f->curcol, "inverse"))
        return PREFS_BAD_CURCOL;
    if (f->term[0] && !slash_term_ok(f->term))
        return PREFS_BAD_TERM;
    return 0;
}

static const char *const pc_colors[PREFS_COLORS_N] = { "", "rgb", "256", "16" };

int prefs_colors_index(const char *colors)
{
    int i;
    for (i = 1; i < PREFS_COLORS_N; i++)
        if (!strcmp(colors, pc_colors[i]))
            return i;
    return 0;
}

const char *prefs_colors_value(int index)
{
    return index > 0 && index < PREFS_COLORS_N ? pc_colors[index] : "";
}

static void pc_cat(char *out, int cap, const char *s)
{
    int n = (int)strlen(out);
    while (*s && n < cap - 1)
        out[n++] = *s++;
    out[n] = 0;
}

void prefs_bad_label(int bad, char *out, int cap)
{
    if (cap < 1)
        return;
    out[0] = 0;
    if (bad >= PREFS_BAD_PAL && bad < PREFS_BAD_PAL + 16) {
        pc_cat(out, cap, "Palette entry ");
        pc_cat(out, cap, prefs_pal_name(bad - PREFS_BAD_PAL));
    } else if (bad == PREFS_BAD_SELFG)
        pc_cat(out, cap, "Selected text");
    else if (bad == PREFS_BAD_SELBG)
        pc_cat(out, cap, "Selection");
    else if (bad == PREFS_BAD_FG)
        pc_cat(out, cap, "Text colour");
    else if (bad == PREFS_BAD_BG)
        pc_cat(out, cap, "Background");
    else if (bad == PREFS_BAD_CURCOL)
        pc_cat(out, cap, "Cursor colour");
    else if (bad == PREFS_BAD_TERM)
        pc_cat(out, cap, "Terminal type");
}


/* The wheel's conversions in 16-bit integer steps (65535 = 1.0): exact enough
 * that every 8-bit channel value comes back as it went in. */
void prefs_rgb_to_hsb(uc_u32 rgb, prefs_hsb *out)
{
    long r = (long)((rgb >> 16) & 0xFF), g = (long)((rgb >> 8) & 0xFF), b = (long)(rgb & 0xFF);
    long max = r, min = r, d, h = 0, s = 0, v;
    if (g > max) max = g;
    if (b > max) max = b;
    if (g < min) min = g;
    if (b < min) min = b;
    d = max - min;
    v = max * 257L;
    if (max)
        s = (d * 65535L + max / 2) / max;
    if (d) {
        long num;
        if (max == r) {
            num = g - b;           /* -d .. d, in sixths of the circle */
            h = num >= 0 ? (num * 10922L + d / 2) / d : 65535L - ((-num) * 10922L + d / 2) / d;
        } else if (max == g) {
            num = b - r;
            h = 21845L + (num >= 0 ? (num * 10922L + d / 2) / d : -((-num) * 10922L + d / 2) / d);
        } else {
            num = r - g;
            h = 43690L + (num >= 0 ? (num * 10922L + d / 2) / d : -((-num) * 10922L + d / 2) / d);
        }
        if (h < 0) h += 65535L;
        if (h > 65535L) h -= 65535L;
    }
    out->hue = (uc_u32)h * 65537UL;
    out->sat = (uc_u32)s * 65537UL;
    out->bri = (uc_u32)v * 65537UL;
}

static long pc_to8(long v16)
{
    long v = (v16 + 128L) / 257L;
    return v > 255 ? 255 : v < 0 ? 0 : v;
}

uc_u32 prefs_hsb_to_rgb(const prefs_hsb *hsb)
{
    long h = (long)(hsb->hue >> 16), s = (long)(hsb->sat >> 16), v = (long)(hsb->bri >> 16);
    long sector, f, p, q, t, r, g, b;
    /* the circle in six sectors of 10922.5 steps */
    sector = (h * 6L) / 65535L;
    if (sector > 5) sector = 5;
    f = h * 6L - sector * 65535L;               /* 0 .. 65535 in the sector */
    p = (v * (65535L - s) + 32767L) / 65535L;
    q = (v * (65535L - (s * f + 32767L) / 65535L) + 32767L) / 65535L;
    t = (v * (65535L - (s * (65535L - f) + 32767L) / 65535L) + 32767L) / 65535L;
    switch (sector) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return ((uc_u32)pc_to8(r) << 16) | ((uc_u32)pc_to8(g) << 8) | (uc_u32)pc_to8(b);
}

void prefs_kc_decode(const char *letters, prefs_kc *k)
{
    int m = le_kc_fncmode(letters);
    if (m & LE_KC_WINDOW)
        k->match = PREFS_KC_WINDOW;
    else if ((m & LE_KC_LIST) && (m & LE_KC_CYCLE))
        k->match = PREFS_KC_LISTCYCLE;
    else if (m & LE_KC_CYCLE)
        k->match = PREFS_KC_CYCLE;
    else if (m & LE_KC_LIST)
        k->match = PREFS_KC_LIST;
    else
        k->match = PREFS_KC_COMMON_ONLY;
    k->common = (m & LE_KC_COMMON) != 0;
    k->silent = (m & LE_KC_SILENT) != 0;
}

void prefs_kc_encode(const prefs_kc *k, char *out, int cap)
{
    char t[8];
    int n = 0;
    switch (k->match) {
    case PREFS_KC_LIST:
        t[n++] = 'L';
        break;
    case PREFS_KC_CYCLE:
        t[n++] = 'B';
        break;
    case PREFS_KC_LISTCYCLE:
        t[n++] = 'L';
        t[n++] = 'B';
        break;
    case PREFS_KC_COMMON_ONLY:
        t[n++] = 'C';
        break;
    default:
        if (k->common || k->silent)
            t[n++] = 'W'; /* the default alone is no key at all */
        break;
    }
    if (k->common && k->match != PREFS_KC_COMMON_ONLY)
        t[n++] = 'C';
    if (k->silent)
        t[n++] = 'S';
    t[n] = 0;
    if (cap < 1)
        return;
    pc_copy(out, t, cap);
}

int prefs_font_compose(const char *name, long size, char *out, int cap)
{
    int n = (int)strlen(name), i, j = 0;
    char num[12];
    int nd = 0;
    out[0] = 0;
    if (n >= 5 && pc_ieq(name + n - 5, ".font"))
        n -= 5;
    if (n < 1)
        return 0;
    for (i = 0; i < n; i++)
        if (name[i] == ':')
            return 0;
    if (n >= cap)
        return 0;
    for (i = 0; i < n; i++)
        out[j++] = name[i];
    if (size > 0) {
        if (size > 999)
            size = 999;
        while (size) {
            num[nd++] = (char)('0' + size % 10);
            size /= 10;
        }
        if (j + 1 + nd >= cap) {
            out[0] = 0;
            return 0;
        }
        out[j++] = ':';
        while (nd)
            out[j++] = num[--nd];
    }
    out[j] = 0;
    return 1;
}

int prefs_font_parse(const char *spec, char *name, int cap, long *size)
{
    int n = 0, i;
    long sz = 0;
    const char *colon = 0;
    name[0] = 0;
    *size = 0;
    for (i = 0; spec[i]; i++)
        if (spec[i] == ':')
            colon = spec + i;
    n = colon ? (int)(colon - spec) : (int)strlen(spec);
    if (n < 1)
        return 0;
    if (colon) {
        const char *d = colon + 1;
        while (*d >= '0' && *d <= '9' && sz < 10000)
            sz = sz * 10 + (*d++ - '0');
    }
    if (n >= cap)
        return 0;
    for (i = 0; i < n; i++)
        name[i] = spec[i];
    name[n] = 0;
    if (!(n >= 5 && pc_ieq(name + n - 5, ".font"))) {
        if (n + 5 >= cap) {
            name[0] = 0;
            return 0;
        }
        strcpy(name + n, ".font");
    }
    *size = sz;
    return 1;
}

int prefs_otag_basename(const char *path, char *out, int cap)
{
    const char *base = path, *p;
    int n;
    out[0] = 0;
    for (p = path; *p; p++)
        if (*p == ':' || *p == '/')
            base = p + 1;
    n = (int)strlen(base);
    if (n < 6 || !pc_ieq(base + n - 5, ".otag") || n - 5 >= cap)
        return 0;
    memcpy(out, base, (size_t)(n - 5));
    out[n - 5] = 0;
    return 1;
}

void prefs_sb_parse(const char *s, long *lines, int *none)
{
    long v = 0;
    int neg = 0, any = 0;
    *lines = PREFS_SB_DEFAULT;
    *none = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        any = 1;
        if (v < 1000000L)
            v = v * 10 + (*s - '0');
        s++;
    }
    if (!any)
        return;
    if (neg || v == 0) {
        *none = 1;
        return;
    }
    *lines = v > PREFS_SB_MAX ? PREFS_SB_MAX : v;
}

void prefs_sb_format(long lines, int none, char *out)
{
    char num[12];
    int n = 0, j = 0;
    if (none) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    if (lines > PREFS_SB_MAX)
        lines = PREFS_SB_MAX;
    if (lines < 1 || lines == PREFS_SB_DEFAULT) {
        out[0] = 0;
        return;
    }
    while (lines) {
        num[n++] = (char)('0' + lines % 10);
        lines /= 10;
    }
    while (n)
        out[j++] = num[--n];
    out[j] = 0;
}

int prefs_mode_parse(const char *s, unsigned long *id)
{
    unsigned long m = 0;
    int n = 0, v;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    for (; *s; s++, n++) {
        v = pc_hexval(*s);
        if (v < 0 || n >= 8)
            return 0;
        m = (m << 4) | (unsigned long)v;
    }
    if (n < 1)
        return 0;
    *id = m;
    return 1;
}

void prefs_mode_format(unsigned long id, char *out)
{
    int i;
    if (!id) {
        out[0] = 0;
        return;
    }
    out[0] = '0';
    out[1] = 'x';
    for (i = 9; i >= 2; i--) {
        out[i] = "0123456789abcdef"[id & 0xF];
        id >>= 4;
    }
    out[10] = 0;
}

int prefs_depth_parse(const char *s)
{
    long v = 0;
    int any = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        if (v < 1000)
            v = v * 10 + (*s - '0');
        s++;
    }
    if (!any || v < 1)
        return 0;
    return v < PREFS_DEPTH_MIN ? PREFS_DEPTH_MIN : v > PREFS_DEPTH_MAX ? PREFS_DEPTH_MAX : (int)v;
}

void prefs_depth_format(int depth, char *out)
{
    if (depth < 1) {
        out[0] = 0;
        return;
    }
    out[0] = (char)('0' + (depth < PREFS_DEPTH_MIN ? PREFS_DEPTH_MIN : depth > PREFS_DEPTH_MAX ? PREFS_DEPTH_MAX : depth));
    out[1] = 0;
}

int prefs_equal(const prefs_fields *a, const prefs_fields *b)
{
    int i;
    if (strcmp(a->font, b->font) || strcmp(a->fallback, b->fallback) ||
        strcmp(a->screenmode, b->screenmode) || strcmp(a->screendepth, b->screendepth) ||
        strcmp(a->sb, b->sb) || strcmp(a->curcol, b->curcol) || strcmp(a->fg, b->fg) ||
        strcmp(a->bg, b->bg) || strcmp(a->selfg, b->selfg) || strcmp(a->selbg, b->selbg) ||
        strcmp(a->term, b->term) || strcmp(a->colors, b->colors) ||
        strcmp(a->am_music, b->am_music) || strcmp(a->am_demo, b->am_demo) ||
        strcmp(a->am_hide, b->am_hide) || strcmp(a->am_font, b->am_font) || strcmp(a->am_gtheme, b->am_gtheme) || a->am_icons != b->am_icons ||
        a->am_mode != b->am_mode || a->am_termmenu != b->am_termmenu ||
        strcmp(a->linkopen, b->linkopen) || strcmp(a->kcmode, b->kcmode))
        return 0;
    for (i = 0; i < 16; i++)
        if (strcmp(a->pal[i], b->pal[i]))
            return 0;
    return a->cursor == b->cursor && a->bell == b->bell && a->blink == b->blink &&
           a->bold == b->bold && a->meta_alt == b->meta_alt && a->copy_sel == b->copy_sel &&
           a->wheel == b->wheel && a->reflow == b->reflow && a->scrollbar == b->scrollbar &&
           a->completion == b->completion && a->screen == b->screen && a->aspect == b->aspect &&
           a->backspace_bs == b->backspace_bs && a->clipboard == b->clipboard &&
           a->kcinfo == b->kcinfo && a->kccache == b->kccache && a->amiga_keys == b->amiga_keys &&
           a->dither == b->dither && a->outline_aa == b->outline_aa && a->pad_extend == b->pad_extend && a->hlcat == b->hlcat;
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

int prefs_rename_profile(upconf *c, const char *from, const char *to)
{
    char name[UC_NAME];
    int i, p = -1;
    for (i = 0; i < c->nprof; i++)
        if (pc_ieq(c->prof[i], from))
            p = i;
    if (p < 0)
        return PREFS_RENAME_MISSING;
    prefs_clean_name(to, name, sizeof(name));
    if (!name[0])
        return PREFS_RENAME_EMPTY;
    for (i = 0; i < c->nprof; i++)
        if (i != p && pc_ieq(c->prof[i], name))
            return PREFS_RENAME_EXISTS;
    pc_copy(c->prof[p], name, UC_NAME);
    return PREFS_RENAME_OK;
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

/* One row of prefs_fields into the profile, in place. A check or cycle is always written (the file text of
 * its value); a text key is written, or deleted when empty so the handler's built-in stands. -1: the palette
 * did not fit its slot. */
static int pc_stage_row(upconf *w, const char *p, const prefs_fields *f, const sd_setting *s)
{
    const char *at = (const char *)f + s->off;
    uc_u32 pal[16];
    char palstr[UC_MAX_VALUE];
    long len;
    int i;
    if (s->kind == SD_PALETTE) {
        for (i = 0; i < 16; i++) {
            uc_u32 rgb;
            pal[i] = 0;
            if (f->pal[i][0] && upconf_hex6(f->pal[i], &rgb))
                pal[i] = 0x01000000UL | rgb;
        }
        /* a full 16-entry palette is 159 bytes: it always fits the slot */
        len = upconf_palette_str(pal, palstr, sizeof(palstr) - 1);
        if (len < 0)
            return -1;
        palstr[len] = 0;
        pc_put(w, p, s->key, palstr);
    } else if (s->ftype == SDF_INT) {
        i = s->kind == SD_CHECK ? *(const int *)at != 0 : sd_index(s, f);
        upconf_set(w, p, s->key, s->values[i].file);
    } else {
        pc_put(w, p, s->key, at);
        if (s->key2)
            pc_put(w, p, s->key2, (const char *)f + s->off2);
    }
    return 0;
}

/* The rows from..to of sd_write_order. */
static int pc_stage_rows(upconf *w, const char *p, const prefs_fields *f, int from, int to)
{
    int i;
    for (i = from; i < to; i++)
        if (pc_stage_row(w, p, f, &sd_settings[sd_write_order[i]]) < 0)
            return -1;
    return 0;
}

/* The look's keys -- font, the colours, the palette -- set from f in place,
 * the table written into buf: the tail of both stagings. */
static long pc_stage_style(upconf *w, const char *p, const prefs_fields *f, char *buf, long cap)
{
    long len;
    /* blank selection colours keep the swap for that half: the key is not written at all */
    if (pc_stage_rows(w, p, f, sd_nwrite - sd_nstyle, sd_nwrite) < 0)
        return PREFS_STAGE_FULL;
    if (w->overflow)
        return PREFS_STAGE_FULL; /* a key or the profile itself had no room */
    len = upconf_save(w, buf, cap - 1);
    if (len < 0)
        return PREFS_STAGE_SIZE;
    buf[len] = 0;
    return len;
}

long prefs_stage_style(upconf *w, const upconf *cur, const char *p,
                       const prefs_fields *f, char *buf, long cap)
{
    if (cur->overflow)
        return PREFS_STAGE_LOSSY;
    memcpy(w, cur, sizeof(*w));
    w->overflow = 0;
    return pc_stage_style(w, p, f, buf, cap);
}

long prefs_stage(upconf *w, const upconf *cur, const char *p,
                 const prefs_fields *f, char *buf, long cap)
{
    if (cur->overflow)
        return PREFS_STAGE_LOSSY; /* what did not fit would be gone from the file */
    memcpy(w, cur, sizeof(*w));
    w->overflow = 0;
    /* Only the editor's own keys change, each where it stands: whatever
     * else the profile holds -- keys only the handler reads, keys typed by
     * hand, comments -- is written back as it was read. (The profile was
     * removed and refilled here, and all of that went with every save.) */
    /* The Amarchy keys have one store, ENVARC:Amarchy/prefs (amconf); this file no longer carries them, and a
     * save drops what an older version left here, so a stale copy cannot shadow or mislead (plan M3). */
    amconf_strip_legacy(w);
    pc_stage_rows(w, p, f, 0, sd_nwrite - sd_nstyle);
    return pc_stage_style(w, p, f, buf, cap);
}

const char *prefs_hlcat_text(int on)
{
    return on ? "on\n" : "off\n";
}

int prefs_write_hlcat(const prefs_fs *fs, int t, int on)
{
    char path[48];
    const char *v = prefs_hlcat_text(on);
    strcpy(path, prefs_conf_dir[t]);
    strcat(path, "/" PREFS_HLCAT_VAR);
    return fs->write_file(fs->ctx, path, v, (long)strlen(v)) ? PREFS_INSTALL_OK : PREFS_INSTALL_WRITE;
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

const prefs_paths prefs_paths_upterm = {
    { "ENV:up-term", "ENVARC:up-term" },
    { "ENV:up-term/up-term", "ENVARC:up-term/up-term" },
    "ENVARC:up-term/up-term.new",
    "ENVARC:up-term/up-term.orig"
};

const prefs_paths prefs_paths_amarchy = {
    { "ENV:Amarchy", "ENVARC:Amarchy" },
    { "ENV:Amarchy/prefs", "ENVARC:Amarchy/prefs" },
    "ENVARC:Amarchy/prefs.new",
    "ENVARC:Amarchy/prefs.orig"
};

int prefs_install_target_at(const prefs_fs *fs, const prefs_paths *p, int t, const char *buf, long len)
{
    if (t == PREFS_T_ENV)
        return fs->write_file(fs->ctx, p->path[t], buf, len) ? PREFS_INSTALL_OK
                                                              : PREFS_INSTALL_WRITE;
    return prefs_install(fs, p->path[t], p->tmp, p->orig, buf, len);
}

int prefs_save_at(const prefs_fs *fs, int (*dir)(const prefs_paths *, int), const prefs_paths *p,
                  const char *buf, long len, int keep, int *failed)
{
    int t, r;
    for (t = keep ? PREFS_T_ENVARC : PREFS_T_ENV; t >= PREFS_T_ENV; t--) {
        *failed = t;
        if (!dir(p, t))
            return PREFS_SAVE_NODIR;
        r = prefs_install_target_at(fs, p, t, buf, len);
        if (r != PREFS_INSTALL_OK)
            return r;
    }
    return PREFS_INSTALL_OK;
}

int prefs_install_target(const prefs_fs *fs, int t, const char *buf, long len)
{
    return prefs_install_target_at(fs, &prefs_paths_upterm, t, buf, len);
}

/* ---- W30: the themes drawer ------------------------------------------------ */

static int pc_lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* the first n bytes of a against all of b, ignoring ASCII case: <0, 0, >0 */
static int pc_ncmp_i(const char *a, int n, const char *b)
{
    int i;
    for (i = 0; i < n && b[i]; i++) {
        int x = pc_lower((unsigned char)a[i]), y = pc_lower((unsigned char)b[i]);
        if (x != y)
            return x - y;
    }
    if (i < n)
        return 1; /* b ended first */
    return b[i] ? -1 : 0;
}

/* the length of s without its ".conf" ending (any case); -1 when it has none */
static int pc_conf_stem(const char *s)
{
    int l = (int)strlen(s);
    return l >= 5 && pc_ieq(s + l - 5, ".conf") ? l - 5 : -1;
}

/* dir and file joined as AmigaDOS AddPart does ("VTC:" + "x" = "VTC:x",
 * "Work:a" + "x" = "Work:a/x", "" + "x" = "x"); 0 when it does not fit */
static int pc_join(char *out, int cap, const char *dir, const char *file)
{
    int d = (int)strlen(dir), f = (int)strlen(file);
    int slash = d && dir[d - 1] != ':' && dir[d - 1] != '/';
    if (d + slash + f + 1 > cap)
        return 0;
    memmove(out, dir, (size_t)d);
    if (slash)
        out[d++] = '/';
    memcpy(out + d, file, (size_t)f + 1);
    return 1;
}

/* the drawer part of path, as AmigaDOS PathPart cuts it (before the last
 * '/', or up to and with the ':'); 0 when path names no drawer or it does
 * not fit */
static int pc_drawer_of(const char *path, char *out, int cap)
{
    int i, end = -1;
    for (i = 0; path[i]; i++)
        if (path[i] == '/')
            end = i;
        else if (path[i] == ':')
            end = i + 1;
    if (end <= 0 || end + 1 > cap)
        return 0;
    memcpy(out, path, (size_t)end);
    out[end] = 0;
    return 1;
}

void prefs_theme_drawer(const prefs_fs *fs, const char *given, const char *home, char *out, int cap)
{
    if (given && given[0] && pc_drawer_of(given, out, cap) && fs->exists(fs->ctx, out))
        return;
    if (pc_join(out, cap, "", PREFS_THEMES_DIR) && fs->exists(fs->ctx, out))
        return;
    if (pc_join(out, cap, "", PREFS_THEMES_ENV) && fs->exists(fs->ctx, out))
        return;
    if (home && home[0]) {
        if (pc_join(out, cap, home, "themes") && fs->exists(fs->ctx, out))
            return;
        if (pc_join(out, cap, "", home) && fs->exists(fs->ctx, out))
            return;
    }
    if (cap > 0)
        out[0] = 0;
}

void prefs_theme_file(const char *drawer, const char *name, char *out, int cap)
{
    int l;
    if (cap < 1)
        return;
    if (strchr(name, ':') || strchr(name, '/') || !pc_join(out, cap, drawer, name))
        pc_copy(out, name, cap);
    l = (int)strlen(out);
    if (pc_conf_stem(out) < 0 && l + 6 <= cap)
        strcpy(out + l, ".conf");
}

int prefs_theme_add(char *names, int *len, int cap, const char *file)
{
    int stem = pc_conf_stem(file), at = 0;
    if (stem <= 0)
        return 0;
    while (at < *len) {
        int c = pc_ncmp_i(file, stem, names + at);
        if (!c)
            return 0;
        if (c < 0)
            break;
        at += (int)strlen(names + at) + 1;
    }
    if (*len + stem + 1 > cap)
        return -1;
    memmove(names + at + stem + 1, names + at, (size_t)(*len - at));
    memcpy(names + at, file, (size_t)stem);
    names[at + stem] = 0;
    *len += stem + 1;
    return 1;
}

int prefs_theme_index(const char *names, int n, const char *file)
{
    const char *base;
    int i, stem;
    if (!file || !file[0])
        return -1;
    for (base = file; *file; file++)
        if (*file == '/' || *file == ':')
            base = file + 1;
    stem = pc_conf_stem(base);
    if (stem < 0)
        stem = (int)strlen(base);
    for (i = 0; i < n; i++, names += strlen(names) + 1)
        if (stem && !pc_ncmp_i(base, stem, names))
            return i;
    return -1;
}

int prefs_grad_pens(unsigned short out[3], long p0, long p1)
{
    out[0] = out[1] = out[2] = 0xFFFF;
    if (p0 < 0 || p1 < 0 || p0 >= 0xFFFF || p1 >= 0xFFFF)
        return 0;
    out[0] = (unsigned short)p0;
    out[1] = (unsigned short)p1;
    return 1;
}
