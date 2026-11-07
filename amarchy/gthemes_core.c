/* gthemes_core (see gthemes_core.h). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gthemes_core.h"
#include "../prefs/settings_def.h"
#include "../prefs/prefs_core.h"
#include "amconf_core.h"

static upconf g_conf;

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int cmp_ci(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) {
        a++;
        b++;
    }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

uc_u32 gt_scale(uc_u32 rgb, int pct)
{
    uc_u32 r = ((rgb >> 16) & 0xff) * (uc_u32)pct / 100, g = ((rgb >> 8) & 0xff) * (uc_u32)pct / 100,
           b = (rgb & 0xff) * (uc_u32)pct / 100;
    return (r << 16) | (g << 8) | b;
}

uc_u32 gt_mix(uc_u32 a, uc_u32 b, int pct)
{
    uc_u32 out = 0;
    int sh;
    for (sh = 16; sh >= 0; sh -= 8) {
        long x = (long)((a >> sh) & 0xff), y = (long)((b >> sh) & 0xff);
        out |= (uc_u32)(x + (y - x) * pct / 100) << sh;
    }
    return out;
}

/* a moved a tenth of the way toward b, per channel */
static uc_u32 mix_tenth(uc_u32 a, uc_u32 b)
{
    uc_u32 out = 0;
    int sh;
    for (sh = 16; sh >= 0; sh -= 8) {
        long x = (long)((a >> sh) & 0xff), y = (long)((b >> sh) & 0xff);
        out |= (uc_u32)(x + (y - x) / 10) << sh;
    }
    return out;
}

/* ---- colour maths of the role derivation (moved from desk_core) ---- */

int gt_luma(uc_u32 rgb)
{
    return (int)((((rgb >> 16) & 0xff) * 77UL + ((rgb >> 8) & 0xff) * 150UL + (rgb & 0xff) * 29UL) >> 8);
}

int gt_contrast(uc_u32 a, uc_u32 b)
{
    int la = gt_luma(a), lb = gt_luma(b), hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 13) * 100 / (lo + 13);
}

/* the colour as a 12-bit (OCS) screen holds it: a nibble per channel, replicated. The weakest depth Amarchy runs on. */
static uc_u32 nib(uc_u32 c)
{
    return (((c >> 20) & 0xf) * 0x110000UL) | (((c >> 12) & 0xf) * 0x1100UL) | (((c >> 4) & 0xf) * 0x11UL);
}

static int qluma(uc_u32 c)
{
    return gt_luma(nib(c));
}

int gt_ground_black(const gtheme *t)
{
    return qluma(t->bg) == 0;
}

/* DESK-BACKDROP-DARKER: never the ground's own colour, on a 24-bit or a 12-bit palette. A ground that is black on the
 * screen keeps it and the backdrop lifts to dark grey; any other ground gets a tone strictly darker after the nibble cut. */
uc_u32 gt_darker(const gtheme *t)
{
    int pct;
    if (gt_ground_black(t))
        return GT_BLACK_LIFT;
    if (gt_luma(t->darker_bg) < gt_luma(t->bg) && qluma(t->darker_bg) < qluma(t->bg) &&
        gt_contrast(t->darker_bg, t->bg) >= GT_STEP_DARK && gt_contrast(t->darker_bg, t->fg) >= GT_FLOOR_TEXT)
        return t->darker_bg;
    for (pct = 80; pct > 0; pct -= 10)
        if (qluma(gt_scale(t->bg, pct)) < qluma(t->bg))
            return gt_scale(t->bg, pct);
    return 0x000000UL;
}

/* FLAT-COLOUR-OVERHAUL: the selection pen colour. The theme's selection when it stands GT_FLOOR_SEL off the surface (as the nibble-cut
 * screen shows both), else pushed away from the surface in 10 % steps: toward black on a light ground, toward white on a dark one. */
/* c pushed toward goal in 10 % steps until it stands floor off the ground, on the 24-bit palette and as the nibble-cut screen shows it */
static uc_u32 push_off(uc_u32 c0, uc_u32 goal, uc_u32 ground, int floor)
{
    int pct;
    for (pct = 0; pct <= 100; pct += 10) {
        uc_u32 c = pct ? 0 : c0;
        if (pct) {
            int sh;
            for (sh = 16; sh >= 0; sh -= 8) {
                int a = (int)((c0 >> sh) & 255), b = (int)((goal >> sh) & 255);
                c |= (uc_u32)(a + (b - a) * pct / 100) << sh;
            }
        }
        if (gt_contrast(c, ground) >= floor && gt_contrast(nib(c), nib(ground)) >= floor)
            return c;
    }
    return goal;
}

uc_u32 gt_selection(const gtheme *t)
{
    return push_off(t->selection, gt_luma(t->bg) >= 128 ? 0x000000UL : 0xFFFFFFUL, t->bg, GT_FLOOR_SEL);
}

/* RL8 step 3: the muted tone (the menu dim item) pushed off the surface until it reads at GT_FLOOR_MUTED */
uc_u32 gt_muted(const gtheme *t)
{
    return push_off(t->muted, gt_luma(t->bg) >= 128 ? 0x000000UL : 0xFFFFFFUL, t->bg, GT_FLOOR_MUTED);
}

/* RL8 FLAT-COLOUR-OVERHAUL / TE-2.1: the role table is the model's groups resolved (gt_roles_from_model); a theme without a model
 * gets the v1 model, whose group refs run gt_darker / gt_selection / gt_muted over the swatches. There is no second derivation. */
void gt_roles_of(const gtheme *t, gt_roles *r)
{
    if (t->model) {
        gt_roles_from_model(t->model, r);
    } else {
        /* the model is ~4.6 KB: heap, not this frame (B5, frames stay under 4096). Out of memory: all-black roles, never a crash. */
        gt_model *m = (gt_model *)malloc(sizeof *m);
        if (!m) {
            memset(r, 0, sizeof *r);
            return;
        }
        gt_model_from_v1(t, m);
        gt_roles_from_model(m, r);
        free(m);
    }
}

/* RL8: the desktop pens are a view of the role table (gt_roles_of); pen index == DK_* enum, 0..7. DK_BACKDROP's palette.prefs colour
 * stays the fail tone so the written file is byte-identical to before; live, desk_pen_colour gives the backdrop its own colour. */
uc_u32 desk_role(const gtheme *t, int role)
{
    gt_roles r;
    gt_roles_of(t, &r);
    switch (role) {
    case DK_BG: return r.c[GR_SURFACE];
    case DK_DARKER: return r.c[GR_BACKGROUND];
    case DK_FG: return r.c[GR_TEXT];
    case DK_ACCENT: return r.c[GR_ACCENT];
    case DK_SELECTION: return r.c[GR_SELECTION];
    case DK_MUTED: return r.c[GR_MUTED];
    case DK_GREEN: return r.ok;
    default: return r.fail;
    }
}

/* the one colour distance of every "nearest pen" choice (gt_nearest_role_pen, the flat role pens): squared RGB over 0xRRGGBB */
unsigned long gt_rgb_dist2(uc_u32 a, uc_u32 b)
{
    long dr = (long)((a >> 16) & 255) - (long)((b >> 16) & 255), dg = (long)((a >> 8) & 255) - (long)((b >> 8) & 255),
         db = (long)(a & 255) - (long)(b & 255);
    return (unsigned long)(dr * dr + dg * dg + db * db);
}

/* the nearest role pen: gt_rgb_dist2 over the pens DK_BG..last, strict < so a tie keeps the lower pen */
int gt_nearest_role_pen(const gtheme *t, uc_u32 c, int depth)
{
    int pen, best = 0, last = depth <= 2 ? DK_ACCENT : DK_DARKER; /* 3 at depth 2, 5 (DK_DARKER) from depth 3 */
    unsigned long bd = 0, d;
    for (pen = 0; pen <= last; pen++) {
        d = gt_rgb_dist2(c, desk_role(t, pen));
        if (pen == 0 || d < bd) {
            bd = d;
            best = pen;
        }
    }
    return best;
}

/* a quoted or bare text value of the file into dst (cap bytes), "" when the key is absent */
static void str_key(char *dst, int cap, const char *key)
{
    const char *v = upconf_str(&g_conf, "default", key, "");
    int n = 0;
    while (*v == '"' || *v == ' ')
        v++;
    while (v[n] && v[n] != '"' && v[n] != '\r' && v[n] != '\n' && n < cap - 1)
        n++;
    memcpy(dst, v, (size_t)n);
    dst[n] = 0;
}

/* Owner rule (2026-10-31): plain white text on a dark theme is never wanted; a dark theme whose foreground is exactly
 * #ffffff takes this grey instead (only vantablack today). Light themes are untouched. */
#define GT_DARK_WHITE_FG 0xBEBEBEUL

/* tokyo-night: background #1a1b26, foreground #a9b1d6, dark_foreground #565f89, accent #7aa2f7 (its colors.toml) */
void gt_builtin(gtheme *out)
{
    memset(out, 0, sizeof *out);
    strcpy(out->name, GT_DEFAULT);
    out->bg = 0x1a1b26UL;
    out->fg = 0xa9b1d6UL;
    out->dim = 0x565f89UL;
    out->accent = 0x7aa2f7UL;
    out->ok = 0x9ece6aUL;   /* green */
    out->fail = 0xf7768eUL; /* red */
    out->dark_bg = 0x13141cUL;
    out->darker_bg = 0x0e0e14UL;
    out->lighter_bg = 0x24283bUL;
    out->selection = 0x292e42UL;
    out->muted = 0x414868UL;
    out->inactive = gt_mix(out->bg, out->fg, GT_INACTIVE_PCT);
}

void gt_parse(const char *buf, long len, gtheme *out)
{
    gtheme d;
    const char *m;
    gt_builtin(&d);
    /* TE-2: gt_parse yields a v1 theme without a model; a caller that owns an attached model calls gt_free first. Every
     * caller (a stack gtheme in gt_term_palette / gt_model_parse, the loaders after gt_builtin) then reads model == 0. */
    out->model = 0;
    upconf_clear(&g_conf);
    upconf_parse(&g_conf, buf, len);
    /* the mode first: the foreground rule below depends on it, and every colour derived from the foreground (the
     * fail fallback, lighter_bg and so the selection, the terminal palette) must see the adjusted value */
    m = upconf_str(&g_conf, "default", "mode", "");
    while (*m == '"' || *m == ' ')
        m++;
    out->light = !strncmp(m, "light", 5);
    out->bg = upconf_rgb(&g_conf, "default", "background", d.bg);
    out->fg = upconf_rgb(&g_conf, "default", "foreground", d.fg);
    if (!out->light && out->fg == 0xFFFFFFUL)
        out->fg = GT_DARK_WHITE_FG;
    out->dim = upconf_rgb(&g_conf, "default", "dark_foreground", d.dim);
    out->accent = upconf_rgb(&g_conf, "default", "accent", d.accent);
    /* a file without green or red (not an Omarchy colors.toml) gives the accent and the foreground, never the
     * built-in's colours, which would clash with another theme's ground */
    out->ok = upconf_rgb(&g_conf, "default", "green", out->accent);
    out->fail = upconf_rgb(&g_conf, "default", "red", out->fg);
    out->dark_bg = upconf_rgb(&g_conf, "default", "dark_background", out->bg);
    /* darker: its own key, else dark_background when the file has one, else the background at 80 % */
    out->darker_bg = upconf_rgb(&g_conf, "default", "darker_background",
                                upconf_str(&g_conf, "default", "dark_background", 0) ? out->dark_bg : gt_scale(out->bg, 80));
    out->lighter_bg = upconf_rgb(&g_conf, "default", "lighter_background", mix_tenth(out->bg, out->fg));
    out->selection = upconf_rgb(&g_conf, "default", "selection", out->lighter_bg);
    out->muted = upconf_rgb(&g_conf, "default", "muted", out->dim);
    out->inactive = upconf_rgb(&g_conf, "default", "inactive", gt_mix(out->bg, out->fg, GT_INACTIVE_PCT));
    str_key(out->backdrop, GT_NAME, "backdrop");
    str_key(out->font, GT_NAME, "font");
    str_key(out->wbtitle, GT_TITLE, "wbtitle");
}

int gt_term_text(const char *buf, long len, const char *name, char *out, int cap)
{
    /* ANSI order: black, red, green, yellow, blue, magenta, cyan, white; then the bright row. A key the file lacks takes the
     * foreground (never a colour of another theme), black is the darkest ground of the file. */
    static const char *const col[6] = { "red", "green", "yellow", "blue", "magenta", "cyan" };
    gtheme t;
    uc_u32 p[16], fg;
    char key[24];
    int i, n;
    if (!out || cap < 1)
        return 0;
    gt_parse(buf, len, &t);
    fg = t.fg;
    p[0] = t.darker_bg;
    for (i = 0; i < 6; i++) {
        p[1 + i] = upconf_rgb(&g_conf, "default", col[i], fg);
        snprintf(key, sizeof key, "bright_%s", col[i]);
        p[9 + i] = upconf_rgb(&g_conf, "default", key, p[1 + i]);
    }
    p[7] = upconf_rgb(&g_conf, "default", "light_foreground", fg);
    p[8] = t.muted;
    p[15] = upconf_rgb(&g_conf, "default", "bright_foreground", fg);
    n = snprintf(out, (size_t)cap, "[profile %s]\nfg = %06lX\nbg = %06lX\ncursor-color = %06lX\npalette = ",
                 name && *name ? name : "amarchy", (unsigned long)t.fg, (unsigned long)t.bg, (unsigned long)t.accent);
    for (i = 0; i < 16 && n > 0 && n < cap; i++)
        n += snprintf(out + n, (size_t)(cap - n), "%s%d,%06lX", i ? "," : "", i, (unsigned long)p[i]);
    if (n > 0 && n < cap - 1)
        n += snprintf(out + n, (size_t)(cap - n), "\n");
    /* TE-7b.2: the theme's Shell Font as the profile key "font = NAME:SIZE" (the handler turns ':' into ' ' and parse_font reads
     * it; NAME keeps its ".font"). Read from the look slot through the model parser, the one parser of "name.font/size". No
     * Shell Font, or a size missing or 0: no line, the terminal keeps its own font. */
    if (n > 0 && n < cap && gt_model_attach(&t, buf, len)) {
        char fname[GT_NAME];
        int fsize;
        if (gt_look_font(&t, GT_FONT_SHELL, fname, (int)sizeof fname, &fsize) && fsize > 0)
            n += snprintf(out + n, (size_t)(cap - n), "font = %s:%d\n", fname, fsize);
        gt_free(&t);
    }
    return n > 0 && n < cap ? n : 0;
}

int gt_path(const char *dir, const char *name, char *out, int cap)
{
    size_t dl = strlen(dir);
    int sl = dl && dir[dl - 1] != ':' && dir[dl - 1] != '/';
    if (!name || !*name || (int)(dl + sl + strlen(name) + sizeof GT_EXT) > cap)
        return 0;
    snprintf(out, (size_t)cap, "%s%s%s%s", dir, sl ? "/" : "", name, GT_EXT);
    return 1;
}

int gt_name_of_file(const char *file, char *out, int cap)
{
    size_t n = strlen(file), e = sizeof GT_EXT - 1;
    if (n <= e || cmp_ci(file + n - e, GT_EXT) != 0 || (int)(n - e) >= cap || (int)(n - e) >= GT_NAME)
        return 0;
    memcpy(out, file, n - e);
    out[n - e] = 0;
    return 1;
}

void gt_list_add(gt_list *l, const char *file, int from_user)
{
    char name[GT_NAME];
    int i, at;
    if (!gt_name_of_file(file, name, sizeof name))
        return;
    for (i = 0; i < l->n; i++)
        if (!cmp_ci(l->name[i], name)) {
            if (from_user) {
                l->user[i] = 1;
                strcpy(l->name[i], name);
            }
            return;
        }
    if (l->n >= GT_MAX)
        return;
    for (at = 0; at < l->n && cmp_ci(l->name[at], name) < 0; at++)
        ;
    for (i = l->n; i > at; i--) {
        strcpy(l->name[i], l->name[i - 1]);
        l->user[i] = l->user[i - 1];
    }
    strcpy(l->name[at], name);
    l->user[at] = from_user ? 1 : 0;
    l->n++;
}

static char g_text[UC_MAX_FILE];

void gt_load(gt_read_fn rd, void *ctx, const char *name, gtheme *out)
{
    static const char *const dirs[2] = { GT_USER_DIR, GT_SHIPPED_DIR };
    char path[160];
    int d;
    gt_builtin(out);
    for (d = 0; d < 2; d++) {
        long n;
        if (!gt_path(dirs[d], name, path, sizeof path))
            return; /* an unusable name: the default */
        n = rd(ctx, path, g_text, (long)sizeof g_text);
        if (n >= 0) {
            gt_parse(g_text, n, out);
            strncpy(out->name, name, GT_NAME - 1);
            out->name[GT_NAME - 1] = 0;
            return;
        }
    }
}

int gt_load_term(gt_read_fn rd, void *ctx, const char *name, char *out, int cap)
{
    static const char *const dirs[2] = { GT_USER_DIR, GT_SHIPPED_DIR };
    char path[160];
    int d;
    for (d = 0; d < 2; d++) {
        long n;
        if (!gt_path(dirs[d], name, path, sizeof path))
            return 0;
        n = rd(ctx, path, g_text, (long)sizeof g_text);
        if (n >= 0)
            return gt_term_text(g_text, n, name, out, cap);
    }
    return 0; /* an unknown name: the tabs keep their colours */
}

int gt_name_ok(const char *name)
{
    return name && *name && (int)strlen(name) < GT_NAME && !strchr(name, '/') && !strchr(name, ':');
}

void gt_label(const char *name, const char *current, char *out, int cap)
{
    snprintf(out, (size_t)cap, "%s%s", name, current && !cmp_ci(name, current) ? " (current)" : "");
}

typedef struct scan_ctx {
    gt_list *l;
    int user;
} scan_ctx;

static void scan_emit(void *ectx, const char *file)
{
    scan_ctx *c = (scan_ctx *)ectx;
    gt_list_add(c->l, file, c->user);
}

void gt_scan(gt_dir_fn dir, void *ctx, gt_list *l)
{
    scan_ctx c;
    memset(l, 0, sizeof *l);
    c.l = l;
    c.user = 0;
    dir(ctx, GT_SHIPPED_DIR, scan_emit, &c);
    c.user = 1;
    dir(ctx, GT_USER_DIR, scan_emit, &c);
    if (!l->n)
        gt_list_add(l, GT_DEFAULT GT_EXT, 0); /* a bare install: the compiled-in theme is still pickable */
}

int gt_path_name(const char *path, char *out, int cap)
{
    size_t p = sizeof GT_ROW_PREFIX - 1;
    if (strncmp(path, GT_ROW_PREFIX, p) || !path[p] || (int)strlen(path + p) >= cap || (int)strlen(path + p) >= GT_NAME)
        return 0;
    strcpy(out, path + p);
    return 1;
}

int gt_preview_pick(const char *row_path, const char *current, const char *drawn, char *out, int cap)
{
    char row[GT_NAME];
    const char *want = row_path && gt_path_name(row_path, row, (int)sizeof row) ? row : current;
    if (!want || !*want || (int)strlen(want) >= cap || (drawn && !cmp_ci(want, drawn)))
        return 0;
    strcpy(out, want);
    return 1;
}

int gt_preview_term(const char *row_path, const char *current, const char *drawn, char *out, int cap,
                    void (*send)(const char *name))
{
    if (!gt_preview_pick(row_path, current, drawn, out, cap))
        return 0;
    if (send)
        send(out);
    return 1;
}

int gt_preview_step(const char *row_path, const char *current, char *drawn, int cap, void (*send)(const char *name))
{
    char want[GT_NAME];
    if (!gt_preview_term(row_path, current, drawn, want, (int)sizeof want, send) || (int)strlen(want) >= cap)
        return 0;
    strcpy(drawn, want);
    return 1;
}

/* The path each install step touched, by the step's own words (prefs_core.h prefs_install_target_at). */
void gt_save_text(int step, int failed, long err, char *out, int cap)
{
    const prefs_paths *p = &prefs_paths_amarchy;
    const char *verb = "write", *what;
    int t = failed == PREFS_T_ENVARC ? PREFS_T_ENVARC : PREFS_T_ENV;
    long e = err < 0 ? 0 : err;
    switch (step) {
    case GT_SAVE_NAME:
        verb = "check the name of";
        what = "the theme";
        break;
    case GT_SAVE_SHIPPED:
        snprintf(out, (size_t)cap, "that name belongs to a shipped theme: save the copy under a new name");
        return;
    case GT_SAVE_EXISTS:
        snprintf(out, (size_t)cap, "a theme of that name already exists");
        return;
    case GT_SAVE_MISSING:
        snprintf(out, (size_t)cap, "there is no such user theme");
        return;
    case GT_SAVE_WRITE:
        snprintf(out, (size_t)cap, "the theme could not be written");
        return;
    case PREFS_SAVE_NODIR:
        verb = "make the drawer";
        what = p->dir[t];
        break;
    case PREFS_INSTALL_BACKUP:
        verb = "back up to";
        what = p->orig;
        break;
    case PREFS_INSTALL_PLACE:
        verb = "rename into place";
        what = p->path[t];
        break;
    case PREFS_INSTALL_STRANDED:
        verb = "restore after a failed rename";
        what = p->path[t];
        break;
    default: /* PREFS_INSTALL_WRITE: ENVARC: writes the tmp file first, ENV: the file itself */
        what = t == PREFS_T_ENVARC ? p->tmp : p->path[t];
        break;
    }
    if (cap <= 0)
        return;
    if (e)
        snprintf(out, (size_t)cap, "the theme could not be saved: %s %s, error %ld", verb, what, e);
    else
        snprintf(out, (size_t)cap, "the theme could not be saved: %s %s", verb, what);
}

int gt_boot_term(const char *configured, void (*send)(const char *name))
{
    send(configured && gt_name_ok(configured) ? configured : GT_DEFAULT);
    return 1;
}

/* ====== Theme file format v2 and the model (plan 2026-11-05-amarchy-theme-editor.md, TE-1) ====== */
#include <stdlib.h>

/* The model is heap-allocated only through gt_model_attach; an Amarchy program that links this file has no VT_MALLOC (that
 * is the engine's), so plain malloc / free (deviation from the plan text, which named VT_MALLOC). */
#define GT_ALLOC(n) malloc(n)
#define GT_FREE(p) free(p)

const gt_group_def gt_group_defs[GTG_COUNT] = {
    { "background", "Background", "darker(background)" }, /* the desktop backdrop: gt_darker over the surface */
    { "surface", "Surface", "background" },
    { "text", "Text", "foreground" },
    { "muted-text", "Muted Text", "muted" },
    { "accent", "Accent", "accent" },
    { "selection", "Selection", "selection(selection)" }, /* gt_selection over the surface */
    { "inactive", "Inactive", "inactive" },
    { "dim", "Dim", "muted(muted)" }, /* gt_muted over the surface: the menu's readable disabled tone */
    { "success", "Success", "green" },
    { "failure", "Failure", "red" },
};

#define ST_ALL 15 /* owner answer D6: four states for every element */
const gt_elem_def gt_elem_defs[GTE_COUNT] = {
    { "title-bar", "Title Bar", GTG_SURFACE, ST_ALL, "DrawInfo FILLPEN" },
    { "title-bar-inactive", "Title Bar, Inactive Window", GTG_INACTIVE, ST_ALL, "inactive window bar" },
    { "title-text", "Title Text", GTG_TEXT, ST_ALL, "DrawInfo FILLTEXTPEN" },
    { "title-text-inactive", "Title Text, Inactive Window", GTG_MUTED_TEXT, ST_ALL, "inactive window title" },
    { "border", "Window Border", GTG_MUTED_TEXT, ST_ALL, "DrawInfo SHINEPEN" },
    { "border-inactive", "Window Border, Inactive Window", GTG_INACTIVE, ST_ALL, "inactive window frame" },
    { "gadget-glyph", "Gadget Glyph", GTG_TEXT, ST_ALL, "window gadgets" },
    { "gadget-glyph-inactive", "Gadget Glyph, Inactive Window", GTG_INACTIVE, ST_ALL, "inactive window gadgets" },
    { "scroll-track", "Scroll Track", GTG_BACKGROUND, ST_ALL, "scroller trough" },
    { "scroll-knob", "Scroll Knob", GTG_MUTED_TEXT, ST_ALL, "scroller knob" },
    { "menu-background", "Menu Background", GTG_SURFACE, ST_ALL, "GR_SURFACE" },
    { "menu-text", "Menu Text", GTG_TEXT, ST_ALL, "GR_TEXT" },
    { "menu-selection", "Menu Selection", GTG_SELECTION, ST_ALL, "GR_SELECTION" },
    { "menu-selected-text", "Menu Selected Text", GTG_TEXT, ST_ALL, "selected row text" },
    { "menu-header", "Menu Header", GTG_ACCENT, ST_ALL, "GR_ACCENT" },
    { "menu-separator", "Menu Separator", GTG_MUTED_TEXT, ST_ALL, "GR_MUTED" },
    { "requester-background", "Requester Background", GTG_SURFACE, ST_ALL, "DrawInfo BACKGROUNDPEN" },
    { "button-face", "Button Face", GTG_SURFACE, ST_ALL, "button fill" },
    { "button-text", "Button Text", GTG_TEXT, ST_ALL, "button label" },
    { "text-field", "Text Field", GTG_BACKGROUND, ST_ALL, "string gadget fill" },
    { "text-cursor", "Text Cursor", GTG_ACCENT, ST_ALL, "string gadget cursor" },
    { "desktop-backdrop", "Desktop Backdrop", GTG_BACKGROUND, ST_ALL, "GR_BACKGROUND" },
    { "icon-label-text", "Icon Label Text", GTG_TEXT, ST_ALL, "icon label" },
    { "icon-label-shadow", "Icon Label Shadow", GTG_BACKGROUND, ST_ALL, "icon label shadow" },
    { "icon-label-outline", "Icon Label Outline", GTG_BACKGROUND, ST_ALL, "icon label outline" },
    { "icon-foreground", "Icon Foreground", GTG_TEXT, ST_ALL, "icon slot foreground" },
    { "icon-accent", "Icon Accent", GTG_ACCENT, ST_ALL, "icon slot accent" },
    { "icon-muted", "Icon Muted", GTG_MUTED_TEXT, ST_ALL, "icon slot muted" },
    { "icon-selection-plate", "Icon Selection Plate", GTG_SELECTION, ST_ALL, "selected icon plate" },
    { "helper-background", "Helper Window Background", GTG_SURFACE, ST_ALL, "flat helper surface" },
    { "helper-text", "Helper Window Text", GTG_TEXT, ST_ALL, "flat helper text" },
    { "terminal-background", "Terminal Background", GTG_SURFACE, ST_ALL, "terminal palette background" },
    { "terminal-foreground", "Terminal Foreground", GTG_TEXT, ST_ALL, "terminal palette foreground" },
    { "terminal-cursor", "Terminal Cursor", GTG_ACCENT, ST_ALL, "terminal palette cursor" },
    { "success-text", "Success Text", GTG_SUCCESS, ST_ALL, "Done footer" },
    { "failure-text", "Failure Text", GTG_FAILURE, ST_ALL, "Failed footer" },
};

static int find_swatch(const gt_model *m, const char *name, int n)
{
    int i;
    for (i = 0; i < m->nswatch; i++)
        if ((int)strlen(m->swatch_name[i]) == n && !strncmp(m->swatch_name[i], name, (size_t)n))
            return i;
    return -1;
}

static int find_swatch_z(const gt_model *m, const char *name)
{
    return find_swatch(m, name, (int)strlen(name));
}

/* ---- the ref parser ---- */
typedef struct rp {
    gt_model *m;
    const char *s;
} rp;

static void rp_skip(rp *p)
{
    while (*p->s == ' ' || *p->s == '\t')
        p->s++;
}

static int is_name(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static int nested_add(gt_model *m, const gt_ref *r)
{
    if (m->nnested >= GT_NESTED_MAX) {
        m->warn |= GT_WARN_OVERFLOW;
        return -1;
    }
    m->nested[m->nnested] = *r;
    return m->nnested++;
}

static int rp_ref(rp *p, int depth, gt_ref *out);

/* "(" ref ")" for the one-argument functions */
static int rp_func1(rp *p, int depth, int kind, gt_ref *out)
{
    gt_ref c;
    int slot;
    if (depth + 1 > GT_REF_DEPTH)
        return 0;
    rp_skip(p);
    if (*p->s != '(')
        return 0;
    p->s++;
    if (!rp_ref(p, depth + 1, &c))
        return 0;
    rp_skip(p);
    if (*p->s != ')')
        return 0;
    p->s++;
    slot = nested_add(p->m, &c);
    if (slot < 0)
        return 0;
    memset(out, 0, sizeof *out);
    out->kind = (unsigned char)kind;
    out->a = (unsigned char)slot;
    return 1;
}

static int rp_mix(rp *p, int depth, gt_ref *out)
{
    gt_ref x, y;
    int sa, sb, pct = 0, digits = 0;
    if (depth + 1 > GT_REF_DEPTH)
        return 0;
    rp_skip(p);
    if (*p->s != '(')
        return 0;
    p->s++;
    if (!rp_ref(p, depth + 1, &x))
        return 0;
    rp_skip(p);
    if (*p->s != ';')
        return 0;
    p->s++;
    if (!rp_ref(p, depth + 1, &y))
        return 0;
    rp_skip(p);
    if (*p->s != ';')
        return 0;
    p->s++;
    rp_skip(p);
    while (*p->s >= '0' && *p->s <= '9' && digits < 4) {
        pct = pct * 10 + (*p->s - '0');
        p->s++;
        digits++;
    }
    rp_skip(p);
    if (!digits || pct > 100 || *p->s != ')')
        return 0;
    p->s++;
    sa = nested_add(p->m, &x);
    sb = nested_add(p->m, &y);
    if (sa < 0 || sb < 0)
        return 0;
    memset(out, 0, sizeof *out);
    out->kind = GT_REF_MIX;
    out->a = (unsigned char)sa;
    out->b = (unsigned char)sb;
    out->pct = (unsigned char)pct;
    return 1;
}

static int rp_ref(rp *p, int depth, gt_ref *out)
{
    const char *b;
    int n, i;
    rp_skip(p);
    memset(out, 0, sizeof *out);
    if (*p->s == '#') {
        char hex[8];
        uc_u32 rgb;
        for (n = 0; n < 7 && p->s[n]; n++)
            hex[n] = p->s[n];
        hex[n] = 0;
        if (n != 7 || !upconf_hex6(hex, &rgb))
            return 0;
        p->s += 7;
        out->kind = GT_REF_RGB;
        out->rgb = rgb;
        return 1;
    }
    b = p->s;
    for (n = 0; is_name(b[n]); n++)
        ;
    if (!n)
        return 0;
    p->s += n;
    if (b[n] == ':' && n == 5 && !strncmp(b, "group", 5)) {
        const char *k = ++p->s;
        for (n = 0; is_name(k[n]); n++)
            ;
        for (i = 0; i < GTG_COUNT; i++)
            if ((int)strlen(gt_group_defs[i].key) == n && !strncmp(gt_group_defs[i].key, k, (size_t)n)) {
                p->s += n;
                out->kind = GT_REF_GROUP;
                out->a = (unsigned char)i;
                return 1;
            }
        return 0;
    }
    rp_skip(p);
    if (*p->s == '(') {
        if (n == 6 && !strncmp(b, "darker", 6))
            return rp_func1(p, depth, GT_REF_DARKER, out);
        if (n == 9 && !strncmp(b, "selection", 9))
            return rp_func1(p, depth, GT_REF_SELECTION, out);
        if (n == 5 && !strncmp(b, "muted", 5))
            return rp_func1(p, depth, GT_REF_MUTED, out);
        if (n == 3 && !strncmp(b, "mix", 3))
            return rp_mix(p, depth, out);
        return 0;
    }
    i = find_swatch(p->m, b, n);
    if (i < 0)
        return 0;
    out->kind = GT_REF_SWATCH;
    out->a = (unsigned char)i;
    return 1;
}

int gt_ref_parse(gt_model *m, const char *text, gt_ref *out)
{
    rp p;
    int save = m->nnested;
    p.m = m;
    p.s = text;
    rp_skip(&p);
    memset(out, 0, sizeof *out);
    if (!*p.s)
        return 1;
    if (rp_ref(&p, 0, out)) {
        rp_skip(&p);
        if (!*p.s)
            return 1;
    }
    memset(out, 0, sizeof *out);
    m->nnested = save;
    m->warn |= GT_WARN_BAD_REF;
    return 0;
}

/* ---- ref text ---- */
static void hex_text(uc_u32 rgb, char *out)
{
    snprintf(out, 8, "#%06lx", (unsigned long)(rgb & 0xFFFFFFUL));
}

static int ref_text_d(const gt_model *m, const gt_ref *r, char *out, int cap, int d)
{
    char a[96], b[96];
    int n;
    if (d > GT_REF_DEPTH + 1)
        return -1;
    switch (r->kind) {
    case GT_REF_SWATCH:
        n = (int)strlen(m->swatch_name[r->a]);
        if (n >= cap)
            return -1;
        strcpy(out, m->swatch_name[r->a]);
        return n;
    case GT_REF_RGB:
        if (cap < 8)
            return -1;
        hex_text(r->rgb, out);
        return 7;
    case GT_REF_GROUP:
        if ((int)strlen(gt_group_defs[r->a].key) + 7 > cap)
            return -1;
        return snprintf(out, (size_t)cap, "group:%s",gt_group_defs[r->a].key);
    case GT_REF_DARKER:
    case GT_REF_SELECTION:
    case GT_REF_MUTED: {
        const char *fn = r->kind == GT_REF_DARKER ? "darker" : r->kind == GT_REF_SELECTION ? "selection" : "muted";
        if (ref_text_d(m, &m->nested[r->a], a, (int)sizeof a, d + 1) < 0)
            return -1;
        n = (int)strlen(fn) + (int)strlen(a) + 2;
        if (n >= cap)
            return -1;
        return snprintf(out, (size_t)cap, "%s(%s)", fn, a);
    }
    case GT_REF_MIX:
        if (ref_text_d(m, &m->nested[r->a], a, (int)sizeof a, d + 1) < 0 ||
            ref_text_d(m, &m->nested[r->b], b, (int)sizeof b, d + 1) < 0)
            return -1;
        n = (int)strlen(a) + (int)strlen(b) + 12;
        if (n >= cap)
            return -1;
        return snprintf(out, (size_t)cap, "mix(%s;%s;%d)",a, b, (int)r->pct);
    default:
        if (cap < 1)
            return -1;
        out[0] = 0;
        return 0;
    }
}

int gt_ref_text(const gt_model *m, const gt_ref *r, char *out, int cap)
{
    return ref_text_d(m, r, out, cap, 0);
}

/* ---- evaluation ---- */
#define GT_EVAL_MAX 24 /* group chains followed; a cycle is cut at load, this stops a hand-built one */

static uc_u32 ref_d(const gt_model *m, const gt_ref *r, int state, int d);

static uc_u32 group_d(const gt_model *m, int g, int state, int d)
{
    const gt_ref *r = &m->group[g][state];
    if (r->kind == GT_REF_NONE)
        r = &m->group[g][GT_ST_NORMAL];
    return ref_d(m, r, state, d + 1);
}

static uc_u32 swatch_or_group(const gt_model *m, const char *name, int g, int state, int d)
{
    int i = find_swatch_z(m, name);
    return i >= 0 ? m->swatch[i] : group_d(m, g, state, d);
}

/* the context gtheme the derived forms run through, so they are gt_darker / gt_selection / gt_muted themselves */
static void ctx_theme(const gt_model *m, gtheme *t, int state, int d)
{
    memset(t, 0, sizeof *t);
    t->bg = swatch_or_group(m, "background", GTG_SURFACE, state, d);
    t->fg = swatch_or_group(m, "foreground", GTG_TEXT, state, d);
    t->light = m->light;
}

static uc_u32 ref_d(const gt_model *m, const gt_ref *r, int state, int d)
{
    gtheme t;
    uc_u32 v;
    int i;
    if (d > GT_EVAL_MAX)
        return 0;
    switch (r->kind) {
    case GT_REF_SWATCH:
        return m->swatch[r->a];
    case GT_REF_RGB:
        return r->rgb;
    case GT_REF_GROUP:
        return group_d(m, r->a, state, d);
    case GT_REF_MIX:
        return gt_mix(ref_d(m, &m->nested[r->a], state, d + 1), ref_d(m, &m->nested[r->b], state, d + 1), (int)r->pct);
    case GT_REF_DARKER:
        v = ref_d(m, &m->nested[r->a], state, d + 1);
        ctx_theme(m, &t, state, d);
        t.bg = v;
        i = find_swatch_z(m, "darker");
        t.darker_bg = i >= 0 ? m->swatch[i] : gt_scale(v, 80);
        return gt_darker(&t);
    case GT_REF_SELECTION:
        v = ref_d(m, &m->nested[r->a], state, d + 1);
        ctx_theme(m, &t, state, d);
        t.selection = v;
        return gt_selection(&t);
    case GT_REF_MUTED:
        v = ref_d(m, &m->nested[r->a], state, d + 1);
        ctx_theme(m, &t, state, d);
        t.muted = v;
        return gt_muted(&t);
    default:
        return 0;
    }
}

uc_u32 gt_ref_rgb(const gt_model *m, const gt_ref *r, int state)
{
    return ref_d(m, r, state, 0);
}

uc_u32 gt_group_rgb(const gt_model *m, int group, int state)
{
    return group_d(m, group, state, 0);
}

gt_ref gt_resolve(const gt_model *m, int elem, int state)
{
    const gt_ref *r = &m->elem[elem][state];
    int g = gt_elem_defs[elem].group;
    if (r->kind != GT_REF_NONE)
        return *r;
    /* trailing states fall back to the element's own normal, then the group's state, then the group's normal */
    if (m->elem[elem][GT_ST_NORMAL].kind != GT_REF_NONE)
        return m->elem[elem][GT_ST_NORMAL];
    if (m->group[g][state].kind != GT_REF_NONE)
        return m->group[g][state];
    return m->group[g][GT_ST_NORMAL];
}

uc_u32 gt_resolve_rgb(const gt_model *m, int elem, int state)
{
    gt_ref r = gt_resolve(m, elem, state);
    uc_u32 c = ref_d(m, &r, state, 0);
    /* TE-8 P1: the DEFAULT of the inactive title text (no own ref in the model, so it is the group's muted tone) is pushed off the
     * inactive title bar until the pair reads GT_FLOOR_DIM. A value the model sets itself is returned as set (it only warns). */
    if (elem == GTE_TITLE_TEXT_INACTIVE && m->elem[elem][state].kind == GT_REF_NONE && m->elem[elem][GT_ST_NORMAL].kind == GT_REF_NONE) {
        gt_ref gr = gt_resolve(m, GTE_TITLE_BAR_INACTIVE, state);
        uc_u32 ground = ref_d(m, &gr, state, 0);
        c = push_off(c, gt_luma(ground) >= 128 ? 0x000000UL : 0xFFFFFFUL, ground, GT_FLOOR_DIM);
    }
    return c;
}

void gt_roles_from_model(const gt_model *m, gt_roles *r)
{
    r->c[GR_BACKGROUND] = gt_group_rgb(m, GTG_BACKGROUND, GT_ST_NORMAL);
    r->c[GR_SURFACE] = gt_group_rgb(m, GTG_SURFACE, GT_ST_NORMAL);
    r->c[GR_TEXT] = gt_group_rgb(m, GTG_TEXT, GT_ST_NORMAL);
    r->c[GR_MUTED] = gt_group_rgb(m, GTG_MUTED_TEXT, GT_ST_NORMAL);
    r->dim = gt_group_rgb(m, GTG_DIM, GT_ST_NORMAL);
    r->c[GR_ACCENT] = gt_group_rgb(m, GTG_ACCENT, GT_ST_NORMAL);
    r->c[GR_SELECTION] = gt_group_rgb(m, GTG_SELECTION, GT_ST_NORMAL);
    r->c[GR_INACTIVE] = gt_group_rgb(m, GTG_INACTIVE, GT_ST_NORMAL);
    r->ok = gt_group_rgb(m, GTG_SUCCESS, GT_ST_NORMAL);
    r->fail = gt_group_rgb(m, GTG_FAILURE, GT_ST_NORMAL);
}

uc_u32 gt_elem_rgb(const gtheme *t, int elem, uc_u32 fallback)
{
    if (!t || !t->model || elem < 0 || elem >= GTE_COUNT || t->model->elem[elem][GT_ST_NORMAL].kind == GT_REF_NONE)
        return fallback;
    return gt_resolve_rgb(t->model, elem, GT_ST_NORMAL);
}

/* ---- v1 -> model ---- */
static void swatch_add(gt_model *m, const char *name, uc_u32 rgb)
{
    if (m->nswatch >= GT_SWATCH_MAX) {
        m->warn |= GT_WARN_OVERFLOW;
        return;
    }
    strncpy(m->swatch_name[m->nswatch], name, GT_SWATCH_NAME - 1);
    m->swatch_name[m->nswatch][GT_SWATCH_NAME - 1] = 0;
    m->swatch[m->nswatch++] = rgb;
}

static void group_defaults(gt_model *m, int g)
{
    memset(m->group[g], 0, sizeof m->group[g]);
    gt_ref_parse(m, gt_group_defs[g].def, &m->group[g][GT_ST_NORMAL]);
}

static void model_init(gt_model *m)
{
    memset(m, 0, sizeof *m);
    m->version = 1;
    m->look.inactive_pct = GT_INACTIVE_PCT;
}

void gt_model_from_v1(const gtheme *t, gt_model *m)
{
    int g;
    model_init(m);
    swatch_add(m, "background", t->bg);
    swatch_add(m, "foreground", t->fg);
    swatch_add(m, "dim", t->dim);
    swatch_add(m, "accent", t->accent);
    swatch_add(m, "green", t->ok);
    swatch_add(m, "red", t->fail);
    swatch_add(m, "dark", t->dark_bg);
    swatch_add(m, "darker", t->darker_bg);
    swatch_add(m, "lighter", t->lighter_bg);
    swatch_add(m, "selection", t->selection);
    swatch_add(m, "muted", t->muted);
    swatch_add(m, "inactive", t->inactive);
    for (g = 0; g < GTG_COUNT; g++)
        group_defaults(m, g);
    m->light = t->light;
    strncpy(m->name, t->name, GT_NAME - 1);
    strncpy(m->backdrop, t->backdrop, GT_NAME - 1);
    strncpy(m->font, t->font, GT_NAME - 1);
    strncpy(m->wbtitle, t->wbtitle, GT_TITLE - 1);
}

void gt_model_to_v1(const gt_model *m, gtheme *t)
{
    int i;
    memset(t, 0, sizeof *t);
    strncpy(t->name, m->name, GT_NAME - 1);
    t->light = m->light;
    t->bg = gt_group_rgb(m, GTG_SURFACE, GT_ST_NORMAL);
    t->fg = gt_group_rgb(m, GTG_TEXT, GT_ST_NORMAL);
    t->accent = gt_group_rgb(m, GTG_ACCENT, GT_ST_NORMAL);
    t->ok = gt_group_rgb(m, GTG_SUCCESS, GT_ST_NORMAL);
    t->fail = gt_group_rgb(m, GTG_FAILURE, GT_ST_NORMAL);
    t->darker_bg = gt_group_rgb(m, GTG_BACKGROUND, GT_ST_NORMAL);
    t->selection = gt_group_rgb(m, GTG_SELECTION, GT_ST_NORMAL);
    t->muted = gt_group_rgb(m, GTG_MUTED_TEXT, GT_ST_NORMAL);
    t->inactive = gt_group_rgb(m, GTG_INACTIVE, GT_ST_NORMAL);
    i = find_swatch_z(m, "dim");
    t->dim = i >= 0 ? m->swatch[i] : gt_group_rgb(m, GTG_DIM, GT_ST_NORMAL);
    i = find_swatch_z(m, "dark");
    t->dark_bg = i >= 0 ? m->swatch[i] : t->bg;
    i = find_swatch_z(m, "lighter");
    t->lighter_bg = i >= 0 ? m->swatch[i] : t->selection;
    strncpy(t->backdrop, m->backdrop, GT_NAME - 1);
    strncpy(t->font, m->font, GT_NAME - 1);
    strncpy(t->wbtitle, m->wbtitle, GT_TITLE - 1);
}

int gt_look_font(const gtheme *t, int which, char *name, int cap, int *size)
{
    const char *spec = t->model ? t->model->look.font[which] : "";
    const char *slash;
    size_t n;
    *size = 0;
    if (!spec[0] && which == GT_FONT_ICON)
        spec = t->font;
    if (!spec[0])
        return 0;
    slash = strchr(spec, '/');
    n = slash ? (size_t)(slash - spec) : strlen(spec);
    if (n == 0 || n >= (size_t)cap)
        return 0;
    memcpy(name, spec, n);
    name[n] = 0;
    if (slash)
        *size = atoi(slash + 1);
    return 1;
}

int gt_look_bar_height(const gtheme *t)
{
    return t->model ? t->model->look.bar_height : 0;
}

/* ---- the v2 reader ---- */
static const char *const gt_managed[] = { "mode", "name", "background", "foreground", "dark_foreground", "dark_background", "accent",
    "green", "red", "darker_background", "lighter_background", "selection", "muted", "inactive", "backdrop", "font", "wbtitle",
    "amarchy-theme", "base", 0 };
static const char *const gt_fontkey[GT_FONTS] = { "screen-font", "title-font", "menu-font", "icon-font", "shell-font" };

typedef struct gt_pass {
    gt_model *m;
    int sec; /* 0 default extras, 1 swatches, 2 groups, 3 elements, 4 look */
} gt_pass;

static void extra_add(gt_model *m, char letter, const char *key, const char *value)
{
    int need = 1 + (int)strlen(key) + 3 + (int)strlen(value) + 1;
    if (m->extralen + need >= GT_EXTRA_MAX) {
        m->warn |= GT_WARN_OVERFLOW;
        return;
    }
    m->extralen += snprintf(m->extra + m->extralen, (size_t)(GT_EXTRA_MAX - m->extralen), "%c%s = %s\n", letter, key, value);
}

static void unquote(const char *v, char *dst, int cap)
{
    int n = 0;
    while (*v == '"' || *v == ' ')
        v++;
    while (v[n] && v[n] != '"' && v[n] != '\r' && v[n] != '\n' && n < cap - 1)
        n++;
    memcpy(dst, v, (size_t)n);
    dst[n] = 0;
}

static void parse_states(gt_model *m, const char *value, gt_ref *dst)
{
    char buf[UC_MAX_VALUE + 1];
    char *seg = buf, *comma;
    int i;
    strncpy(buf, value, UC_MAX_VALUE);
    buf[UC_MAX_VALUE] = 0;
    memset(dst, 0, sizeof(gt_ref) * GT_STATES);
    for (i = 0; i < GT_STATES && seg; i++) {
        comma = strchr(seg, ',');
        if (comma)
            *comma = 0;
        gt_ref_parse(m, seg, &dst[i]);
        seg = comma ? comma + 1 : 0;
    }
}

static int look_row(gt_model *m, const char *key, const char *value)
{
    int i;
#define LN(k, f) if (!cmp_ci(key, k)) { m->look.f = (unsigned char)atoi(value); return 1; }
    LN("title-align", title_align) LN("title-style", title_style) LN("corner-radius", corner_radius)
    LN("border-width", border_width) LN("title-bar-height", bar_height) LN("gadget-size", gadget_size)
    LN("gadget-pad", gadget_pad) LN("scroll-width", scroll_width) LN("knob-min", knob_min) LN("knob-shape", knob_shape)
    LN("glyph-set", glyph_set) LN("gadget-order", gadget_order) LN("ghost-style", ghost_style) LN("hover", hover_on)
    LN("inactive-pct", inactive_pct)
#undef LN
    for (i = 0; i < GT_FONTS; i++)
        if (!cmp_ci(key, gt_fontkey[i])) {
            unquote(value, m->look.font[i], (int)sizeof m->look.font[i]);
            return 1;
        }
    if (!cmp_ci(key, "pointer")) {
        char buf[UC_MAX_VALUE + 1];
        char *seg = buf, *comma;
        strncpy(buf, value, UC_MAX_VALUE);
        buf[UC_MAX_VALUE] = 0;
        for (i = 0; i < 3 && seg; i++) {
            comma = strchr(seg, ',');
            if (comma)
                *comma = 0;
            upconf_hex6(seg, &m->look.pointer[i]);
            seg = comma ? comma + 1 : 0;
        }
        return 1;
    }
    return 0;
}

static int row_cb(void *ud, int lineno, const char *key, const char *value)
{
    gt_pass *p = (gt_pass *)ud;
    gt_model *m = p->m;
    int i;
    (void)lineno;
    switch (p->sec) {
    case 0:
        for (i = 0; gt_managed[i]; i++)
            if (!cmp_ci(key, gt_managed[i]))
                return 0;
        extra_add(m, 'D', key, value);
        break;
    case 1: {
        uc_u32 rgb;
        if (upconf_hex6(value[0] == '"' ? value + 1 : value, &rgb))
            swatch_add(m, key, rgb);
        else
            m->warn |= GT_WARN_BAD_REF;
        break;
    }
    case 2:
        for (i = 0; i < GTG_COUNT; i++)
            if (!cmp_ci(key, gt_group_defs[i].key)) {
                parse_states(m, value, m->group[i]);
                return 0;
            }
        m->warn |= GT_WARN_UNKNOWN_KEY;
        extra_add(m, 'G', key, value);
        break;
    case 3:
        for (i = 0; i < GTE_COUNT; i++)
            if (!cmp_ci(key, gt_elem_defs[i].key)) {
                parse_states(m, value, m->elem[i]);
                return 0;
            }
        m->warn |= GT_WARN_UNKNOWN_KEY;
        extra_add(m, 'E', key, value);
        break;
    default:
        if (!look_row(m, key, value)) {
            m->warn |= GT_WARN_UNKNOWN_KEY;
            extra_add(m, 'L', key, value);
        }
        break;
    }
    return 0;
}

/* g reaches group g0 again through group refs (own state, falling back to normal), or runs past the chain limit */
static int ref_cycles(const gt_model *m, const gt_ref *r, int g0, int state, int d)
{
    const gt_ref *n;
    if (d > GT_EVAL_MAX)
        return 1;
    switch (r->kind) {
    case GT_REF_GROUP:
        if (r->a == g0)
            return 1;
        n = &m->group[r->a][state];
        if (n->kind == GT_REF_NONE)
            n = &m->group[r->a][GT_ST_NORMAL];
        return ref_cycles(m, n, g0, state, d + 1);
    case GT_REF_DARKER:
    case GT_REF_SELECTION:
    case GT_REF_MUTED:
        return ref_cycles(m, &m->nested[r->a], g0, state, d + 1);
    case GT_REF_MIX:
        return ref_cycles(m, &m->nested[r->a], g0, state, d + 1) || ref_cycles(m, &m->nested[r->b], g0, state, d + 1);
    default:
        return 0;
    }
}

void gt_model_parse(const char *buf, long len, gt_model *m)
{
    gtheme t;
    gt_pass p;
    int g, s;
    gt_parse(buf, len, &t); /* fills g_conf; the v1 reading is also the fallback colours */
    if (upconf_int(&g_conf, "default", "amarchy-theme", 1) < 2) {
        gt_model_from_v1(&t, m);
        str_key(m->name, GT_NAME, "name");
        return;
    }
    model_init(m);
    m->version = 2;
    m->light = t.light;
    strcpy(m->backdrop, t.backdrop);
    strcpy(m->font, t.font);
    strcpy(m->wbtitle, t.wbtitle);
    str_key(m->name, GT_NAME, "name");
    str_key(m->base, GT_NAME, "base");
    p.m = m;
    p.sec = 1; /* swatches first: the other sections name them */
    upconf_rows(buf, len, "swatches", row_cb, &p, 0);
    p.sec = 0;
    upconf_rows(buf, len, "default", row_cb, &p, 0);
    for (g = 0; g < GTG_COUNT; g++)
        group_defaults(m, g);
    p.sec = 2;
    upconf_rows(buf, len, "groups", row_cb, &p, 0);
    p.sec = 3;
    upconf_rows(buf, len, "elements", row_cb, &p, 0);
    p.sec = 4;
    upconf_rows(buf, len, "look", row_cb, &p, 0);
    /* a group cycle falls back to the group's default and warns; all members are found first, so every
     * group on the loop is reset, not only the one the scan reaches first */
    {
        unsigned char bad[GTG_COUNT][GT_STATES];
        memset(bad, 0, sizeof bad);
        for (g = 0; g < GTG_COUNT; g++)
            for (s = 0; s < GT_STATES; s++)
                bad[g][s] = (unsigned char)ref_cycles(m, &m->group[g][s], g, s, 0);
        for (g = 0; g < GTG_COUNT; g++)
            for (s = 0; s < GT_STATES; s++)
                if (bad[g][s]) {
                    memset(&m->group[g][s], 0, sizeof m->group[g][s]);
                    if (s == GT_ST_NORMAL)
                        gt_ref_parse(m, gt_group_defs[g].def, &m->group[g][s]);
                    m->warn |= GT_WARN_CYCLE;
                }
    }
}

/* ---- the writer ---- */
typedef struct gt_w {
    char *out;
    long cap, n;
    int bad;
} gt_w;

static void w_put(gt_w *w, const char *s)
{
    long l = (long)strlen(s);
    if (w->bad || w->n + l >= w->cap) {
        w->bad = 1;
        return;
    }
    memcpy(w->out + w->n, s, (size_t)l);
    w->n += l;
    w->out[w->n] = 0;
}

static void w_kv(gt_w *w, const char *key, const char *value)
{
    if (strlen(value) >= UC_MAX_VALUE) {
        w->bad = 1;
        return;
    }
    w_put(w, key);
    w_put(w, " = ");
    w_put(w, value);
    w_put(w, "\n");
}

static void w_color(gt_w *w, const char *key, uc_u32 rgb)
{
    char v[16];
    v[0] = '"';
    hex_text(rgb, v + 1);
    v[8] = '"';
    v[9] = 0;
    w_kv(w, key, v);
}

static void w_text(gt_w *w, const char *key, const char *value)
{
    char v[GT_TITLE + 4];
    snprintf(v, sizeof v, "\"%s\"", value);
    w_kv(w, key, v);
}

static void w_extras(gt_w *w, const gt_model *m, char letter)
{
    const char *p = m->extra;
    while (p < m->extra + m->extralen) {
        const char *e = strchr(p, '\n');
        char line[UC_MAX_VALUE + 64];
        int n = (int)(e - p);
        if (*p == letter && n < (int)sizeof line) {
            memcpy(line, p + 1, (size_t)(n - 1));
            line[n - 1] = 0;
            w_put(w, line);
            w_put(w, "\n");
        }
        p = e + 1;
    }
}

static void w_states(gt_w *w, const gt_model *m, const char *key, const gt_ref *r)
{
    char v[UC_MAX_VALUE + 8], t[UC_MAX_VALUE];
    int last = -1, i, n;
    v[0] = 0;
    for (i = 0; i < GT_STATES; i++)
        if (r[i].kind != GT_REF_NONE)
            last = i;
    if (last < 0)
        return;
    n = 0;
    for (i = 0; i <= last; i++) {
        int l = r[i].kind == GT_REF_NONE ? 0 : gt_ref_text(m, &r[i], t, (int)sizeof t);
        if (l < 0 || n + l + 2 >= UC_MAX_VALUE) {
            w->bad = 1;
            return;
        }
        if (i)
            v[n++] = ',';
        if (l)
            memcpy(v + n, t, (size_t)l);
        n += l;
        v[n] = 0;
    }
    w_kv(w, key, v);
}

static void w_num(gt_w *w, const char *key, unsigned v)
{
    char s[16];
    snprintf(s, sizeof s, "%u", v);
    w_kv(w, key, s);
}

/* the text of m under the theme name `name`: the file name is the identity, never the model's own name */
static long model_save_named(const gt_model *m, const char *name, char *out, long cap)
{
    gt_w w;
    gtheme t;
    int i;
    char v[GT_FONTS * 0 + 128];
    w.out = out;
    w.cap = cap;
    w.n = 0;
    w.bad = 0;
    if (cap > 0)
        out[0] = 0;
    gt_model_to_v1(m, &t);
    w_text(&w, "mode", m->light ? "light" : "dark");
    if (name[0])
        w_text(&w, "name", name);
    w_color(&w, "background", t.bg);
    w_color(&w, "foreground", t.fg);
    w_color(&w, "dark_foreground", t.dim);
    w_color(&w, "dark_background", t.dark_bg);
    w_color(&w, "accent", t.accent);
    w_color(&w, "green", t.ok);
    w_color(&w, "red", t.fail);
    w_color(&w, "darker_background", t.darker_bg);
    w_color(&w, "lighter_background", t.lighter_bg);
    w_color(&w, "selection", t.selection);
    w_color(&w, "muted", t.muted);
    w_color(&w, "inactive", t.inactive);
    if (m->backdrop[0])
        w_text(&w, "backdrop", m->backdrop);
    if (m->font[0])
        w_text(&w, "font", m->font);
    if (m->wbtitle[0])
        w_text(&w, "wbtitle", m->wbtitle);
    w_extras(&w, m, 'D');
    w_kv(&w, "amarchy-theme", "2");
    if (m->base[0])
        w_kv(&w, "base", m->base);
    w_put(&w, "\n[profile swatches]\n");
    for (i = 0; i < m->nswatch; i++) {
        hex_text(m->swatch[i], v);
        w_kv(&w, m->swatch_name[i], v);
    }
    w_extras(&w, m, 'S');
    w_put(&w, "\n[profile groups]\n");
    for (i = 0; i < GTG_COUNT; i++)
        w_states(&w, m, gt_group_defs[i].key, m->group[i]);
    w_extras(&w, m, 'G');
    w_put(&w, "\n[profile elements]\n");
    for (i = 0; i < GTE_COUNT; i++)
        w_states(&w, m, gt_elem_defs[i].key, m->elem[i]);
    w_extras(&w, m, 'E');
    w_put(&w, "\n[profile look]\n");
    for (i = 0; i < GT_FONTS; i++)
        if (m->look.font[i][0])
            w_kv(&w, gt_fontkey[i], m->look.font[i]);
#define LW(k, f) if (m->look.f) w_num(&w, k, m->look.f);
    LW("title-align", title_align) LW("title-style", title_style) LW("corner-radius", corner_radius)
    LW("border-width", border_width) LW("title-bar-height", bar_height) LW("gadget-size", gadget_size)
    LW("gadget-pad", gadget_pad) LW("scroll-width", scroll_width) LW("knob-min", knob_min) LW("knob-shape", knob_shape)
    LW("glyph-set", glyph_set) LW("gadget-order", gadget_order) LW("ghost-style", ghost_style) LW("hover", hover_on)
#undef LW
    w_num(&w, "inactive-pct", m->look.inactive_pct);
    if (m->look.pointer[0] || m->look.pointer[1] || m->look.pointer[2]) {
        char a[8], b[8], c[8];
        hex_text(m->look.pointer[0], a);
        hex_text(m->look.pointer[1], b);
        hex_text(m->look.pointer[2], c);
        snprintf(v, sizeof v, "%s,%s,%s", a, b, c);
        w_kv(&w, "pointer", v);
    }
    w_extras(&w, m, 'L');
    if (w.bad || w.n >= UC_MAX_FILE)
        return -1;
    return w.n;
}

long gt_model_save_text(const gt_model *m, char *out, long cap)
{
    return model_save_named(m, m->name, out, cap);
}

/* ---- TE-1.6: the model on a gtheme ---- */
int gt_model_attach(gtheme *t, const char *buf, long len)
{
    gt_model *m = (gt_model *)GT_ALLOC(sizeof *m);
    if (!m)
        return 0;
    gt_model_parse(buf, len, m);
    if (t->model)
        GT_FREE(t->model);
    t->model = m;
    return 1;
}

void gt_load_model(gt_read_fn rd, void *ctx, const char *name, gtheme *out)
{
    static const char *const dirs[2] = { GT_USER_DIR, GT_SHIPPED_DIR };
    char path[160];
    int d;
    gt_builtin(out);
    for (d = 0; d < 2; d++) {
        long n;
        if (!gt_path(dirs[d], name, path, sizeof path))
            return;
        n = rd(ctx, path, g_text, (long)sizeof g_text);
        if (n >= 0) {
            gt_parse(g_text, n, out);
            strncpy(out->name, name, GT_NAME - 1);
            out->name[GT_NAME - 1] = 0;
            gt_model_attach(out, g_text, n);
            if (out->model)
                strcpy(out->model->name, out->name);
            return;
        }
    }
}

void gt_free(gtheme *t)
{
    if (t->model)
        GT_FREE(t->model);
    t->model = 0;
}


/* ====== TE-3: Save, Save As, Duplicate, Import, Export, Delete ====== */
unsigned long gt_dos_edit_applies;

static char g_save[UC_MAX_FILE];
static gt_model g_smodel;

static void why_put(char *why, int cap, int step)
{
    if (why && cap > 0)
        gt_save_text(step, 0, 0, why, cap);
}

static void why_msg(char *why, int cap, const char *msg)
{
    if (why && cap > 0)
        snprintf(why, (size_t)cap, "the file is not a theme: %s", msg);
}

static int gt_file_path(int env, const char *name, char *out, int cap)
{
    return gt_path(env ? GT_ENV_DIR : GT_USER_DIR, name, out, cap);
}

void gt_custom_name(const char *base, int (*taken)(void *ctx, const char *name), void *ctx, char *out, int cap)
{
    char b[GT_NAME];
    int n;
    strncpy(b, base, sizeof b - 1);
    b[sizeof b - 1] = 0;
    if ((int)strlen(b) > GT_NAME - 1 - 11)  /* room for " Custom 99" */
        b[GT_NAME - 1 - 11] = 0;
    snprintf(out, (size_t)cap, "%s Custom", b);
    for (n = 2; taken && taken(ctx, out) && n < 100; n++)
        snprintf(out, (size_t)cap, "%s Custom %d", b, n);
}

int gt_check_text(const char *buf, long len, char *msg, int cap)
{
    const char *why = 0;
    long i;
    int eq = 0;
    if (len <= 0)
        why = "it is empty";
    for (i = 0; !why && i < len; i++)
        if (buf[i] == '=')
            eq = 1;
    if (!why && !eq)
        why = "it has no key = value line";
    if (!why) {
        gt_model_parse(buf, len, &g_smodel);
        if (g_smodel.warn & GT_WARN_BAD_REF)
            why = "a colour reference names an unknown swatch or group";
        else if (g_smodel.warn & GT_WARN_CYCLE)
            why = "groups refer to each other in a loop";
        else if (g_smodel.warn & GT_WARN_OVERFLOW)
            why = "it holds more swatches or references than a theme may";
    }
    if (why && msg && cap > 0) {
        strncpy(msg, why, (size_t)cap - 1);
        msg[cap - 1] = 0;
    }
    return why == 0;
}

static int user_has(const prefs_fs *fs, const char *name)
{
    char p[160];
    return gt_file_path(0, name, p, sizeof p) && fs->exists(fs->ctx, p);
}

static int shipped_has(const prefs_fs *fs, const char *name)
{
    char p[160];
    return gt_path(GT_SHIPPED_DIR, name, p, sizeof p) && fs->exists(fs->ctx, p);
}

/* the text into the user file, then the ENV: copy (the ENVARC: file is the one that must succeed) */
static int put_text(const prefs_fs *fs, const char *name, const char *text, long len, char *why, int cap)
{
    char p[160];
    int env;
    for (env = 0; env < 2; env++) {
        if (!gt_file_path(env, name, p, sizeof p) || !fs->write_file(fs->ctx, p, text, len)) {
            if (!env) {
                why_put(why, cap, GT_SAVE_WRITE);
                return 0;
            }
        }
    }
    return 1;
}

int gt_save(const prefs_fs *fs, const char *name, const gt_model *m, char *why, int cap)
{
    long n;
    if (!gt_name_ok(name)) {
        why_put(why, cap, GT_SAVE_NAME);
        return 0;
    }
    if (shipped_has(fs, name) && !user_has(fs, name)) {
        why_put(why, cap, GT_SAVE_SHIPPED);
        return 0;
    }
    n = model_save_named(m, name, g_save, (long)sizeof g_save);
    if (n < 0) {
        why_put(why, cap, GT_SAVE_WRITE);
        return 0;
    }
    return put_text(fs, name, g_save, n, why, cap);
}

int gt_save_as(const prefs_fs *fs, const char *name, const gt_model *m, char *why, int cap)
{
    if (gt_name_ok(name) && (user_has(fs, name) || shipped_has(fs, name))) {
        why_put(why, cap, shipped_has(fs, name) && !user_has(fs, name) ? GT_SAVE_SHIPPED : GT_SAVE_EXISTS);
        return 0;
    }
    return gt_save(fs, name, m, why, cap);
}

/* the file text of theme name: the user's file, else the shipped one; -1 when neither */
static long theme_text(gt_read_fn rd, void *ctx, const char *name, char *buf, long cap)
{
    static const char *const dirs[2] = { GT_USER_DIR, GT_SHIPPED_DIR };
    char p[160];
    int d;
    for (d = 0; d < 2; d++) {
        long n;
        if (!gt_path(dirs[d], name, p, sizeof p))
            return -1;
        n = rd(ctx, p, buf, cap);
        if (n >= 0)
            return n;
    }
    return -1;
}

int gt_duplicate(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *from, const char *to, char *why, int cap)
{
    if (!gt_name_ok(from) || !gt_preset_model(rd, ctx, from, &g_smodel)) {
        why_put(why, cap, GT_SAVE_MISSING);
        return 0;
    }
    strncpy(g_smodel.base, from, GT_NAME - 1);
    g_smodel.base[GT_NAME - 1] = 0;
    return gt_save_as(fs, to, &g_smodel, why, cap);
}

int gt_import(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *src, const char *name, char *why, int cap)
{
    char msg[80];
    long n;
    if (!gt_name_ok(name)) {
        why_put(why, cap, GT_SAVE_NAME);
        return 0;
    }
    n = rd(ctx, src, g_save, (long)sizeof g_save);
    if (n < 0) {
        why_msg(why, cap, "it could not be read");
        return 0;
    }
    if (!gt_check_text(g_save, n, msg, sizeof msg)) {
        why_msg(why, cap, msg);
        return 0;
    }
    if (user_has(fs, name) || shipped_has(fs, name)) {
        why_put(why, cap, shipped_has(fs, name) && !user_has(fs, name) ? GT_SAVE_SHIPPED : GT_SAVE_EXISTS);
        return 0;
    }
    return put_text(fs, name, g_save, n, why, cap);
}

int gt_export(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *name, const char *dst, char *why, int cap)
{
    long n = gt_name_ok(name) ? theme_text(rd, ctx, name, g_save, (long)sizeof g_save) : -1;
    if (n < 0) {
        why_put(why, cap, GT_SAVE_MISSING);
        return 0;
    }
    if (!fs->write_file(fs->ctx, dst, g_save, n)) {
        why_put(why, cap, GT_SAVE_WRITE);
        return 0;
    }
    return 1;
}

int gt_delete_user(const prefs_fs *fs, const char *name, char *why, int cap)
{
    char p[160];
    if (!gt_name_ok(name) || !user_has(fs, name)) {
        why_put(why, cap, GT_SAVE_MISSING);
        return 0;
    }
    gt_file_path(0, name, p, sizeof p);
    if (!fs->remove(fs->ctx, p)) {
        why_put(why, cap, GT_SAVE_WRITE);
        return 0;
    }
    if (gt_file_path(1, name, p, sizeof p))
        fs->remove(fs->ctx, p);
    return 1;
}

int gt_save_edit(const prefs_fs *fs, const char *name, const gt_model *m, const char *current, int (*apply)(const char *name),
                 char *why, int cap)
{
    if (!gt_save(fs, name, m, why, cap))
        return 0;
    if (current && !cmp_ci(name, current) && apply) {
        gt_dos_edit_applies++;
        apply(name);
    }
    return 1;
}

unsigned long gt_dos_reset_prompts;

int gt_save_editor(const prefs_fs *fs, const char *name, const gt_model *m, const char *current, int (*apply)(const char *name),
                   unsigned (*bound)(const gt_model *running, const gt_model *neu), void (*ask)(unsigned mask), char *why, int cap)
{
    unsigned mask = 0;
    if (current && !cmp_ci(name, current) && bound && gt_ed.open && !cmp_ci(name, gt_ed.name))
        mask = bound(&gt_ed.saved, m);       /* before gt_save: the file on disk is still the running one */
    if (!gt_save_edit(fs, name, m, current, apply, why, cap))
        return 0;
    if (mask && ask) {
        gt_dos_reset_prompts++;
        ask(mask);
    }
    return 1;
}

/* ---- TE-4: the edit buffer and the editor's rows ---- */
#include <stddef.h>

gt_edit gt_ed;
unsigned long gte_edits;
void (*gte_live)(const gt_model *m);
unsigned long gte_live_sends;

static void ed_touch(void)
{
    gte_edits++;
    if (gte_live && gt_ed.open) {
        gte_live_sends++;
        gte_live(&gt_ed.m);
    }
}

static int any_override(const gt_model *m)
{
    int e, s;
    for (e = 0; e < GTE_COUNT; e++)
        for (s = 0; s < GT_STATES; s++)
            if (m->elem[e][s].kind != GT_REF_NONE)
                return 1;
    return 0;
}

void gte_open(const gt_model *m, const char *name)
{
    gt_ed.m = *m;
    gt_ed.saved = *m;
    gt_ed.open = 1;
    gt_ed.advanced = any_override(m);
    strncpy(gt_ed.name, name ? name : "", GT_NAME - 1);
    gt_ed.name[GT_NAME - 1] = 0;
}

int gte_action_plan(int aux, int editor_open)
{
    switch (aux) {
    case GTA_OPEN:
        return GTP_LEAVE | GTP_NAME;
    case GTA_RESET_BASE:
        return GTP_LEAVE;
    case GTA_SAVE:
    case GTA_APPLY:
        return editor_open ? 0 : GTP_SKIP;
    case GTA_SAVE_AS:
    case GTA_DUPLICATE:
        return editor_open ? GTP_NAME : GTP_SKIP;
    case GTA_ADD_SWATCH:
    case GTA_RENAME_SWATCH:
    case GTA_REMOVE_SWATCH:
    case GTA_IMPORT:
        return GTP_NAME;
    }
    return 0;
}

int gte_open_from(gt_read_fn rd, void *ctx, const char *name)
{
    gtheme t;
    memset(&t, 0, sizeof t);
    gt_load_model(rd, ctx, name, &t);
    if (!t.model)
        return 0;
    gte_open(t.model, name);
    gt_free(&t);
    return 1;
}

/* ---- TE-8.1 presets ---- */
int gt_preset_model(gt_read_fn rd, void *ctx, const char *name, gt_model *out)
{
    long n = gt_name_ok(name) ? theme_text(rd, ctx, name, g_save, (long)sizeof g_save) : -1;
    if (n >= 0)
        gt_model_parse(g_save, n, out);
    else if (gt_name_ok(name) && !strcmp(name, GT_DEFAULT)) {
        static gtheme bt;
        gt_builtin(&bt);
        gt_model_from_v1(&bt, out);
    } else
        return 0;
    strncpy(out->name, name, GT_NAME - 1);
    out->name[GT_NAME - 1] = 0;
    return 1;
}

int gte_reset_to_base(gt_read_fn rd, void *ctx)
{
    char name[GT_NAME], base[GT_NAME];
    if (!gt_ed.open || !gt_ed.m.base[0] || !gt_preset_model(rd, ctx, gt_ed.m.base, &g_smodel))
        return 0;
    strcpy(name, gt_ed.m.name);
    strcpy(base, gt_ed.m.base);
    gt_ed.m = g_smodel;
    strcpy(gt_ed.m.name, name);
    strcpy(gt_ed.m.base, base);
    gt_ed.advanced = any_override(&gt_ed.m);
    ed_touch();
    return 1;
}

/* ---- TE-8.2 contrast pairs: the text side against the element it sits on ---- */
static const struct { unsigned char text, ts, ground, gs, kind; } gt_cpair_def[] = {
    { GTE_TITLE_TEXT, GT_ST_NORMAL, GTE_TITLE_BAR, GT_ST_NORMAL, GTC_TEXT },
    { GTE_TITLE_TEXT_INACTIVE, GT_ST_NORMAL, GTE_TITLE_BAR_INACTIVE, GT_ST_NORMAL, GTC_DIM },
    { GTE_MENU_TEXT, GT_ST_NORMAL, GTE_MENU_BACKGROUND, GT_ST_NORMAL, GTC_TEXT },
    { GTE_MENU_TEXT, GT_ST_DISABLED, GTE_MENU_BACKGROUND, GT_ST_NORMAL, GTC_MUTED },
    { GTE_MENU_SELECTED_TEXT, GT_ST_NORMAL, GTE_MENU_SELECTION, GT_ST_NORMAL, GTC_TEXT },
    { GTE_MENU_SELECTION, GT_ST_NORMAL, GTE_MENU_BACKGROUND, GT_ST_NORMAL, GTC_SEL },
    { GTE_BUTTON_TEXT, GT_ST_NORMAL, GTE_BUTTON_FACE, GT_ST_NORMAL, GTC_TEXT },
    { GTE_BUTTON_TEXT, GT_ST_DISABLED, GTE_BUTTON_FACE, GT_ST_DISABLED, GTC_MUTED },
    { GTE_SCROLL_KNOB, GT_ST_NORMAL, GTE_SCROLL_TRACK, GT_ST_NORMAL, GTC_SEL },
    { GTE_HELPER_TEXT, GT_ST_NORMAL, GTE_HELPER_BACKGROUND, GT_ST_NORMAL, GTC_TEXT },
    { GTE_TERMINAL_FOREGROUND, GT_ST_NORMAL, GTE_TERMINAL_BACKGROUND, GT_ST_NORMAL, GTC_TEXT },
    { GTE_ICON_LABEL_TEXT, GT_ST_NORMAL, GTE_DESKTOP_BACKDROP, GT_ST_NORMAL, GTC_TEXT },
};
#define GT_NCPAIR ((int)(sizeof gt_cpair_def / sizeof gt_cpair_def[0]))

int gt_contrast_pairs(const gt_model *m, gt_cpair out[GT_CPAIRS])
{
    int i;
    for (i = 0; i < GT_NCPAIR && i < GT_CPAIRS; i++) {
        gt_cpair *p = &out[i];
        p->text = gt_cpair_def[i].text;
        p->text_state = gt_cpair_def[i].ts;
        p->ground = gt_cpair_def[i].ground;
        p->ground_state = gt_cpair_def[i].gs;
        p->kind = gt_cpair_def[i].kind;
        p->floor = p->kind == GTC_SEL ? GT_FLOOR_SEL : p->kind == GTC_MUTED ? GT_FLOOR_MUTED : p->kind == GTC_DIM ? GT_FLOOR_DIM : GT_FLOOR_TEXT;
        p->contrast = gt_contrast(gt_resolve_rgb(m, p->text, p->text_state), gt_resolve_rgb(m, p->ground, p->ground_state));
        p->below = p->contrast < p->floor;
    }
    return i;
}

int gt_contrast_warnings(const gt_model *m, int *first)
{
    gt_cpair p[GT_CPAIRS];
    int i, n = gt_contrast_pairs(m, p), w = 0;
    if (first)
        *first = -1;
    for (i = 0; i < n; i++)
        if (p[i].below) {
            if (first && *first < 0)
                *first = i;
            w++;
        }
    return w;
}

int gt_contrast_elem_warns(const gt_model *m, int elem)
{
    gt_cpair p[GT_CPAIRS];
    int i, n = gt_contrast_pairs(m, p);
    for (i = 0; i < n; i++)
        if (p[i].below && p[i].text == elem)
            return 1;
    return 0;
}

/* ---- TE-8.3 pen budget ---- */
static int is_icon_elem(int e)
{
    return e == GTE_ICON_FOREGROUND || e == GTE_ICON_ACCENT || e == GTE_ICON_MUTED || e == GTE_ICON_SELECTION_PLATE;
}

/* the walk behind the budget and the apply plan (one source): the distinct normal-state colours of the 36 elements, and of those the
 * ones no role pen carries (and, from depth 4, no icon element) in need[]. Returns the distinct count in *nseen, the needing count. */
static int needing_colours(const gt_model *m, int depth, const gtheme *t, uc_u32 *need, int *nseen_out)
{
    uc_u32 seen[GTE_COUNT];
    int e, j, pen, nseen = 0, nneed = 0;
    for (e = 0; e < GTE_COUNT; e++) {
        uc_u32 c = gt_resolve_rgb(m, e, GT_ST_NORMAL) & 0xFFFFFFUL;
        for (j = 0; j < nseen && seen[j] != c; j++)
            ;
        if (j < nseen)
            continue;
        seen[nseen++] = c;
        if (depth >= 4 && is_icon_elem(e))
            continue;               /* held icon pens carry these */
        for (pen = 0; pen < DK_ROLES; pen++)
            if (pen != DK_GREEN && pen != DK_BACKDROP && (desk_role(t, pen) & 0xFFFFFFUL) == c)
                break;
        if (pen == DK_ROLES)
            need[nneed++] = c;
    }
    *nseen_out = nseen;
    return nneed;
}

void gt_pen_budget(const gt_model *m, int depth, gt_budget *b)
{
    static gtheme t;
    uc_u32 need[GTE_COUNT];
    int j, g, nseen = 0, nneed;
    memset(b, 0, sizeof *b);
    for (g = 0; g < GTG_COUNT; g++) {
        b->grp_pen[g] = -1;
        b->grp_with[g] = (signed char)g;
    }
    b->depth = depth;
    gt_model_to_v1(m, &t);
    nneed = needing_colours(m, depth, &t, need, &nseen);
    b->colours = nseen;
    b->shared = GTE_COUNT - nseen;
    b->needing = nneed;
    if (depth >= 15) {
        b->exact = 1;
        return;
    }
    b->pens = depth >= 8 ? 256 : 1 << (depth < 1 ? 1 : depth);
    b->held = depth <= 2 ? 4 : depth == 3 ? 8 : depth == 4 ? 16 : depth == 5 ? 19 : 27;
    b->free = b->pens - b->held;
    b->fold = depth <= 3 ? nneed : (nneed > b->free ? nneed - b->free : 0);
    if (depth <= 3)
        for (g = 0; g < GTG_COUNT; g++) {
            b->grp_pen[g] = (signed char)gt_nearest_role_pen(&t, gt_group_rgb(m, g, GT_ST_NORMAL), depth);
            for (j = 0; j < g; j++)
                if (b->grp_pen[j] == b->grp_pen[g]) {
                    b->grp_with[g] = (signed char)j;
                    break;
                }
        }
}

void gt_budget_text(const gt_budget *b, char *out, int n)
{
    if (b->exact)
        snprintf(out, (size_t)n, "Truecolour: exact colours, %d elements share a colour", b->shared);
    else if (b->depth <= 3)
        snprintf(out, (size_t)n, "Screen %d colours: %d of %d colours fold to the nearest of %d pens", b->pens, b->needing, b->colours, b->held);
    else if (b->fold > 0)
        snprintf(out, (size_t)n, "Screen %d colours: %d of %d free pens, %d take the nearest pen", b->pens, b->needing - b->fold, b->free, b->fold);
    else
        snprintf(out, (size_t)n, "Screen %d colours: %d of %d free pens, %d elements share a colour", b->pens, b->needing, b->free, b->shared);
}

int gt_budget_share_text(const gt_budget *b, int g, char *out, int n)
{
    if (n > 0)
        out[0] = 0;
    if (g < 0 || g >= GTG_COUNT || b->grp_with[g] == g)
        return 0;
    snprintf(out, (size_t)n, "Shares a pen with %s", gt_group_defs[b->grp_with[g]].label);
    return 1;
}

int gte_dirty(void)
{
    return gt_ed.open && memcmp(&gt_ed.m, &gt_ed.saved, sizeof gt_ed.m) != 0;
}

void gte_revert(void)
{
    gt_ed.m = gt_ed.saved;
    ed_touch();
}

void gte_saved(void)
{
    gt_ed.saved = gt_ed.m;
}

int gte_swatch_count(void)
{
    return gt_ed.m.nswatch;
}

#define ED_SLOTS (GTG_COUNT * GT_STATES + GTE_COUNT * GT_STATES + GT_NESTED_MAX)
/* every ref slot of the model in one walk: groups, elements, nested */
static int ref_slots(gt_model *m, gt_ref **out)
{
    int g, e, s, n = 0, i;
    for (g = 0; g < GTG_COUNT; g++)
        for (s = 0; s < GT_STATES; s++)
            out[n++] = &m->group[g][s];
    for (e = 0; e < GTE_COUNT; e++)
        for (s = 0; s < GT_STATES; s++)
            out[n++] = &m->elem[e][s];
    for (i = 0; i < m->nnested; i++)
        out[n++] = &m->nested[i];
    return n;
}

static const char *const ed_state_name[GT_STATES] = { "Normal", "Hover", "Pressed", "Disabled" };

int gte_swatch_users(int sw, char *out, int cap)
{
    gt_model *m = &gt_ed.m;
    int g, e, s, i, n = 0;
    if (cap > 0)
        out[0] = 0;
    for (g = 0; g < GTG_COUNT; g++)
        for (s = 0; s < GT_STATES; s++)
            if (m->group[g][s].kind == GT_REF_SWATCH && m->group[g][s].a == sw) {
                n++;
                if ((int)strlen(out) + 48 < cap)
                    snprintf(out + strlen(out), (size_t)(cap - (int)strlen(out)), "%s%s %s", n > 1 ? ", " : "", gt_group_defs[g].label, ed_state_name[s]);
            }
    for (e = 0; e < GTE_COUNT; e++)
        for (s = 0; s < GT_STATES; s++)
            if (m->elem[e][s].kind == GT_REF_SWATCH && m->elem[e][s].a == sw) {
                n++;
                if ((int)strlen(out) + 48 < cap)
                    snprintf(out + strlen(out), (size_t)(cap - (int)strlen(out)), "%s%s %s", n > 1 ? ", " : "", gt_elem_defs[e].label, ed_state_name[s]);
            }
    for (i = 0; i < m->nnested; i++)
        if (m->nested[i].kind == GT_REF_SWATCH && m->nested[i].a == sw)
            n++;
    return n;
}

static int hex_parse(const char *t, uc_u32 *v)
{
    char *end;
    unsigned long x;
    if (*t == '#')
        t++;
    if (strlen(t) != 6)
        return 0;
    x = strtoul(t, &end, 16);
    if (*end)
        return 0;
    *v = (uc_u32)x;
    return 1;
}

int gte_swatch_set(int sw, const char *text)
{
    uc_u32 v;
    if (sw < 0 || sw >= gt_ed.m.nswatch || !hex_parse(text, &v))
        return GTR_BAD;
    gt_ed.m.swatch[sw] = v;
    ed_touch();
    return GTR_OK;
}

static int swatch_name_ok(const char *name, int except)
{
    int i;
    if (!*name || strlen(name) >= GT_SWATCH_NAME)
        return GTR_NAME;
    for (i = 0; name[i]; i++)
        if (!is_name(name[i]))
            return GTR_NAME;
    for (i = 0; i < gt_ed.m.nswatch; i++)
        if (i != except && !cmp_ci(gt_ed.m.swatch_name[i], name))
            return GTR_EXISTS;
    return GTR_OK;
}

int gte_swatch_add(const char *name, uc_u32 rgb)
{
    int r = swatch_name_ok(name, -1);
    if (r)
        return r;
    if (gt_ed.m.nswatch >= GT_SWATCH_MAX)
        return GTR_FULL;
    strcpy(gt_ed.m.swatch_name[gt_ed.m.nswatch], name);
    gt_ed.m.swatch[gt_ed.m.nswatch++] = rgb;
    ed_touch();
    return GTR_OK;
}

int gte_swatch_find(const char *name)
{
    int i;
    for (i = 0; i < gt_ed.m.nswatch; i++)
        if (!cmp_ci(gt_ed.m.swatch_name[i], name))
            return i;
    return -1;
}

int gte_swatch_rename_name(const char *old, const char *name)
{
    return gte_swatch_rename(gte_swatch_find(old), name);
}

int gte_swatch_remove_name(const char *old, char *why, int cap)
{
    return gte_swatch_remove(gte_swatch_find(old), why, cap);
}

int gte_swatch_rename(int sw, const char *name)
{
    int r;
    if (sw < 0 || sw >= gt_ed.m.nswatch)
        return GTR_BAD;
    r = swatch_name_ok(name, sw);
    if (r)
        return r;
    strcpy(gt_ed.m.swatch_name[sw], name);
    ed_touch();
    return GTR_OK;
}

int gte_swatch_remove(int sw, char *why, int cap)
{
    gt_ref *slot[ED_SLOTS];
    char users[240];
    int n, i;
    if (sw < 0 || sw >= gt_ed.m.nswatch)
        return GTR_BAD;
    if (gte_swatch_users(sw, users, (int)sizeof users)) {
        if (why && cap > 0)
            snprintf(why, (size_t)cap, "%s is used by %s", gt_ed.m.swatch_name[sw], users[0] ? users : "a derived colour");
        return GTR_IN_USE;
    }
    for (i = sw; i < gt_ed.m.nswatch - 1; i++) {
        strcpy(gt_ed.m.swatch_name[i], gt_ed.m.swatch_name[i + 1]);
        gt_ed.m.swatch[i] = gt_ed.m.swatch[i + 1];
    }
    gt_ed.m.nswatch--;
    gt_ed.m.swatch_name[gt_ed.m.nswatch][0] = 0;
    gt_ed.m.swatch[gt_ed.m.nswatch] = 0;
    n = ref_slots(&gt_ed.m, slot);
    for (i = 0; i < n; i++)
        if (slot[i]->kind == GT_REF_SWATCH && slot[i]->a > sw)
            slot[i]->a--;
    ed_touch();
    return GTR_OK;
}

int gte_group_set(int group, int state, const char *text)
{
    gt_ref r;
    if (group < 0 || group >= GTG_COUNT || state < 0 || state >= GT_STATES)
        return GTR_BAD;
    if (!gt_ref_parse(&gt_ed.m, text, &r))
        return GTR_BAD;
    if (state == GT_ST_NORMAL && r.kind == GT_REF_NONE)
        return GTR_BAD; /* a group always has its normal colour */
    gt_ed.m.group[group][state] = r;
    ed_touch();
    return GTR_OK;
}

int gte_elem_set(int elem, int state, const char *text)
{
    gt_ref r;
    if (elem < 0 || elem >= GTE_COUNT || state < 0 || state >= GT_STATES)
        return GTR_BAD;
    if (!gt_ref_parse(&gt_ed.m, text, &r))
        return GTR_BAD;
    gt_ed.m.elem[elem][state] = r;
    ed_touch();
    return GTR_OK;
}

void gte_elem_clear(int elem)
{
    memset(gt_ed.m.elem[elem], 0, sizeof gt_ed.m.elem[elem]);
    ed_touch();
}

int gte_elem_overridden(int elem)
{
    int s;
    for (s = 0; s < GT_STATES; s++)
        if (gt_ed.m.elem[elem][s].kind != GT_REF_NONE)
            return 1;
    return 0;
}

int gte_group_overridden(int group)
{
    int e;
    for (e = 0; e < GTE_COUNT; e++)
        if (gt_elem_defs[e].group == group && gte_elem_overridden(e))
            return 1;
    return 0;
}

/* the next colour after cur: each swatch by name, then Darker, Selection and Muted of the surface group (a hand edit
 * of the text gives any other form). dir +1 or -1. */
int gte_ref_step(const gt_ref *cur, int dir, char *out, int cap)
{
    static const char *const fn[3] = { "darker", "selection", "muted" };
    int n = gt_ed.m.nswatch, total = n + 3, at, next;
    if (cur->kind == GT_REF_SWATCH)
        at = cur->a;
    else if (cur->kind == GT_REF_DARKER)
        at = n;
    else if (cur->kind == GT_REF_SELECTION)
        at = n + 1;
    else if (cur->kind == GT_REF_MUTED)
        at = n + 2;
    else
        at = dir > 0 ? -1 : 0;
    next = ((at + dir) % total + total) % total;
    if (next < n)
        snprintf(out, (size_t)cap, "%s", gt_ed.m.swatch_name[next]);
    else
        snprintf(out, (size_t)cap, "%s(group:%s)", fn[next - n], gt_group_defs[GTG_SURFACE].key);
    return next;
}

/* ---- the rows ---- */
#define ER_MAX 512
#define ER_LABEL 44
#define ER_INFO_SHARE 100 /* TE-8.3: + group id, the share line of that group on a shallow screen */
#define ER_INFO_BASE 1
#define ER_INFO_CONTRAST 2
#define ER_INFO_PENS 3
static sd_setting er_row[ER_MAX];
static char er_label[ER_MAX][ER_LABEL];
static char er_key[ER_MAX][24];
static int er_n;

static const sd_value v_mode[2] = { { "simple", "Simple" }, { "advanced", "Advanced" } };
static const sd_value v_align[3] = { { "0", "Left" }, { "1", "Centre" }, { "2", "Right" } };
static const sd_value v_style[2] = { { "0", "Plain" }, { "1", "Bold" } };
static const sd_value v_shape[2] = { { "0", "Square" }, { "1", "Rounded" } };
static const sd_value v_order[2] = { { "0", "Left" }, { "1", "Right" } };
static const sd_value v_ghost[3] = { { "0", "Dim" }, { "1", "Stipple" }, { "2", "Both" } };
static const sd_value v_hover[2] = { { "0", "Off" }, { "1", "On" } };
static const sd_value v_scrdef[3] = { { "all", "All Screens" }, { "workbench", "Workbench Only" }, { "none", "None" } };

typedef struct look_def {
    const char *key, *label;
    int page, kind;
    const sd_value *vals;
    int nvals;
    size_t off;
    long max;
} look_def;
#define LD(k, l, pg, kd, v, nv, f, mx) { k, l, pg, kd, v, nv, offsetof(gt_look, f), mx }
static const look_def look_defs[] = {
    LD("title-align", "Title Alignment", PG_PAGE_THEME_TYPE, SD_CYCLE, v_align, 3, title_align, 2),
    LD("title-style", "Title Style", PG_PAGE_THEME_TYPE, SD_CYCLE, v_style, 2, title_style, 1),
    LD("corner-radius", "Corner Radius", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, corner_radius, 16),
    LD("border-width", "Border Width", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, border_width, 8),
    LD("title-bar-height", "Title Bar Height", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, bar_height, 64),
    LD("gadget-size", "Gadget Size", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, gadget_size, 64),
    LD("gadget-pad", "Gadget Padding", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, gadget_pad, 16),
    LD("scroll-width", "Scroll Bar Width", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, scroll_width, 64),
    LD("knob-min", "Knob Minimum Length", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, knob_min, 64),
    LD("knob-shape", "Knob Shape", PG_PAGE_THEME_SHAPE, SD_CYCLE, v_shape, 2, knob_shape, 1),
    LD("glyph-set", "Gadget Glyph Set", PG_PAGE_THEME_SHAPE, SD_INT, 0, 0, glyph_set, 8),
    LD("gadget-order", "Gadget Order", PG_PAGE_THEME_SHAPE, SD_CYCLE, v_order, 2, gadget_order, 1),
    LD("ghost-style", "Ghost Style", PG_PAGE_THEME_STATES, SD_CYCLE, v_ghost, 3, ghost_style, 2),
    LD("hover", "Hover Highlight", PG_PAGE_THEME_STATES, SD_CYCLE, v_hover, 2, hover_on, 1),
    LD("inactive-pct", "Inactive Window Dimming", PG_PAGE_THEME_STATES, SD_INT, 0, 0, inactive_pct, 100)
};
#define N_LOOK ((int)(sizeof look_defs / sizeof look_defs[0]))
/* No label = no editor row: font.prefs has no title-font chunk, so the title slot (kept in the model, theme file and wbreset) is not offered. */
static const char *const font_label[GT_FONTS] = { "Screen Font", 0, "Menu Font", "Icon Font", "Shell Font" };

static sd_setting *er_add(int page, int kind, const char *key, const char *label)
{
    sd_setting *s;
    if (er_n >= ER_MAX)
        return &er_row[ER_MAX - 1];
    s = &er_row[er_n];
    memset(s, 0, sizeof *s);
    s->id = SDI_COUNT + er_n;
    s->page = page;
    s->kind = kind;
    s->store = SD_GTHEME;
    s->ftype = SDF_NONE;
    s->def = "";
    snprintf(er_key[er_n], sizeof er_key[0], "%s", key);
    s->key = er_key[er_n];
    snprintf(er_label[er_n], ER_LABEL, "%s", label);
    s->label = er_label[er_n];
    er_n++;
    return s;
}

/* TE-4.5: a row that already exists (Amarchy prefs, Prefs palette) shown again on a theme page. The copy keeps the store, the
 * field and the values, so its edits go the way the original row's do; only the id, page, label and help are its own. */
static sd_setting *er_reuse(int page, const sd_setting *src, const char *label, const char *help)
{
    sd_setting *s = er_add(page, src->kind, src->key ? src->key : "", label);
    int id = s->id;
    const char *key = s->key, *lab = s->label;
    *s = *src;
    s->id = id;
    s->page = page;
    s->key = key;
    s->label = lab;
    if (help)
        s->help = help;
    return s;
}

static void er_build(void)
{
    int i, g, e, st;
    char k[24], l[ER_LABEL];
    sd_setting *s;
    if (er_n)
        return;
    s = er_add(PG_PAGE_THEME, SD_CYCLE, "mode", "Mode");
    s->values = v_mode;
    s->nvalues = 2;
    s->aux = -1;
    er_add(PG_PAGE_THEME, SD_INFO, "base", "Base Theme")->aux = ER_INFO_BASE;
    er_add(PG_PAGE_THEME, SD_INFO, "contrast", "Contrast")->aux = ER_INFO_CONTRAST;
    er_add(PG_PAGE_THEME, SD_INFO, "pens", "Pen Budget")->aux = ER_INFO_PENS;
    for (g = 0; g < GTG_COUNT; g++) {
        snprintf(l, sizeof l, "Pen Share %s", gt_group_defs[g].label);
        snprintf(k, sizeof k, "penshare-%s", gt_group_defs[g].key);
        er_add(PG_PAGE_THEME, SD_INFO, k, l)->aux = ER_INFO_SHARE + g;
    }
    er_add(PG_PAGE_THEME, SD_GACTION, "open", "Edit Theme...")->aux = GTA_OPEN;
    er_add(PG_PAGE_THEME, SD_GACTION, "save", "Save")->aux = GTA_SAVE;
    er_add(PG_PAGE_THEME, SD_GACTION, "saveas", "Save As...")->aux = GTA_SAVE_AS;
    er_add(PG_PAGE_THEME, SD_GACTION, "revert", "Revert")->aux = GTA_REVERT;
    er_add(PG_PAGE_THEME, SD_GACTION, "apply", "Apply to Desktop")->aux = GTA_APPLY;
    for (i = 0; i < GT_SWATCH_MAX; i++) {
        snprintf(k, 24, "swatch%d", i);
        er_add(PG_PAGE_THEME_SWATCHES, SD_SWATCH, k, "Swatch")->aux = i;
    }
    er_add(PG_PAGE_THEME_SWATCHES, SD_GACTION, "addswatch", "Add Swatch...")->aux = GTA_ADD_SWATCH;
    er_add(PG_PAGE_THEME_SWATCHES, SD_GACTION, "renameswatch", "Rename Swatch...")->aux = GTA_RENAME_SWATCH;
    er_add(PG_PAGE_THEME_SWATCHES, SD_GACTION, "removeswatch", "Remove Swatch...")->aux = GTA_REMOVE_SWATCH;
    for (g = 0; g < GTG_COUNT; g++)
        for (st = 0; st < GT_STATES; st++) {
            snprintf(k, 24, "group.%d.%d", g, st);
            snprintf(l, sizeof l, "%s %s", gt_group_defs[g].label, ed_state_name[st]);
            s = er_add(PG_PAGE_THEME_GROUPS, SD_SWATCHREF, k, l);
            s->aux = g;
            s->aux2 = st;
        }
    for (e = 0; e < GTE_COUNT; e++) {
        for (st = 0; st < GT_STATES; st++) {
            snprintf(k, 24, "elem.%d.%d", e, st);
            snprintf(l, sizeof l, "%s %s", gt_elem_defs[e].label, ed_state_name[st]);
            s = er_add(PG_PAGE_THEME_ELEMENTS, SD_ELEMENT, k, l);
            s->aux = e;
            s->aux2 = st;
        }
        snprintf(k, 24, "clear.%d", e);
        snprintf(l, sizeof l, "Clear Override: %s", gt_elem_defs[e].label);
        s = er_add(PG_PAGE_THEME_ELEMENTS, SD_GACTION, k, l);
        s->aux = GTA_CLEAR_ELEMENT;
        s->aux2 = e;
    }
    for (i = 0; i < GT_FONTS; i++)
        if (font_label[i])
            er_add(PG_PAGE_THEME_TYPE, SD_FONT, gt_fontkey[i], font_label[i])->aux = i + 1;
    for (i = 0; i < N_LOOK; i++) {
        s = er_add(look_defs[i].page, look_defs[i].kind, look_defs[i].key, look_defs[i].label);
        s->values = look_defs[i].vals;
        s->nvalues = look_defs[i].nvals;
        s->off = look_defs[i].off;
        s->max = look_defs[i].max;
        s->aux = 100 + i;
    }
    er_reuse(PG_PAGE_THEME_SWATCHES, &sd_settings[SDI_PALETTE], "Terminal Palette", 0);
    {
        static const struct { int am; const char *label; const char *help; } desk[] = {
            { AM_DESK, "Theme the Desktop", "Master switch: off leaves every desktop part below alone." },
            { AM_DESKPAL, "Desktop Palette", "Write the theme colours to the Workbench palette." },
            { AM_DESKBACK, "Desktop Backdrop", "Write the theme backdrop pattern and colour." },
            { AM_DESKPTR, "Desktop Pointer", "Write the theme pointer colours." },
            { AM_DESKLBL, "Desktop Icon Labels", "Write the theme icon label settings." }
        };
        for (i = 0; i < (int)(sizeof desk / sizeof desk[0]); i++) {
            s = er_reuse(PG_PAGE_THEME_DESKTOP, sd_amarchy_key(desk[i].am), desk[i].label, desk[i].help);
            s->kind = SD_CYCLE;
            s->values = sd_settings[SDI_AM_BORDERBLANK].values;
            s->nvalues = sd_settings[SDI_AM_BORDERBLANK].nvalues;
        }
        er_reuse(PG_PAGE_THEME_DESKTOP, &sd_settings[SDI_AM_ICONTEXT], "Icon Label Style", 0);
        s = er_add(PG_PAGE_THEME_DESKTOP, SD_ELEMENT, "icon-label-colour", "Icon Label Colour");
        s->aux = GTE_ICON_LABEL_TEXT;
        s->aux2 = 0;
    }
    for (i = 0; i < 3; i++) {
        snprintf(l, ER_LABEL, "Pointer Colour %d", i + 1);
        snprintf(k, 24, "pointer%d", i);
        er_add(PG_PAGE_THEME_DESKTOP, SD_STRING, k, l)->aux = 200 + i;
    }
    er_add(PG_PAGE_THEME_FILES, SD_GACTION, "duplicate", "Duplicate Theme...")->aux = GTA_DUPLICATE;
    er_add(PG_PAGE_THEME_FILES, SD_GACTION, "import", "Import Theme...")->aux = GTA_IMPORT;
    er_add(PG_PAGE_THEME_FILES, SD_GACTION, "export", "Export Theme...")->aux = GTA_EXPORT;
    er_add(PG_PAGE_THEME_FILES, SD_GACTION, "resetbase", "Reset to Base")->aux = GTA_RESET_BASE;
    er_add(PG_PAGE_THEME_FILES, SD_GACTION, "delete", "Delete User Theme")->aux = GTA_DELETE;
    /* TE-10.8b: the Screens page. Rows are static, so every slot exists and gte_row_visible hides the unused ones. */
    for (i = 0; i < GT_SE_ROWS; i++) {
        char k[24];
        sd_setting *r;
        snprintf(k, sizeof k, "scr-tick%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_CYCLE, k, "Screen");
        r->aux = GTSE_TICK; r->aux2 = i; r->values = v_hover; r->nvalues = 2;
        snprintf(k, sizeof k, "scr-theme%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_STRING, k, "Theme");
        r->aux = GTSE_THEME; r->aux2 = i;
        snprintf(k, sizeof k, "scr-take%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_CYCLE, k, "Take Over Palette");
        r->aux = GTSE_TAKE; r->aux2 = i; r->values = v_hover; r->nvalues = 2;
    }
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-all", "All")->aux = GTA_SCR_ALL;
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-none", "None")->aux = GTA_SCR_NONE;
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-invert", "Invert")->aux = GTA_SCR_INVERT;
    {
        sd_setting *r = er_add(PG_PAGE_THEME_SCREENS, SD_CYCLE, "scr-default", "Default");
        r->aux = GTSE_DEFAULT; r->values = v_scrdef; r->nvalues = 3;
    }
    for (i = 0; i < GT_SE_PATS; i++) {
        char k[24], l[ER_LABEL];
        sd_setting *r;
        snprintf(k, sizeof k, "scr-pat%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_STRING, k, "Rule Pattern");
        r->aux = GTSE_PAT; r->aux2 = i;
        snprintf(k, sizeof k, "scr-ptheme%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_STRING, k, "Rule Theme");
        r->aux = GTSE_PTHEME; r->aux2 = i;
        snprintf(k, sizeof k, "scr-ptake%d", i);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_CYCLE, k, "Rule Take Over Palette");
        r->aux = GTSE_PTAKE; r->aux2 = i; r->values = v_hover; r->nvalues = 2;
        snprintf(k, sizeof k, "scr-up%d", i);
        snprintf(l, sizeof l, "Rule %d Move Up", i + 1);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, k, l);
        r->aux = GTA_SCR_UP; r->aux2 = i;
        snprintf(k, sizeof k, "scr-down%d", i);
        snprintf(l, sizeof l, "Rule %d Move Down", i + 1);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, k, l);
        r->aux = GTA_SCR_DOWN; r->aux2 = i;
        snprintf(k, sizeof k, "scr-rm%d", i);
        snprintf(l, sizeof l, "Rule %d Remove", i + 1);
        r = er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, k, l);
        r->aux = GTA_SCR_REMOVE; r->aux2 = i;
    }
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-add", "Add Rule")->aux = GTA_SCR_ADD;
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-save", "Save Screen Themes")->aux = GTA_SCR_SAVE;
    er_add(PG_PAGE_THEME_SCREENS, SD_GACTION, "scr-restore", "Restore Original")->aux = GTA_SCR_RESTORE;
}

const sd_setting *gte_row(int id)
{
    if (id < SDI_COUNT)
        return &sd_settings[id];
    er_build();
    return id - SDI_COUNT < er_n ? &er_row[id - SDI_COUNT] : &sd_settings[0];
}

int gte_nrows(void)
{
    er_build();
    return er_n;
}

static int se_visible(const sd_setting *s);
static int se_text(const sd_setting *s, char *out, int n);
static int se_label(const sd_setting *s, char *out, int n);
static int se_set_text(const sd_setting *s, const char *text);
static int se_step(const sd_setting *s);
static int se_set_index(const sd_setting *s, int index);

int gte_row_visible(const sd_setting *s)
{
    if (s->store != SD_GTHEME)
        return 1;
    if (s->page == PG_PAGE_THEME_SCREENS)
        return se_visible(s);
    if (s->page == PG_PAGE_THEME_ELEMENTS && !gt_ed.advanced)
        return 0;
    if (s->kind == SD_INFO && s->aux >= ER_INFO_SHARE) {   /* TE-8.3: only a group that shares a pen on a shallow screen */
        gt_budget b;
        if (gt_ed.depth <= 0 || !gt_ed.open)
            return 0;
        gt_pen_budget(&gt_ed.m, gt_ed.depth, &b);
        return b.grp_with[s->aux - ER_INFO_SHARE] != s->aux - ER_INFO_SHARE;
    }
    if (s->kind == SD_SWATCH)
        return s->aux < gt_ed.m.nswatch;
    if (s->kind == SD_ELEMENT)
        return (gt_elem_defs[s->aux].states >> s->aux2) & 1;
    return 1;
}

static unsigned char *look_byte(const sd_setting *s)
{
    return (unsigned char *)&gt_ed.m.look + s->off;
}

static void ref_label(const gt_ref *r, const gt_ref *fallback, char *out, int n)
{
    char t[96];
    if (r->kind == GT_REF_NONE) {
        if (fallback && fallback->kind != GT_REF_NONE && gt_ref_text(&gt_ed.m, fallback, t, (int)sizeof t) >= 0)
            snprintf(out, (size_t)n, "Follows %s", t);
        else
            snprintf(out, (size_t)n, "Follows its group");
    } else if (gt_ref_text(&gt_ed.m, r, t, (int)sizeof t) < 0)
        snprintf(out, (size_t)n, "too long");
    else
        snprintf(out, (size_t)n, "%s", t);
}

static void info_text(int which, char *out, int n)
{
    const gt_model *m = &gt_ed.m;
    if (which == ER_INFO_BASE)
        snprintf(out, (size_t)n, "%s", m->base[0] ? m->base : "none");
    else if (which == ER_INFO_CONTRAST) {
        gt_cpair p[GT_CPAIRS];
        int first, w = gt_contrast_warnings(m, &first);
        if (w && first >= 0) {
            gt_contrast_pairs(m, p);
            snprintf(out, (size_t)n, "%d warning%s: %s on %s too faint", w, w == 1 ? "" : "s", gt_elem_defs[p[first].text].label,
                     gt_elem_defs[p[first].ground].label);
        } else
            snprintf(out, (size_t)n, "0 warnings");
    } else if (which >= ER_INFO_SHARE) {
        gt_budget b;
        gt_pen_budget(m, gt_ed.depth, &b);
        gt_budget_share_text(&b, which - ER_INFO_SHARE, out, n);
    } else {
        gt_budget b;
        if (gt_ed.depth <= 0)
            snprintf(out, (size_t)n, "screen depth unknown");
        else {
            gt_pen_budget(m, gt_ed.depth, &b);
            gt_budget_text(&b, out, n);
        }
    }
}

int gte_row_text(const sd_setting *s, char *out, int n)
{
    char t[96];
    if (n < 1)
        return 0;
    out[0] = 0;
    if (s->store == SD_GTHEME && s->page == PG_PAGE_THEME_SCREENS)
        return se_text(s, out, n);
    switch (s->kind) {
    case SD_SWATCH:
        if (s->aux >= gt_ed.m.nswatch)
            return 0;
        hex_text(gt_ed.m.swatch[s->aux], t);
        snprintf(out, (size_t)n, "%s", t);
        return 1;
    case SD_SWATCHREF:
        ref_label(&gt_ed.m.group[s->aux][s->aux2], s->aux2 ? &gt_ed.m.group[s->aux][GT_ST_NORMAL] : 0, out, n);
        return 1;
    case SD_ELEMENT:
        ref_label(&gt_ed.m.elem[s->aux][s->aux2], &gt_ed.m.group[gt_elem_defs[s->aux].group][s->aux2], out, n);
        if (s->aux2 == GT_ST_NORMAL && gt_contrast_elem_warns(&gt_ed.m, s->aux)) {   /* TE-8.2, D8: the warning on the row */
            size_t l = strlen(out);
            snprintf(out + l, (size_t)n - l, " (low contrast)");
        }
        return 1;
    case SD_INT:
        snprintf(out, (size_t)n, "%d", (int)*look_byte(s));
        return 1;
    case SD_FONT:
        snprintf(out, (size_t)n, "%s", gt_ed.m.look.font[s->aux - 1]);
        return 1;
    case SD_STRING:
        if (gt_ed.m.look.pointer[s->aux - 200]) {
            hex_text(gt_ed.m.look.pointer[s->aux - 200], t);
            snprintf(out, (size_t)n, "%s", t);
        }
        return 1;
    case SD_INFO:
        info_text(s->aux, out, n);
        return 1;
    case SD_CYCLE:
        if (s->aux == -1)
            snprintf(out, (size_t)n, "%s", v_mode[gt_ed.advanced ? 1 : 0].text);
        else
            snprintf(out, (size_t)n, "%s", s->values[*look_byte(s) < s->nvalues ? *look_byte(s) : 0].text);
        return 1;
    default:
        return 0;
    }
}

int gte_row_label(const sd_setting *s, char *out, int n)
{
    char v[96], nm[GT_SWATCH_NAME + 4];
    const char *label = s->label;
    if (s->store == SD_GTHEME && s->page == PG_PAGE_THEME_SCREENS)
        return se_label(s, out, n);
    if (s->kind == SD_SWATCH) {
        snprintf(nm, sizeof nm, "%s", gt_ed.m.swatch_name[s->aux < gt_ed.m.nswatch ? s->aux : 0]);
        label = nm;
    }
    if (s->kind == SD_GACTION) {
        snprintf(out, (size_t)n, "%s", label);
        return 1;
    }
    gte_row_text(s, v, (int)sizeof v);
    if (s->kind == SD_SWATCHREF && s->aux2 == GT_ST_NORMAL && gte_group_overridden(s->aux))
        snprintf(out, (size_t)n, "%s: %s (elements override)", label, v);
    else
        snprintf(out, (size_t)n, "%s: %s", label, v);
    return 1;
}

int gte_row_set_text(const sd_setting *s, const char *text)
{
    int i;
    uc_u32 c = 0;
    if (s->store == SD_GTHEME && s->page == PG_PAGE_THEME_SCREENS)
        return se_set_text(s, text);
    switch (s->kind) {
    case SD_SWATCH:
        return gte_swatch_set(s->aux, text);
    case SD_SWATCHREF:
        return gte_group_set(s->aux, s->aux2, text);
    case SD_ELEMENT:
        return gte_elem_set(s->aux, s->aux2, text);
    case SD_INT: {
        char *end;
        long v = strtol(text, &end, 10);
        if (!*text || *end || v < 0 || v > s->max)
            return GTR_BAD;
        *look_byte(s) = (unsigned char)v;
        ed_touch();
        return GTR_OK;
    }
    case SD_FONT:
        if (strlen(text) >= sizeof gt_ed.m.look.font[0])
            return GTR_BAD;
        strcpy(gt_ed.m.look.font[s->aux - 1], text);
        ed_touch();
        return GTR_OK;
    case SD_STRING:
        i = s->aux - 200;
        if (*text && !hex_parse(text, &c))
            return GTR_BAD;
        gt_ed.m.look.pointer[i] = c;
        ed_touch();
        return GTR_OK;
    default:
        return GTR_BAD;
    }
}

int gte_row_step(const sd_setting *s)
{
    char t[96];
    if (s->store == SD_GTHEME && s->page == PG_PAGE_THEME_SCREENS)
        return se_step(s);
    if (s->kind == SD_CYCLE && s->aux == -1) {
        gt_ed.advanced = !gt_ed.advanced;
        ed_touch();
        return GTR_OK;
    }
    if (s->kind == SD_CYCLE) {
        *look_byte(s) = (unsigned char)((*look_byte(s) + 1) % s->nvalues);
        ed_touch();
        return GTR_OK;
    }
    if (s->kind == SD_SWATCHREF || s->kind == SD_ELEMENT) {
        const gt_ref *cur = s->kind == SD_SWATCHREF ? &gt_ed.m.group[s->aux][s->aux2] : &gt_ed.m.elem[s->aux][s->aux2];
        if (gte_ref_step(cur, 1, t, (int)sizeof t) < 0)
            return GTR_BAD;
        return gte_row_set_text(s, t);
    }
    return GTR_BAD;
}

int gte_row_set_index(const sd_setting *s, int index)
{
    if (s->store == SD_GTHEME && s->page == PG_PAGE_THEME_SCREENS)
        return se_set_index(s, index);
    if (s->store != SD_GTHEME || s->kind != SD_CYCLE || index < 0 || index >= s->nvalues)
        return GTR_BAD;
    if (s->aux == -1)
        gt_ed.advanced = index == 1;
    else
        *look_byte(s) = (unsigned char)index;
    ed_touch();
    return GTR_OK;
}


/* ---- TE-10.2: screen rules (searched the tree first: shell/sh_expand.c sh_match is a Unix glob without #?, handler/complete.c and
 * gthemes_dos.c call dos.library; no pure AmigaDOS matcher exists, so this one is small and local) ---- */
static int lc(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

unsigned long gt_screen_match_steps;

/* Non-recursive state-set (NFA) match, O(pattern length * name length), no malloc, fixed stack (2 * GT_SCR_PAT + 2 bytes).
 * The pattern is tokenised on the fly: a token starts at offset i and is "#c" (repeat c, or any char when c is '?') when
 * p[i] is '#' and p[i+1] exists, else one literal / '?' character. A lone trailing '#' is a literal. reach[i] = 1 when the
 * name read so far can leave the matcher about to read the token that starts at offset i; reach[plen] = whole pattern consumed.
 * A repeat token may be skipped (closure to i + 2). A pattern longer than GT_SCR_PAT - 1 returns 0: the rule parser
 * cannot produce one. */
int gt_screen_match(const char *p, const char *s)
{
    unsigned char a[GT_SCR_PAT + 2], b[GT_SCR_PAT + 2];
    unsigned char *cur = a, *nxt = b, *t;
    size_t plen = strlen(p), i;

    if (plen > GT_SCR_PAT - 1)
        return 0;
    memset(cur, 0, plen + 2);
    cur[0] = 1;
    for (;;) {
        for (i = 0; i < plen; i++) /* closure: repeat tokens may match nothing */
            if (cur[i] && p[i] == '#' && p[i + 1])
                cur[i + 2] = 1;
        if (!*s)
            return cur[plen];
        memset(nxt, 0, plen + 2);
        for (i = 0; i < plen; i++) {
            gt_screen_match_steps++;
            if (!cur[i])
                continue;
            if (p[i] == '#' && p[i + 1]) {
                if (p[i + 1] == '?' || lc((unsigned char)p[i + 1]) == lc((unsigned char)*s))
                    nxt[i] = 1;
                i++; /* the char after '#' is part of this token; positions inside a token are never reachable */
            } else if (p[i] == '?' || lc((unsigned char)p[i]) == lc((unsigned char)*s))
                nxt[i + 1] = 1;
        }
        t = cur; cur = nxt; nxt = t;
        s++;
    }
}

int gt_screen_exact(const char *pat)
{
    return pat && !strchr(pat, '#') && !strchr(pat, '?');
}

static int is_wb_exact(const char *pat)
{
    return cmp_ci(pat, "Workbench") == 0;
}

void gt_screen_rules_parse(const char *buf, long len, gt_screen_rules *out)
{
    long i = 0;
    int first = 1;
    memset(out, 0, sizeof *out);
    out->enabled = 1;
    out->def = GT_SCR_WORKBENCH;
    while (buf && i < len) {
        long b = i, e, eq;
        char pat[GT_SCR_PAT], th[GT_NAME];
        int tk = 0;
        long pl, tl;
        while (i < len && buf[i] != '\n')
            i++;
        e = i;
        if (i < len)
            i++;
        if (e > b && buf[e - 1] == '\r')
            e--;
        while (b < e && (buf[b] == ' ' || buf[b] == '\t'))
            b++;
        if (b >= e || buf[b] == ';')
            continue;
        if (first) {
            first = 0;
            if (e - b >= 8 && !strncmp(buf + b, "default ", 8)) {
                const char *v = buf + b + 8;
                long vl = e - b - 8;
                if (vl == 3 && !strncmp(v, "all", 3))
                    out->def = GT_SCR_ALL;
                else if (vl == 4 && !strncmp(v, "none", 4))
                    out->def = GT_SCR_NONE;
                continue;
            }
        }
        for (eq = b; eq + 2 < e && strncmp(buf + eq, " = ", 3); eq++)
            ;
        if (eq + 2 >= e)
            continue;
        pl = eq - b;
        if (pl < 1 || pl >= GT_SCR_PAT)
            continue;
        memcpy(pat, buf + b, (size_t)pl);
        pat[pl] = 0;
        b = eq + 3;
        while (b < e && buf[b] == ' ')
            b++;
        while (e > b && buf[e - 1] == ' ')
            e--;
        if (e - b > 9 && !strncmp(buf + e - 9, " takeover", 9)) {
            tk = 1;
            e -= 9;
            while (e > b && buf[e - 1] == ' ')
                e--;
        }
        tl = e - b;
        if (tl < 1 || tl >= GT_NAME)
            continue;
        memcpy(th, buf + b, (size_t)tl);
        th[tl] = 0;
        if (out->n < GT_SCR_MAX && !(is_wb_exact(pat) && cmp_ci(th, "exclude") != 0))
            gt_screen_rules_add(out, pat, th, tk);
    }
}

int gt_screen_rules_add(gt_screen_rules *r, const char *pat, const char *theme, int takeover)
{
    gt_screen_rule *x;
    if (r->n >= GT_SCR_MAX || !pat || !*pat || strlen(pat) >= GT_SCR_PAT || !theme || strlen(theme) >= GT_NAME)
        return 0;
    x = &r->rule[r->n++];
    strcpy(x->pat, pat);
    x->exclude = cmp_ci(theme, "exclude") == 0;
    strcpy(x->theme, x->exclude ? "" : theme);
    x->takeover = takeover != 0;
    return 1;
}

int gt_screen_rules_remove(gt_screen_rules *r, const char *pat)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (cmp_ci(r->rule[i].pat, pat) == 0) {
            memmove(&r->rule[i], &r->rule[i + 1], (size_t)(r->n - i - 1) * sizeof r->rule[0]);
            r->n--;
            return 1;
        }
    return 0;
}

long gt_screen_rules_write(const gt_screen_rules *r, char *buf, long cap)
{
    static const char *const dn[] = { "all", "workbench", "none" };
    long n = 0;
    int pass, i;
    n = snprintf(buf, (size_t)cap, "default %s\n", dn[r->def >= 0 && r->def <= 2 ? r->def : 1]);
    if (n < 0 || n >= cap)
        return -1;
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < r->n; i++) {
            const gt_screen_rule *x = &r->rule[i];
            int w;
            if (gt_screen_exact(x->pat) != (pass == 0) || (is_wb_exact(x->pat) && !x->exclude))
                continue;
            w = snprintf(buf + n, (size_t)(cap - n), "%s = %s%s\n", x->pat, x->exclude ? "exclude" : x->theme,
                         x->takeover ? " takeover" : "");
            if (w < 0 || w >= cap - n)
                return -1;
            n += w;
        }
    return n;
}

static void scr_name(char *out, int cap, const char *name)
{
    if (!gt_name_ok(name) || (int)strlen(name) >= cap)
        name = GT_DEFAULT;
    strcpy(out, name);
}

/* TE-10.8: the Screens page rows. One decision for a screen name: scr_pick, shared by gt_screen_theme_of and the row model. */
static int scr_pick(const gt_screen_rules *r, const char *name, int skip, const gt_screen_rule **hit)
{
    int i, pass, wb = is_wb_exact(name);
    *hit = 0;
    /* exact-name rules first, then pattern rules, each in list order: the order gt_screen_rules_write
       stores them, so a rule list resolves the same before and after a write and parse */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < r->n; i++) {
            const gt_screen_rule *x = &r->rule[i];
            if (i == skip || gt_screen_exact(x->pat) != (pass == 0) || !gt_screen_match(x->pat, name))
                continue;
            if (x->exclude)
                return 0;
            *hit = x;
            return 1;
        }
    }
    return !(r->def == GT_SCR_NONE || (r->def == GT_SCR_WORKBENCH && !wb));
}

/* 1 when the decision takes the main theme: Workbench always (D10-10), the default, or a rule saying GT_SCR_MAIN */
static int scr_is_main(const char *name, const gt_screen_rule *hit)
{
    return is_wb_exact(name) || !hit || !strcmp(hit->theme, GT_SCR_MAIN);
}

int gt_screen_theme_of(const gt_screen_rules *r, const char *name, const char *main_theme, char *theme, int cap, int *takeover)
{
    const gt_screen_rule *hit;
    if (takeover)
        *takeover = 0;
    if (!r->enabled) {
        if (!is_wb_exact(name))
            return 0;
        scr_name(theme, cap, main_theme);
        return 1;
    }
    if (!scr_pick(r, name, -1, &hit))
        return 0;
    if (takeover && hit)
        *takeover = hit->takeover;
    scr_name(theme, cap, scr_is_main(name, hit) ? main_theme : hit->theme);
    return 1;
}

static int mask_has(const uc_u32 *mask, int pen)
{
    return (int)((mask[pen >> 5] >> (pen & 31)) & 1UL);
}

int gt_screen_apply_plan(const gt_model *m, int depth, const uc_u32 *free_mask, int allow_owner_pens, gt_plan_pen *out, int cap)
{
    static gtheme t;
    uc_u32 need[GTE_COUNT];
    gt_budget b;
    int nseen = 0, nneed, i, n = 0, pass = 0, next = 0;
    gt_pen_budget(m, depth, &b);
    if (b.exact || cap <= 0)
        return 0;
    gt_model_to_v1(m, &t);
    nneed = needing_colours(m, depth, &t, need, &nseen);
    /* exclusive candidates: pass 0 the free pens, pass 1 (Take Over Palette only) the owner's pens; both from the held count up */
    next = b.held;
    for (i = 0; i < nneed && n < cap; i++) {
        int pen = -1;
        while (pass < (allow_owner_pens ? 2 : 1)) {
            for (; next < b.pens; next++)
                if (mask_has(free_mask, next) == (pass == 0))
                    break;
            if (next < b.pens) {
                pen = next++;
                break;
            }
            pass++;
            next = b.held;
        }
        out[n].rgb = need[i];
        if (pen >= 0) {
            out[n].pen = (short)pen;
            out[n].exclusive = 1;
        } else {
            out[n].pen = (short)gt_nearest_role_pen(&t, need[i], depth);
            out[n].exclusive = 0;
        }
        n++;
    }
    return n;
}

void gt_screen_draw_theme(const gt_screen_rules *r, const char *screen_name, const char *main_theme, char *out, int cap)
{
    if (cap <= 0)
        return;
    if (!screen_name || !*screen_name || !gt_screen_theme_of(r, screen_name, main_theme, out, cap, 0))
        scr_name(out, cap, main_theme);
}

/* TE-10.8: the row model and its operations. The rules count as switched on here (enabled is the page's own switch). */
static int scr_name_ok(const char *name)
{
    return name && *name && strlen(name) < GT_SCR_PAT && gt_screen_exact(name);
}

static int scr_exact_idx(const gt_screen_rules *r, const char *name)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (gt_screen_exact(r->rule[i].pat) && cmp_ci(r->rule[i].pat, name) == 0)
            return i;
    return -1;
}

/* an exact "name = :main" without takeover is dropped when the screen is themed the same way without it */
static void scr_normalise(gt_screen_rules *r, const char *name)
{
    const gt_screen_rule *hit;
    int i = scr_exact_idx(r, name);
    if (i < 0 || r->rule[i].exclude || r->rule[i].takeover || strcmp(r->rule[i].theme, GT_SCR_MAIN))
        return;
    if (scr_pick(r, name, i, &hit) && scr_is_main(name, hit) && !(hit && hit->takeover))
        gt_screen_rules_remove(r, name);
}

static void row_fill(const gt_screen_rules *r, const char *name, int open, gt_screen_row *o)
{
    const gt_screen_rule *hit;
    strcpy(o->name, name);
    o->open = open;
    o->on = scr_pick(r, name, -1, &hit);
    o->theme[0] = 0;
    if (o->on && !scr_is_main(name, hit))
        strcpy(o->theme, hit->theme);
    o->takeover = (o->on && hit) ? hit->takeover : 0;
}

static int row_has(const gt_screen_row *out, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (cmp_ci(out[i].name, name) == 0)
            return 1;
    return 0;
}

int gt_screen_rows(const gt_screen_rules *r, const char *const *open, int nopen, gt_screen_row *out, int cap)
{
    int n = 0, i;
    if (cap <= 0)
        return 0;
    row_fill(r, "Workbench", 1, &out[n++]);
    for (i = 0; i < nopen && n < cap; i++)
        if (open[i] && *open[i] && strlen(open[i]) < GT_SCR_PAT && !row_has(out, n, open[i]))
            row_fill(r, open[i], 1, &out[n++]);
    for (i = 0; i < r->n && n < cap; i++)
        if (gt_screen_exact(r->rule[i].pat) && !row_has(out, n, r->rule[i].pat))
            row_fill(r, r->rule[i].pat, 0, &out[n++]);
    return n;
}

int gt_screen_row_tick(gt_screen_rules *r, const char *name, int on)
{
    const gt_screen_rule *hit;
    int i;
    if (!scr_name_ok(name))
        return 0;
    if (on) {
        if (is_wb_exact(name) && r->def == GT_SCR_NONE)
            return 0; /* Workbench is themed by the default or by nothing: none cannot tick it */
        /* a removal frees a slot, so the add below cannot fail after one */
        while ((i = scr_exact_idx(r, name)) >= 0 && r->rule[i].exclude)
            gt_screen_rules_remove(r, name);
        return scr_pick(r, name, -1, &hit) ? 1 : gt_screen_rules_add(r, name, GT_SCR_MAIN, 0);
    }
    while ((i = scr_exact_idx(r, name)) >= 0 && !r->rule[i].exclude)
        gt_screen_rules_remove(r, name);
    return scr_pick(r, name, -1, &hit) ? gt_screen_rules_add(r, name, "exclude", 0) : 1;
}

int gt_screen_row_theme(gt_screen_rules *r, const char *name, const char *theme)
{
    int i;
    if (!scr_name_ok(name) || is_wb_exact(name) || !theme)
        return 0;
    if (!*theme)
        theme = GT_SCR_MAIN;
    if ((strcmp(theme, GT_SCR_MAIN) && !gt_name_ok(theme)) || cmp_ci(theme, "exclude") == 0)
        return 0;
    i = scr_exact_idx(r, name);
    if (i >= 0) {
        r->rule[i].exclude = 0;
        strcpy(r->rule[i].theme, theme);
    } else if (!gt_screen_rules_add(r, name, theme, 0))
        return 0;
    scr_normalise(r, name);
    return 1;
}

int gt_screen_row_takeover(gt_screen_rules *r, const char *name, int on)
{
    const gt_screen_rule *hit;
    int i;
    if (!scr_name_ok(name) || is_wb_exact(name) || !scr_pick(r, name, -1, &hit))
        return 0;
    on = on != 0;
    if ((hit ? hit->takeover : 0) == on)
        return 1;
    i = scr_exact_idx(r, name);
    if (i >= 0)
        r->rule[i].takeover = on;
    else if (!gt_screen_rules_add(r, name, hit ? hit->theme : GT_SCR_MAIN, on))
        return 0;
    scr_normalise(r, name);
    return 1;
}

/* mode 0 = none, 1 = all, 2 = invert; Workbench first, a listed name once */
static int rows_each(gt_screen_rules *r, const char *const *open, int nopen, int mode)
{
    const gt_screen_rule *hit;
    int fails = 0, i, j;
    for (i = -1; i < nopen; i++) {
        const char *nm = i < 0 ? "Workbench" : open[i];
        int dup = 0;
        if (!nm || (i >= 0 && is_wb_exact(nm)))
            continue;
        for (j = 0; j < i; j++)
            if (open[j] && cmp_ci(open[j], nm) == 0)
                dup = 1;
        if (!dup && !gt_screen_row_tick(r, nm, mode == 2 ? !scr_pick(r, nm, -1, &hit) : mode))
            fails++;
    }
    return fails;
}

int gt_screen_rows_all(gt_screen_rules *r, const char *const *open, int nopen) { return rows_each(r, open, nopen, 1); }
int gt_screen_rows_none(gt_screen_rules *r, const char *const *open, int nopen) { return rows_each(r, open, nopen, 0); }
int gt_screen_rows_invert(gt_screen_rules *r, const char *const *open, int nopen) { return rows_each(r, open, nopen, 2); }

/* ---- TE-10.8b: pattern rules and the state of the Screens page ---- */
static int pat_rule(const gt_screen_rules *r, int p)
{
    int i, k = 0;
    for (i = 0; i < r->n; i++)
        if (!gt_screen_exact(r->rule[i].pat)) {
            if (k == p)
                return i;
            k++;
        }
    return -1;
}

int gt_screen_pat_count(const gt_screen_rules *r)
{
    int i, k = 0;
    for (i = 0; i < r->n; i++)
        k += !gt_screen_exact(r->rule[i].pat);
    return k;
}

static int pat_ok(const char *pat)
{
    size_t l = pat ? strlen(pat) : 0;
    return l > 0 && l < GT_SCR_PAT && !gt_screen_exact(pat) && pat[0] != ' ' && pat[l - 1] != ' ' && !strpbrk(pat, "=\n\r\t;");
}

int gt_screen_pat_add(gt_screen_rules *r, const char *pat)
{
    return pat_ok(pat) && gt_screen_rules_add(r, pat, GT_SCR_MAIN, 0);
}

int gt_screen_pat_set(gt_screen_rules *r, int p, const char *pat)
{
    int i = pat_rule(r, p);
    if (i < 0 || !pat_ok(pat))
        return 0;
    strcpy(r->rule[i].pat, pat);
    return 1;
}

int gt_screen_pat_theme(gt_screen_rules *r, int p, const char *theme)
{
    int i = pat_rule(r, p);
    if (i < 0 || !theme)
        return 0;
    if (cmp_ci(theme, "exclude") == 0) {
        r->rule[i].exclude = 1;
        r->rule[i].theme[0] = 0;
        r->rule[i].takeover = 0;
        return 1;
    }
    if (!*theme)
        theme = GT_SCR_MAIN;
    if (strcmp(theme, GT_SCR_MAIN) && !gt_name_ok(theme))
        return 0;
    r->rule[i].exclude = 0;
    strcpy(r->rule[i].theme, theme);
    return 1;
}

int gt_screen_pat_takeover(gt_screen_rules *r, int p, int on)
{
    int i = pat_rule(r, p);
    if (i < 0 || r->rule[i].exclude)
        return 0;
    r->rule[i].takeover = on != 0;
    return 1;
}

int gt_screen_pat_move(gt_screen_rules *r, int p, int dir)
{
    int i = pat_rule(r, p), j = pat_rule(r, p + (dir < 0 ? -1 : 1));
    gt_screen_rule t;
    if (i < 0 || j < 0)
        return 0;
    t = r->rule[i];
    r->rule[i] = r->rule[j];
    r->rule[j] = t;
    return 1;
}

int gt_screen_pat_remove(gt_screen_rules *r, int p)
{
    int i = pat_rule(r, p);
    if (i < 0)
        return 0;
    for (; i + 1 < r->n; i++)
        r->rule[i] = r->rule[i + 1];
    r->n--;
    return 1;
}

#define SE_THEMES 64
static struct {
    gt_screen_rules r;
    char open[GT_SE_ROWS][GT_SCR_PAT];
    int nopen;
    char theme[SE_THEMES][GT_NAME];
    int nthemes;
    int dirty;
} gt_se;

void (*gt_se_loader)(void);
static int se_loaded;

static void se_ensure(void)
{
    if (!se_loaded && gt_se_loader) {
        se_loaded = 1;      /* before the call: the loader calls gt_se_load */
        gt_se_loader();
    }
}

void gt_se_set_themes(const char *const *names, int n)
{
    int i;
    gt_se.nthemes = 0;
    for (i = 0; i < n && gt_se.nthemes < SE_THEMES; i++)
        if (names[i] && strlen(names[i]) < GT_NAME)
            strcpy(gt_se.theme[gt_se.nthemes++], names[i]);
}

void gt_se_load(const char *buf, long len, const char *const *open, int nopen)
{
    int i;
    se_loaded = 1;
    gt_screen_rules_parse(buf, len, &gt_se.r);
    gt_se.nopen = 0;
    for (i = 0; i < nopen && gt_se.nopen < GT_SE_ROWS - 1; i++)
        if (open[i] && strlen(open[i]) < GT_SCR_PAT)
            strcpy(gt_se.open[gt_se.nopen++], open[i]);
    gt_se.dirty = 0;
}

long gt_se_text(char *buf, long cap)
{
    gt_se.r.enabled = 1;
    return gt_screen_rules_write(&gt_se.r, buf, cap);
}

int gt_se_enabled(void)
{
    return !(gt_se.r.def == GT_SCR_WORKBENCH && gt_se.r.n == 0);
}

int gt_se_dirty(void)
{
    return gt_se.dirty;
}

/* The open screens may have changed (a menu open): the next row asked for runs the loader again. Unsaved edits win: a dirty
 * page keeps its rows until Save or a restart, a reload would throw the edits away. */
void gt_se_stale(void)
{
    if (!gt_se.dirty)
        se_loaded = 0;
}

const gt_screen_rules *gt_se_rules(void)
{
    return &gt_se.r;
}

/* The Screens page rows live in one scratch table, not in each caller's frame (24 rows are 4.6 KB; six callers each held a copy). gt_se
 * is already the page's single static state; se_rows rebuilds the table from it, so a nested call refills the same bytes. */
static gt_screen_row se_scratch[GT_SE_ROWS];

static int se_rows(gt_screen_row *rows)
{
    const char *op[GT_SE_ROWS];
    int i;
    for (i = 0; i < gt_se.nopen; i++)
        op[i] = gt_se.open[i];
    return gt_screen_rows(&gt_se.r, op, gt_se.nopen, rows, GT_SE_ROWS);
}

static int se_done(int ok)
{
    if (ok)
        gt_se.dirty = 1;
    return ok ? GTR_OK : GTR_BAD;
}

int gt_se_action(int aux, int index)
{
    const char *op[GT_SE_ROWS];
    int i;
    for (i = 0; i < gt_se.nopen; i++)
        op[i] = gt_se.open[i];
    switch (aux) {
    case GTA_SCR_ALL:    gt_screen_rows_all(&gt_se.r, op, gt_se.nopen); return se_done(1);
    case GTA_SCR_NONE:   gt_screen_rows_none(&gt_se.r, op, gt_se.nopen); return se_done(1);
    case GTA_SCR_INVERT: gt_screen_rows_invert(&gt_se.r, op, gt_se.nopen); return se_done(1);
    case GTA_SCR_UP:     return se_done(gt_screen_pat_move(&gt_se.r, index, -1));
    case GTA_SCR_DOWN:   return se_done(gt_screen_pat_move(&gt_se.r, index, 1));
    case GTA_SCR_REMOVE: return se_done(gt_screen_pat_remove(&gt_se.r, index));
    case GTA_SCR_RESTORE:
        gt_screen_rules_parse("default workbench\n", 18, &gt_se.r);   /* the stock file: no rule, Workbench only */
        return se_done(1);
    default:             return GTR_BAD;
    }
}

int gt_se_add_rule(const char *pat)
{
    if (!pat_ok(pat))
        return GTR_BAD;
    if (gt_se.r.n >= GT_SCR_MAX || gt_screen_pat_count(&gt_se.r) >= GT_SE_PATS)
        return GTR_FULL;
    return se_done(gt_screen_pat_add(&gt_se.r, pat));
}

static int se_visible(const sd_setting *s)
{
    gt_screen_row *rows = se_scratch;
    se_ensure();
    switch (s->aux) {
    case GTSE_TICK: case GTSE_THEME: case GTSE_TAKE:
        return s->aux2 < se_rows(rows);
    case GTSE_PAT: case GTSE_PTHEME: case GTSE_PTAKE:
    case GTA_SCR_UP: case GTA_SCR_DOWN: case GTA_SCR_REMOVE:
        return s->aux2 < gt_screen_pat_count(&gt_se.r);
    default:
        return 1;
    }
}

/* the theme cycle: Main Theme, the themes, and for a pattern rule Exclude last */
static const char *se_theme_name(int i, int exclude_too, int *count)
{
    int n = 1 + gt_se.nthemes + (exclude_too ? 1 : 0);
    *count = n;
    if (i == 0)
        return "Main Theme";
    if (i <= gt_se.nthemes)
        return gt_se.theme[i - 1];
    return "Exclude";
}

static int se_theme_find(const char *text, int exclude_too)
{
    int i, n;
    for (i = 0; i < 1 + gt_se.nthemes + (exclude_too ? 1 : 0); i++)
        if (cmp_ci(se_theme_name(i, exclude_too, &n), text) == 0)
            return i;
    return *text ? -1 : 0;
}

static const char *se_row_theme_text(const gt_screen_row *w, char *tmp)
{
    (void)tmp;
    return w->theme[0] && strcmp(w->theme, GT_SCR_MAIN) ? w->theme : "Main Theme";
}

static const char *se_pat_theme_text(const gt_screen_rule *u)
{
    if (u->exclude)
        return "Exclude";
    return u->theme[0] && strcmp(u->theme, GT_SCR_MAIN) ? u->theme : "Main Theme";
}

static int se_text(const sd_setting *s, char *out, int n)
{
    gt_screen_row *rows = se_scratch;
    int nr = se_rows(rows), i = s->aux2, q;
    switch (s->aux) {
    case GTSE_TICK:
        snprintf(out, (size_t)n, "%s", i < nr && rows[i].on ? "On" : "Off");
        return 1;
    case GTSE_THEME:
        snprintf(out, (size_t)n, "%s", i < nr ? se_row_theme_text(&rows[i], out) : "");
        return 1;
    case GTSE_TAKE:
        snprintf(out, (size_t)n, "%s", i < nr && rows[i].takeover ? "On" : "Off");
        return 1;
    case GTSE_DEFAULT:
        snprintf(out, (size_t)n, "%s", v_scrdef[gt_se.r.def >= 0 && gt_se.r.def < 3 ? gt_se.r.def : 1].text);
        return 1;
    case GTSE_PAT: case GTSE_PTHEME: case GTSE_PTAKE:
        q = pat_rule(&gt_se.r, i);
        if (q < 0)
            return 1;
        if (s->aux == GTSE_PAT)
            snprintf(out, (size_t)n, "%s", gt_se.r.rule[q].pat);
        else if (s->aux == GTSE_PTHEME)
            snprintf(out, (size_t)n, "%s", se_pat_theme_text(&gt_se.r.rule[q]));
        else
            snprintf(out, (size_t)n, "%s", gt_se.r.rule[q].takeover ? "On" : "Off");
        return 1;
    default:
        return 0;
    }
}

static int se_label(const sd_setting *s, char *out, int n)
{
    gt_screen_row *rows = se_scratch;
    char v[GT_SCR_PAT + 8];
    int nr = se_rows(rows), i = s->aux2;
    se_text(s, v, (int)sizeof v);
    switch (s->aux) {
    case GTSE_TICK:
        snprintf(out, (size_t)n, "%s%s: %s", i < nr ? rows[i].name : "", i < nr && !rows[i].open ? " (not open)" : "", v);
        break;
    case GTSE_THEME:
        snprintf(out, (size_t)n, "%s Theme: %s", i < nr ? rows[i].name : "", v);
        break;
    case GTSE_TAKE:
        snprintf(out, (size_t)n, "%s Take Over Palette: %s", i < nr ? rows[i].name : "", v);
        break;
    case GTSE_DEFAULT:
        snprintf(out, (size_t)n, "Default: %s", v);
        break;
    case GTSE_PAT:
        snprintf(out, (size_t)n, "Rule %d Pattern: %s", i + 1, v);
        break;
    case GTSE_PTHEME:
        snprintf(out, (size_t)n, "Rule %d Theme: %s", i + 1, v);
        break;
    case GTSE_PTAKE:
        snprintf(out, (size_t)n, "Rule %d Take Over Palette: %s", i + 1, v);
        break;
    default:
        snprintf(out, (size_t)n, "%s", s->label);
        break;
    }
    return 1;
}

static int se_set_theme(const sd_setting *s, const char *text)
{
    gt_screen_row *rows = se_scratch;
    int nr = se_rows(rows), i = s->aux2, k = se_theme_find(text, s->aux == GTSE_PTHEME), cnt;
    const char *name;
    if (k < 0)
        return GTR_BAD;
    name = se_theme_name(k, s->aux == GTSE_PTHEME, &cnt);
    if (s->aux == GTSE_PTHEME)
        return se_done(gt_screen_pat_theme(&gt_se.r, i, k == 0 ? "" : name));
    return se_done(i < nr && gt_screen_row_theme(&gt_se.r, rows[i].name, k == 0 ? "" : name));
}

static int se_set_index(const sd_setting *s, int index)
{
    gt_screen_row *rows = se_scratch;
    int nr = se_rows(rows), i = s->aux2;
    if (s->kind != SD_CYCLE || index < 0 || index >= s->nvalues)
        return GTR_BAD;
    switch (s->aux) {
    case GTSE_TICK:    return se_done(i < nr && gt_screen_row_tick(&gt_se.r, rows[i].name, index));
    case GTSE_TAKE:    return se_done(i < nr && gt_screen_row_takeover(&gt_se.r, rows[i].name, index));
    case GTSE_PTAKE:   return se_done(gt_screen_pat_takeover(&gt_se.r, i, index));
    case GTSE_DEFAULT: gt_se.r.def = index; return se_done(1);
    default:           return GTR_BAD;
    }
}

static int se_step(const sd_setting *s)
{
    gt_screen_row *rows = se_scratch;
    char cur[GT_SCR_PAT + 8];
    int nr = se_rows(rows), i = s->aux2, k, cnt, pat = s->aux == GTSE_PTHEME;
    if (s->kind == SD_CYCLE) {
        se_text(s, cur, (int)sizeof cur);
        k = s->aux == GTSE_DEFAULT ? gt_se.r.def : strcmp(cur, "On") == 0;
        return se_set_index(s, (k + 1) % s->nvalues);
    }
    if (s->aux != GTSE_THEME && !pat)
        return GTR_BAD;
    if (!pat && i >= nr)
        return GTR_BAD;
    se_text(s, cur, (int)sizeof cur);
    k = se_theme_find(cur, pat);
    se_theme_name(0, pat, &cnt);
    return se_set_theme(s, se_theme_name(((k < 0 ? 0 : k) + 1) % cnt, pat, &cnt));
}

static int se_set_text(const sd_setting *s, const char *text)
{
    switch (s->aux) {
    case GTSE_THEME: case GTSE_PTHEME:
        return se_set_theme(s, text);
    case GTSE_PAT:
        return se_done(gt_screen_pat_set(&gt_se.r, s->aux2, text));
    default:
        return GTR_BAD;
    }
}
