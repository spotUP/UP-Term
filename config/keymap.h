/* keymap: the Amarchy key bindings -- a plain-text file read by the XCON:
 * handler and by the commodity, parsed with no OS calls so it can be
 * host-tested (tests/test_keymap.c). Plan
 * thoughts/shared/plans/2026-10-22-amarchy-commodity.md section 1.
 *
 * The file (ENVARC:up-term/keymap; shipped as dist/keymap):
 *
 *   ; comment to end of line
 *   [keymap]
 *   version = 1                    ; a newer major is refused whole
 *   super   = lamiga               ; fixed in v1; any other value skips the row
 *   [bind]
 *   ; key spec         = action             ; label            | category
 *   super+space        = menu:root          ; Menu             | menu
 *   super+1..9         = tab:goto %n        ; Switch to tab %n | workspace
 *   [menu]
 *   ; dotted id        = label              | action
 *   apps.upkg          = Install add-ons    | run:upkg-browse
 *
 * Key spec: "super" plus optional shift, ctrl, alt, rshift, joined with +,
 * then one key name. Letters and digits are RAW key codes (the key's place
 * on the board, not its character), so a Swedish keymap does not move them.
 * The symbol names minus, plus, comma and period follow the active keymap:
 * the key that PRINTS that character (keymap_set_layout, called at startup;
 * US positions 0x0B 0x0C 0x38 0x39 until it is).
 * "1..9" (digits) expands to one row per digit with %n replaced in the
 * action and the label. Super is Left Amiga.
 *
 * Refused (row skipped, counted in refused): super+m and super+n with any
 * qualifier (Intuition's), anything naming the mouse, and every spec with
 * ctrl (Ctrl + both Amigas is the hardware reset). Also skipped: an unknown
 * action prefix or tab: verb, an unknown key name, a spec without super, a
 * table that is full. A later row for the same key replaces the earlier one
 * in place; "none" as the action unbinds it. keymap_overlay reads the
 * optional keymap.user over the shipped file the same way ([menu] ids
 * replace by id).
 *
 * Unknown sections and unknown [keymap] keys are ignored (forward
 * compatible). Built on upconf_rows (one shared tokeniser). Portable C89.
 *
 * Memory: the keymap struct is a small header the caller provides (zeroed
 * once: a static, or MEMF_CLEAR). The bind and menu arrays and the strings
 * (action argument, label, category, menu id) live on the heap, sized to the
 * parsed content, in chunks that never move so a const char * into them stays
 * valid until keymap_free. keymap_parse and keymap_free release what the
 * struct held; keymap_swap hands a parsed scratch over to the live table.
 */
#ifndef KEYMAP_H
#define KEYMAP_H

#define KM_MAX_BIND  192
#define KM_MAX_MENU  128
#define KM_ARG       80
#define KM_LABEL     32
#define KM_CAT       12
#define KM_ID        32
#define KM_VERSION   1

/* qualifier mask of a binding (the key spec's words) */
#define KM_Q_SUPER   1   /* Left Amiga */
#define KM_Q_SHIFT   2
#define KM_Q_CTRL    4   /* never present in a parsed table: refused */
#define KM_Q_ALT     8
#define KM_Q_RSHIFT  16

/* action kinds: the closed prefix set (keymap_action_name) */
enum {
    KA_NONE = 0,
    KA_MENU,     /* menu:<id>     quick menu at a tree node */
    KA_VIEWER,   /* viewer */
    KA_TAB,      /* tab:<new|close|next|prev|goto N> */
    KA_MENUID,   /* menuid:<MENU_NAME> */
    KA_SLASH,    /* slash:/cmd args */
    KA_RUN,      /* run:<command line> */
    KA_REXX,     /* rexx:<script or string> */
    KA_OPEN,     /* open:uterm */
    KA_OTHERMENU,   /* othermenu:<id>  menu: with the other menu program (the upconf menu-style setting) */
    KA_OTHERVIEWER, /* otherviewer     viewer with the other program */
    KA_DETACH,      /* detach:<command line>  a program in the background: no window, input and output NIL: */
    KA_SCAN         /* scan:<command line>  like detach:, and the menu watches the scan it started (RESCAN-FEEDBACK) */
};

/* where an action runs (keymap_action_scope) */
#define KS_TERMINAL  1   /* tab:, menuid:, slash: */
#define KS_COMMODITY 2   /* open:, rexx: */
#define KS_BOTH      3   /* run:, detach:, scan:, menu:, viewer, othermenu:, otherviewer */

typedef struct km_bind {
    unsigned short qual;        /* KM_Q_* */
    unsigned char  code;        /* Amiga raw key code */
    unsigned char  kind;        /* KA_* */
    const char *arg;            /* the text after the prefix (at most KM_ARG - 1), never NULL */
    const char *label;          /* cut to KM_LABEL - 1, never NULL */
    const char *cat;            /* cut to KM_CAT - 1, never NULL */
} km_bind;

typedef struct km_menu {
    unsigned char kind;
    const char *id;             /* dotted tree id, e.g. "apps.upkg" */
    const char *label;
    const char *arg;
} km_menu;

struct km_chunk;                /* the string pool: a chain of fixed blocks */

typedef struct keymap {
    int version;                /* [keymap] version, 0 when absent */
    int newer;                  /* the file's major is newer than KM_VERSION: nothing was loaded */
    int nbind, nmenu;
    int skipped;                /* rows left out, refused ones included */
    int refused;                /* the safety refusals among them */
    int overflow;               /* a table was full or a line too long */
    int first_bad;              /* line number of the first skipped row, 0 when none */
    km_bind *bind;              /* heap: bcap slots, nbind used */
    km_menu *menu;              /* heap: mcap slots, nmenu used */
    int bcap, mcap;
    struct km_chunk *pool;
} keymap;

/* Parse a whole file: the table is released and cleared first. 1 when the table is
 * usable (an empty or missing file is: 0 rows), 0 when buf is NULL or the
 * file's version is newer than ours (k->newer; the table stays empty and
 * the caller keeps its compiled-in default). */
int  keymap_parse(keymap *k, const char *buf, long len);
/* Release the arrays and strings; the struct is zeroed (safe to parse again). */
void keymap_free(keymap *k);
/* Make *live the parsed *scratch: what live held is released, scratch is
 * zeroed (its memory now belongs to live). */
void keymap_swap(keymap *live, keymap *scratch);
/* Rebuild the table at its exact size: only the binds `keep` accepts (all when keep is NULL), no
 * [menu] rows, one array and one string block with no slack; the old memory is released. For a
 * caller that reads the file once and holds the result (the Amarchy commodity). 1 ok, 0 out of
 * memory (the table is unchanged). */
int  keymap_compact(keymap *k, int (*keep)(const km_bind *));
/* Read a second file over the table (keymap.user): rows replace by key
 * spec, "none" unbinds, [menu] rows replace by id. Counters add up. Same
 * return as keymap_parse; a newer-version file changes nothing. */
int  keymap_overlay(keymap *k, const char *buf, long len);

/* The binding for a raw code and qualifier mask, 0 when none. */
const km_bind *keymap_find(const keymap *k, unsigned qual, unsigned code);
/* Move the symbol names (minus, plus, comma, period) to the keys that print
 * them. char_to_raw returns the raw code of the key that prints ch unshifted,
 * -1 when none (that name keeps its US position). NULL restores US. Call
 * before the first parse. */
void keymap_set_layout(int (*char_to_raw)(char ch));
/* The raw code of a key name ("k", "space", "f3"), -1 when unknown. */
int  keymap_key_code(const char *name);
/* The key name of a raw code, 0 when it has none (the inverse, for the viewer). */
const char *keymap_key_name(unsigned code);
/* KS_* of an action kind. */
int  keymap_action_scope(int kind);

#endif
