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

/* RL8 FLAT-COLOUR-OVERHAUL: the one derivation, the same for every theme (see gthemes_core.h) */
void gt_roles_of(const gtheme *t, gt_roles *r)
{
    r->c[GR_BACKGROUND] = gt_darker(t);
    r->c[GR_SURFACE] = t->bg;
    r->c[GR_TEXT] = t->fg;
    r->c[GR_MUTED] = t->muted;
    r->dim = gt_muted(t);
    r->c[GR_ACCENT] = t->accent;
    r->c[GR_SELECTION] = gt_selection(t);
    r->c[GR_INACTIVE] = t->inactive;
    r->ok = t->ok;
    r->fail = t->fail;
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
