/* amconf_core (see amconf_core.h). */
#include <string.h>
#include "amconf_core.h"

const am_key am_keys[AM_NKEYS] = {
    { "amarchy-music-drawers", "" },
    { "amarchy-demo-drawers", "" },
    { "amarchy-hide-drawers", "" },
    { "amarchy-menu-font", "" },
    { "amarchy-app-icons", "generic" },
    { "menu-style", "graphical" },
    { "amarchy-index-skip", "" },
    { "amarchy-index-volumes", "" },
    { "menu-in-terminal", "text" },
};

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

int amconf_icons(const am_prefs *p)
{
    return upconf_app_icons(p->val[AM_ICONS]);
}

int amconf_style(const am_prefs *p)
{
    return upconf_menu_style_value(p->val[AM_STYLE]);
}

int amconf_term_menu(const am_prefs *p)
{
    return upconf_term_menu_value(p->val[AM_TERMMENU]);
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

int amconf_migrate_with(const amconf_io *io)
{
    am_prefs p;
    int failed;
    if (!amconf_need_migrate(io->fs->exists(io->fs->ctx, prefs_paths_amarchy.path[PREFS_T_ENVARC])))
        return AMCONF_MIG_NONE;
    amconf_load_with(io, &p); /* nothing of the new name exists yet: the legacy keys, or the defaults */
    return amconf_save_with(io, &p, 1, &failed) == PREFS_INSTALL_OK ? AMCONF_MIG_DONE : AMCONF_MIG_FAILED;
}

int amconf_strip_legacy(upconf *c)
{
    int k, n = 0;
    for (k = 0; k < AM_NKEYS; k++)
        if (upconf_str(c, "default", am_keys[k].name, 0) && upconf_del(c, "default", am_keys[k].name))
            n++;
    return n;
}
