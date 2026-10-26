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
#include "../config/upconf.h"

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
    SD_DYNLIST,     /* one of a list read at run time (the GUI themes, gt_scan); the file text is the chosen name */
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

/* The Amarchy keys with no control in Prefs (index skip list, index volumes): rows of the same kind, no page. */
#define SD_NHIDDEN 2
extern const sd_setting sd_hidden[SD_NHIDDEN];
/* The row, of sd_settings or sd_hidden, that stores the am_prefs value k (AM_*); every k has one. */
const sd_setting *sd_amarchy_key(int k);

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
/* The index into values[] that profile p of c gives the row: the file text's, the default's when the key is
 * absent or its text names no value (a garbage "bold-bright = zzz" is the default, on). The one parse of a
 * check or cycle: prefs_from_conf and the handler's window setup both read through it. */
int sd_conf_index(const sd_setting *s, const upconf *c, const char *p);
/* The same for the key of an SD_UPTERM row; 0 when no row has the key. */
int sd_conf_key_index(const char *key, const upconf *c, const char *p);
void sd_set_index(const sd_setting *s, void *store, int index);
/* The other kinds: the text of entry n (0 unless the row is the palette) in store, 0 when the row has no
 * text. s->size bytes. */
char *sd_string(const sd_setting *s, void *store, int n);
/* The one range check of a typed value, for the menu rows (Prefs' own gadgets cut to the same bounds): SD_INT
 * text is blank (the built-in) or a whole number min..max; SD_STRING text is one line of printable characters
 * that fits the field (s->size - 1). SD_TEXT_OK or the reason; SD_TEXT_KIND for any other kind. */
enum { SD_TEXT_OK = 0, SD_TEXT_NAN, SD_TEXT_RANGE, SD_TEXT_LONG, SD_TEXT_CTRL, SD_TEXT_KIND };
int sd_text_check(const sd_setting *s, const char *text);
/* The words of a refusal, for the menu's message line. */
const char *sd_text_reason(int check);

/* The menu's page groups: `settings:<name>` in the keymap [menu] names a page of Prefs. The PG_PAGE_* of a
 * name, -1 when none; the name of a page, 0 when out of range. */
int sd_page_by_name(const char *name);
const char *sd_page_name(int page);

/* The label without the Prefs shortcut `_`, copied to out (n bytes, always terminated); the length. */
int sd_label_plain(const sd_setting *s, char *out, int n);
/* One menu row, "Label: Value" for every kind, the label without its `_`. store is the row's own store
 * (a prefs_fields for SD_UPTERM, an am_prefs for SD_AMARCHY, as s->store says). width is the size of buf in
 * bytes: when label and value do not fit, the value is cut (ending in "..") and the label is kept whole.
 * Shown values: a check or cycle the display text of its value, an int, string, colour, font, screen mode
 * and theme name as stored ("default" when empty, "(empty)" for an empty string), the palette, theme and
 * profile rows "...". The length of the text. */
int sd_row_label(const sd_setting *s, const void *store, char *buf, int width);

#endif
