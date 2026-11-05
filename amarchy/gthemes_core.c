/* gthemes_core (see gthemes_core.h). */
#include <stdio.h>
#include <string.h>
#include "gthemes_core.h"
#include "../prefs/prefs_core.h"

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
        gt_model m;
        gt_model_from_v1(t, &m);
        gt_roles_from_model(&m, r);
    }
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
    return ref_d(m, &r, state, 0);
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

long gt_model_save_text(const gt_model *m, char *out, long cap)
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
    if (m->name[0])
        w_text(&w, "name", m->name);
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
