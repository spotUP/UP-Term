/* gthemes_core (see gthemes_core.h). */
#include <stdio.h>
#include <string.h>
#include "gthemes_core.h"

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

/* tokyo-night: background #1a1b26, foreground #a9b1d6, dark_foreground #565f89, accent #7aa2f7 (its colors.toml) */
void gt_builtin(gtheme *out)
{
    memset(out, 0, sizeof *out);
    strcpy(out->name, GT_DEFAULT);
    out->bg = 0x1a1b26UL;
    out->fg = 0xa9b1d6UL;
    out->dim = 0x565f89UL;
    out->accent = 0x7aa2f7UL;
}

void gt_parse(const char *buf, long len, gtheme *out)
{
    gtheme d;
    const char *m;
    gt_builtin(&d);
    upconf_clear(&g_conf);
    upconf_parse(&g_conf, buf, len);
    out->bg = upconf_rgb(&g_conf, "default", "background", d.bg);
    out->fg = upconf_rgb(&g_conf, "default", "foreground", d.fg);
    out->dim = upconf_rgb(&g_conf, "default", "dark_foreground", d.dim);
    out->accent = upconf_rgb(&g_conf, "default", "accent", d.accent);
    m = upconf_str(&g_conf, "default", "mode", "");
    while (*m == '"' || *m == ' ')
        m++;
    out->light = !strncmp(m, "light", 5);
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
