/* amconf_core: Amarchy's own settings file (ENVARC:Amarchy/prefs, ENV:Amarchy/prefs), in the upconf syntax of
 * the UP-Term file. This table is the one list of the keys and their defaults: the commodity, amarchy-gui,
 * amarchy-index, the preferences window, the migration and UP-Term Prefs' legacy strip all walk it. No DOS
 * calls: the file operations come in as callbacks (plan 2026-10-25-amarchy-own-prefs.md). */
#ifndef AMCONF_CORE_H
#define AMCONF_CORE_H

#include "../config/upconf.h"
#include "../prefs/prefs_core.h"

enum {
    AM_MUSIC,   /* amarchy-music-drawers: path list */
    AM_DEMO,    /* amarchy-demo-drawers: path list */
    AM_HIDE,    /* amarchy-hide-drawers: path list */
    AM_FONT,    /* amarchy-menu-font: name:size, empty = the built-in font */
    AM_ICONS,   /* amarchy-app-icons: generic, amiga, none */
    AM_STYLE,   /* menu-style: graphical, text, minimal */
    AM_SKIP,    /* amarchy-index-skip: path list */
    AM_VOLUMES, /* amarchy-index-volumes: volume list */
    AM_TERMMENU, /* menu-in-terminal: text, graphical */
    AM_GTHEME,  /* amarchy-gui-theme: the drawn menu's Omarchy theme name (gthemes_core.h) */
    AM_TILEGAP, /* tile-gap: AmTile's gap between tiled windows, pixels (0..AM_TILEGAP_MAX), no Prefs gadget */
    AM_NKEYS
};

typedef struct am_key {
    const char *name;
    const char *def; /* the value of an absent key; a key at its default is not written */
} am_key;
/* The keys, in AM_* order, read from the settings table (prefs/settings_def.c) on the first use: the table
 * is the one place a key's name and default are written. am_keys[k] reads as an array. */
const am_key *amconf_keys(void);
#define am_keys (amconf_keys())

/* every key's value as text; the enumerated ones read through amconf_icons / amconf_style */
typedef struct am_prefs {
    char val[AM_NKEYS][UC_MAX_VALUE];
} am_prefs;

void amconf_defaults(am_prefs *out);
/* the keys of the default profile of buf (len bytes), the others at their defaults. A file in the Amarchy
 * file's format, and the UP-Term file's keys alike: both keep the keys in "[profile default]". Always 1. */
int amconf_parse(const char *buf, long len, am_prefs *out);
/* amconf_parse by its other name where the text is the old UP-Term file (the migration reads it so) */
int amconf_legacy(const char *upterm_buf, long len, am_prefs *out);
/* the file text for p: a header line, then the keys that differ from their defaults. Length (no NUL), -1
 * when cap is too small */
long amconf_serialize(const am_prefs *p, char *buf, long cap);
/* the enumerated keys as UC_ICONS_* and UC_MENU_*, by the shared readers */
int amconf_icons(const am_prefs *p);
int amconf_style(const am_prefs *p);
int amconf_term_menu(const am_prefs *p);   /* UC_TERMMENU_* */
#define AM_TILEGAP_MAX 64
/* tile-gap as a number: whole digits 0..AM_TILEGAP_MAX, anything else the default of the key */
int amconf_tile_gap(const am_prefs *p);

/* The Prefs editor's view of the file: the six keys its Amarchy page shows, to and from its field set. The
 * keys it does not show (index skip, volumes) stay as they were in the am_prefs it writes back. */
void amconf_to_fields(const am_prefs *p, prefs_fields *f);
void amconf_from_fields(am_prefs *p, const prefs_fields *f);

/* the migration decision: the marker is the ENVARC: file of the new name being absent (never the ENV: one,
 * OS 3.2 answers a lookup there with the ENVARC: copy) */
int amconf_need_migrate(int envarc_new_exists);

/* Remove the eight legacy keys from the "default" profile of an UP-Term upconf (UP-Term Prefs Save does, so
 * the old file stops carrying settings Amarchy no longer reads there). Returns how many were present. */
int amconf_strip_legacy(upconf *c);

/* Callbacks of the DOS side (and the tests' memory file system). */
typedef long (*amconf_read_fn)(void *ctx, const char *path, char *buf, long cap); /* length, -1 missing */
typedef struct amconf_io {
    const prefs_fs *fs;
    amconf_read_fn read;
    int (*dir)(const prefs_paths *, int t); /* make the drawer of a target */
    /* every file name in drawer dir, each handed to add(ud, name); 1 when the drawer was read. 0 (unset)
     * where nothing lists drawers: the theme list is then empty. */
    int (*list)(const char *dir, void (*add)(void *ud, const char *file), void *ud);
} amconf_io;

/* where the values came from */
enum { AMCONF_DEFAULTS = -1, AMCONF_ENV, AMCONF_ENVARC, AMCONF_LEGACY_ENV, AMCONF_LEGACY_ENVARC };
/* ENV:Amarchy/prefs, else ENVARC:Amarchy/prefs, else the UP-Term file (ENV: then ENVARC:); the source taken */
int amconf_load_with(const amconf_io *io, am_prefs *out);

/* Save p to ENVARC: then ENV: (keep 0: ENV: only, "Use"). A prefs_save_at result; *failed the target. */
int amconf_save_with(const amconf_io *io, const am_prefs *p, int keep, int *failed);

/* The Prefs editor's save of its Amarchy page: the six fields over *cur (the keys the page does not show stay as
 * loaded), written to Amarchy's file when they changed or when *src says they were only read from the old UP-Term
 * file (the save of that file drops them, so they must already be in their store). On success *cur and *src
 * are what the files now hold. A prefs_save_at result. Call it BEFORE saving the UP-Term file. */
int amconf_save_fields_with(const amconf_io *io, am_prefs *cur, int *src, const prefs_fields *f, int keep, int *failed);

enum { AMCONF_MIG_NONE, AMCONF_MIG_DONE, AMCONF_MIG_FAILED };
/* Startup migration: the ENVARC: file absent: the legacy keys (or defaults) written to it and to ENV:, the
 * file existing even with no key so the next start does nothing. Both there: nothing, the new file wins. A
 * failed write returns AMCONF_MIG_FAILED (the caller logs one line; the load chain keeps reading legacy). */
int amconf_migrate_with(const amconf_io *io);

#endif
