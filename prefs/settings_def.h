/* settings_def: every setting of UP-Term Prefs' six pages as one row of one table (plan
 * 2026-10-26-amarchy-prefs-inline.md): the file key, the kind, the label with its `_` shortcut, the help, the
 * choices of a cycle as file text plus display text, the default, which file stores it and where its value
 * sits in the field set. The Prefs window, the Amarchy menu and the file parse and serialise read this one
 * table instead of keeping a copy each. No OS calls: the host tests walk it (tests/test_settings_def.c).
 *
 * A row is a setting, not a gadget: the swatch and its hex field, "Scrollback" and "No scrollback", the
 * cursor colour and "Inverse" are one row each (gadget, aux, aux2 name the pg_items ids of the row; the
 * palette is one row for its 16 swatches and 16 hex fields). Portable C89, as prefs_core. */
#ifndef SETTINGS_DEF_H
#define SETTINGS_DEF_H

#include <stddef.h>

#include "prefs_ui.h"

/* the rows, in the order of the pages and, within a page, of pg_items */
enum {
    /* General */
    SDI_PROFILE, SDI_FONT, SDI_FALLBACK, SDI_SCROLLBACK, SDI_CURSOR, SDI_BLINK, SDI_CURSOR_COLOUR,
    SDI_BELL, SDI_BOLD, SDI_SCROLLBAR,
    /* Colors */
    SDI_FG, SDI_BG, SDI_SELFG, SDI_SELBG, SDI_THEME, SDI_DITHER, SDI_OUTLINE_AA, SDI_PADDING,
    SDI_TERM, SDI_COLORS,
    /* Palette */
    SDI_PALETTE,
    /* Keyboard */
    SDI_META, SDI_AMIGAKEYS, SDI_BACKSPACE, SDI_COMPLETE, SDI_KC_MATCH, SDI_KC_COMMON, SDI_KC_SILENT,
    SDI_KC_INFO, SDI_KC_CACHE, SDI_COPY, SDI_WHEEL,
    /* Advanced */
    SDI_SCREEN, SDI_SCREEN_MODE, SDI_ASPECT, SDI_CLIPBOARD, SDI_HLCAT, SDI_REFLOW, SDI_LINK,
    /* Amarchy */
    SDI_AM_MUSIC, SDI_AM_DEMO, SDI_AM_HIDE, SDI_AM_ICONS, SDI_AM_MODE, SDI_AM_TERMMENU, SDI_AM_GTHEME,
    SDI_AM_FONT,
    SDI_COUNT
};

/* what a setting is; the menu row and the gadget are made from it */
enum {
    SD_CHECK,       /* on / off: values[0] is off, values[1] is on */
    SD_CYCLE,       /* one of values[] */
    SD_INT,         /* a number min..max, kept as text in the field set */
    SD_STRING,      /* free text */
    SD_COLOUR,      /* RRGGBB, or empty */
    SD_FONT,        /* name:size, or empty; the font requester picks it */
    SD_SCREENMODE,  /* mode id (key) and depth (key2); the screen mode requester picks them */
    SD_PALETTE,     /* the 16 colour entries (count rows of size bytes each) */
    SD_THEME,       /* takes the colours of a theme file; holds no value */
    SD_PROFILE      /* the profile being edited; holds no value */
};

/* which file holds it */
enum { SD_UPTERM, SD_AMARCHY };

/* where the value sits, in the store the row names (prefs_fields for SD_UPTERM, am_prefs for SD_AMARCHY) */
enum {
    SDF_NONE,       /* no value (profile, theme) */
    SDF_INT,        /* an int at off: the index into values[] */
    SDF_STR,        /* a char array of size bytes at off in prefs_fields */
    SDF_AMSTR,      /* a char array of size bytes at off in am_prefs */
    SDF_KC_MATCH,   /* the match part of the KingCON letters at off (PREFS_KC_*) */
    SDF_KC_COMMON,  /* the common-part flag of the KingCON letters at off */
    SDF_KC_SILENT   /* the silent flag of the KingCON letters at off */
};

typedef struct sd_value {
    const char *file;   /* the text in the file */
    const char *text;   /* the text a gadget or menu row shows */
} sd_value;

typedef struct sd_setting {
    int id;                 /* SDI_* */
    const char *key;        /* the file key; 0: the row is no key of the file (profile, theme) */
    const char *key2;       /* a second key the row writes (screen-depth), else 0 */
    int page;               /* PG_PAGE_* */
    int kind;               /* SD_* */
    const char *label;      /* "_" before the Prefs shortcut letter; the menu cuts it out */
    const char *help;
    const sd_value *values; /* SD_CHECK and SD_CYCLE */
    int nvalues;
    const char *def;        /* the file text of an absent key; "" for a key that is then not written */
    int store;              /* SD_UPTERM or SD_AMARCHY */
    int ftype;              /* SDF_* */
    size_t off;
    int size;               /* SDF_STR and SDF_AMSTR: bytes of one array */
    long min, max;          /* SD_INT */
    int gadget;             /* the PG_* id of the row's control in Prefs */
    int aux, aux2;          /* its other controls (hex field, "No scrollback", "Inverse"), else 0 */
    int count;              /* 1; 16 for the palette: gadget + i and aux + i */
    size_t off2;            /* key2's char array of size bytes in prefs_fields (screen-depth), else 0 */
} sd_setting;

extern const sd_setting sd_settings[SDI_COUNT];
extern const int sd_nsettings;

/* The order prefs_stage writes the keys of a profile that has none yet (a key already in the file is set in
 * place, whatever the order): the ids of the SD_UPTERM rows that own a file key, with the three KingCON rows
 * as SDI_KC_MATCH alone. It is the order of the files already on disks, kept so a save writes the bytes it
 * always wrote; the table above is in page order. The last sd_nstyle ids are the look (font, colours,
 * palette), which prefs_stage_style writes alone. */
extern const int sd_write_order[];
extern const int sd_nwrite;
extern const int sd_nstyle;

/* The row of a file key within a store (SD_UPTERM or SD_AMARCHY), 0 when none; a key two rows share
 * (kingcon-mode) gives the first. */
const sd_setting *sd_find_key(int store, const char *key);
/* The row whose controls include the pg_items id, 0 when none (buttons, the common gadgets). */
const sd_setting *sd_find_gadget(int id);

/* SD_CHECK and SD_CYCLE: the index into values[] of the value in store (a prefs_fields or an am_prefs, as
 * row->store says); a text no value names gives the default's. */
int sd_index(const sd_setting *s, const void *store);
int sd_def_index(const sd_setting *s);
/* The index into values[] of the file text (case-insensitive), -1 when no value names it. */
int sd_value_index(const sd_setting *s, const char *file);
void sd_set_index(const sd_setting *s, void *store, int index);
/* The other kinds: the text of entry n (0 unless the row is the palette) in store, 0 when the row has no
 * text. s->size bytes. */
char *sd_string(const sd_setting *s, void *store, int n);

#endif
