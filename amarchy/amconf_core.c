/* amconf_core (see amconf_core.h). */
#include <stdlib.h>
#include <string.h>
#include "amconf_core.h"


/* one parse table: the whole file is never held twice */
static upconf g_conf;
static char g_buf[UC_MAX_FILE];

void amconf_defaults(am_prefs *out)
{
    int k;
    memset(out, 0, sizeof *out);
    for (k = 0; k < AM_NKEYS; k++)
        strcpy(out->val[k], am_keys[k].def);
}

int amconf_parse(const char *buf, long len, am_prefs *out)
{
    int k;
    amconf_defaults(out);
    upconf_clear(&g_conf);
    upconf_parse(&g_conf, buf, len);
    for (k = 0; k < AM_NKEYS; k++) {
        const char *v = upconf_str(&g_conf, "default", am_keys[k].name, am_keys[k].def);
        strncpy(out->val[k], v, UC_MAX_VALUE - 1);
        out->val[k][UC_MAX_VALUE - 1] = 0;
    }
    return 1;
}

int amconf_legacy(const char *upterm_buf, long len, am_prefs *out)
{
    return amconf_parse(upterm_buf, len, out);
}

long amconf_serialize(const am_prefs *p, char *buf, long cap)
{
    static const char hdr[] = "; Amarchy preferences. Written by the Amarchy preferences window (Exchange: Show Interface).\n";
    long n = (long)sizeof hdr - 1, body;
    int k;
    upconf_clear(&g_conf);
    for (k = 0; k < AM_NKEYS; k++)
        if (strcmp(p->val[k], am_keys[k].def) != 0)
            upconf_set(&g_conf, "default", am_keys[k].name, p->val[k]);
    if (n >= cap)
        return -1;
    memcpy(buf, hdr, (size_t)n);
    body = upconf_save(&g_conf, buf + n, cap - n);
    return body < 0 ? -1 : n + body;
}

void amconf_to_fields(const am_prefs *p, prefs_fields *f)
{
    strncpy(f->am_music, p->val[AM_MUSIC], sizeof f->am_music - 1);
    strncpy(f->am_demo, p->val[AM_DEMO], sizeof f->am_demo - 1);
    strncpy(f->am_hide, p->val[AM_HIDE], sizeof f->am_hide - 1);
    strncpy(f->am_font, p->val[AM_FONT], sizeof f->am_font - 1);
    strncpy(f->am_gtheme, p->val[AM_GTHEME], sizeof f->am_gtheme - 1);
    f->am_icons = amconf_icons(p);
    f->am_mode = amconf_style(p);
    f->am_termmenu = amconf_term_menu(p);
    f->am_flat = amconf_flat_index(p, AM_FLAT);
    f->am_flatgad = amconf_flat_index(p, AM_FLATGAD);
    f->am_flatsize = amconf_flat_index(p, AM_FLATSIZE);
    f->am_flatstyle = amconf_flat_index(p, AM_FLATSTYLE);
}

void amconf_from_fields(am_prefs *p, const prefs_fields *f)
{
    strcpy(p->val[AM_MUSIC], f->am_music);
    strcpy(p->val[AM_DEMO], f->am_demo);
    strcpy(p->val[AM_HIDE], f->am_hide);
    strcpy(p->val[AM_FONT], f->am_font);
    strcpy(p->val[AM_GTHEME], f->am_gtheme[0] ? f->am_gtheme : am_keys[AM_GTHEME].def);
    strcpy(p->val[AM_ICONS], f->am_icons == UC_ICONS_AMIGA ? "amiga" : f->am_icons == UC_ICONS_NONE ? "none" : "generic");
    strcpy(p->val[AM_STYLE], f->am_mode == UC_MENU_TEXT ? "text" : f->am_mode == UC_MENU_MINIMAL ? "minimal" : "graphical");
    strcpy(p->val[AM_TERMMENU], f->am_termmenu == UC_TERMMENU_GRAPHICAL ? "graphical" : "text");
    strcpy(p->val[AM_FLAT], f->am_flat ? "on" : "off");
    strcpy(p->val[AM_FLATGAD], f->am_flatgad ? "on" : "off");
    strcpy(p->val[AM_FLATSIZE], f->am_flatsize ? "thin" : "stock");
    strcpy(p->val[AM_FLATSTYLE], f->am_flatstyle ? "block" : "line");
}

int amconf_icons(const am_prefs *p)
{
    return upconf_app_icons(p->val[AM_ICONS]);
}

int amconf_style(const am_prefs *p)
{
    return upconf_menu_style_value(p->val[AM_STYLE]);
}

int amconf_flat_index(const am_prefs *p, int key)
{
    const char *v = p->val[key];
    return !strcmp(v, "on") || !strcmp(v, "thin") || !strcmp(v, "block");
}

int amconf_term_menu(const am_prefs *p)
{
    return upconf_term_menu_value(p->val[AM_TERMMENU]);
}

const char *amconf_agent(const am_prefs *p)
{
    const char *s = p->val[AM_AGENT];
    return *s ? s : am_keys[AM_AGENT].def;
}

const char *amconf_shot_dir(const am_prefs *p)
{
    const char *s = p->val[AM_SHOTDIR];
    return *s ? s : am_keys[AM_SHOTDIR].def;
}

static int amconf_on(const am_prefs *p, int k)
{
    const char *s = p->val[k];
    if (!*s)
        s = am_keys[k].def;
    return !strcmp(s, "on");
}

unsigned amconf_desk_mask(const am_prefs *p)
{
    unsigned m = 0;
    int t;
    if (!amconf_on(p, AM_DESK))
        return 0;
    for (t = 0; t < AM_DESKLBL - AM_DESKPAL + 1; t++)
        if (amconf_on(p, AM_DESKPAL + t))
            m |= 1u << t;
    return m;
}

int amconf_tile_gap(const am_prefs *p)
{
    const char *s = p->val[AM_TILEGAP];
    long v = 0;
    if (!*s)
        s = am_keys[AM_TILEGAP].def;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') {
            v = -1;
            break;
        }
        v = v * 10 + (*s - '0');
        if (v > AM_TILEGAP_MAX)
            break;
    }
    if (v < 0 || v > AM_TILEGAP_MAX)
        return (int)(am_keys[AM_TILEGAP].def[0] - '0');
    return (int)v;
}

/* whole digits only, value in lo..hi, else the key's default (same rule as amconf_tile_gap) */
static int digits_in(const char *s, int lo, int hi, int key)
{
    long v = 0;
    const char *d = s;
    if (!*d)
        return atoi(am_keys[key].def);
    for (; *d; d++) {
        if (*d < '0' || *d > '9')
            return atoi(am_keys[key].def);
        v = v * 10 + (*d - '0');
        if (v > hi)
            return atoi(am_keys[key].def);
    }
    return v < lo ? atoi(am_keys[key].def) : (int)v;
}

const char *amconf_tile_layout(const am_prefs *p)
{
    const char *s = p->val[AM_TILELAYOUT];
    return *s ? s : am_keys[AM_TILELAYOUT].def;
}

int amconf_tile_ratio(const am_prefs *p)
{
    return digits_in(p->val[AM_TILERATIO], AM_TILERATIO_MIN, AM_TILERATIO_MAX, AM_TILERATIO);
}

int amconf_tile_nmaster(const am_prefs *p)
{
    return digits_in(p->val[AM_TILENMASTER], 1, AM_TILENM_MAX, AM_TILENMASTER);
}

int amconf_need_migrate(int envarc_new_exists)
{
    return !envarc_new_exists;
}

static int read_into(const amconf_io *io, const char *path, am_prefs *out)
{
    long n = io->read(io->fs->ctx, path, g_buf, (long)sizeof g_buf);
    if (n < 0)
        return 0;
    amconf_parse(g_buf, n, out);
    return 1;
}

int amconf_load_with(const amconf_io *io, am_prefs *out)
{
    int t;
    amconf_defaults(out);
    for (t = PREFS_T_ENV; t <= PREFS_T_ENVARC; t++)
        if (read_into(io, prefs_paths_amarchy.path[t], out))
            return t == PREFS_T_ENV ? AMCONF_ENV : AMCONF_ENVARC;
    for (t = PREFS_T_ENV; t <= PREFS_T_ENVARC; t++)
        if (read_into(io, prefs_paths_upterm.path[t], out))
            return t == PREFS_T_ENV ? AMCONF_LEGACY_ENV : AMCONF_LEGACY_ENVARC;
    return AMCONF_DEFAULTS;
}

int amconf_save_with(const amconf_io *io, const am_prefs *p, int keep, int *failed)
{
    static char out[UC_MAX_FILE];
    long n = amconf_serialize(p, out, (long)sizeof out);
    if (n < 0) {
        *failed = PREFS_T_ENV;
        return PREFS_INSTALL_WRITE;
    }
    return prefs_save_at(io->fs, io->dir, &prefs_paths_amarchy, out, n, keep, failed);
}

int amconf_save_fields_with(const amconf_io *io, am_prefs *cur, int *src, const prefs_fields *f, int keep, int *failed)
{
    am_prefs next = *cur;
    int r;
    amconf_from_fields(&next, f);
    if (memcmp(&next, cur, sizeof next) == 0 && *src < AMCONF_LEGACY_ENV)
        return PREFS_INSTALL_OK;
    r = amconf_save_with(io, &next, keep, failed);
    if (r == PREFS_INSTALL_OK) {
        *cur = next;
        *src = keep ? AMCONF_ENVARC : AMCONF_ENV;
    }
    return r;
}

int amconf_migrate_with(const amconf_io *io)
{
    am_prefs p;
    int failed;
    if (!amconf_need_migrate(io->fs->exists(io->fs->ctx, prefs_paths_amarchy.path[PREFS_T_ENVARC])))
        return AMCONF_MIG_NONE;
    amconf_load_with(io, &p); /* nothing of the new name exists yet: the legacy keys, or the defaults */
    return amconf_save_with(io, &p, 1, &failed) == PREFS_INSTALL_OK ? AMCONF_MIG_DONE : AMCONF_MIG_FAILED;
}

