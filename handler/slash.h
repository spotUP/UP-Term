/* slash: UP-Term's typed commands ("/cursor bar") -- the table, the parse,
 * the help and the completion, with no OS calls so the host tests cover
 * them (tests/test_slash.c). Ledger C1, plan
 * thoughts/shared/plans/2026-10-03-slash-commands.md.
 *
 * A line is ours when it starts with "/" and a lower-case name in the
 * table, followed by a blank or the end; anything else -- "/", "//",
 * "/Work", "/c/dir", a line with a leading blank -- is the program's, as
 * typed (on AmigaDOS "/" is the parent directory).
 *
 * The settings' values carry the window menu's item ids (menu_ids.h): the
 * handler runs a typed setting as the menu pick it is. Portable C89.
 */
#ifndef SLASH_H
#define SLASH_H

#include "menu_ids.h"

/* What a command does, beyond the menu items (ids past the menu's). */
enum {
    SLASH_SCROLLBACK = 1000, /* arg: lines, or "none" */
    SLASH_FONT,              /* arg: "NAME SIZE"; empty: the requester */
    SLASH_FALLBACK,          /* arg: an outline font's name, or "none" */
    SLASH_FG, SLASH_BG, SLASH_CURSOR_COLOR, SLASH_SEL_FG, SLASH_SEL_BG, /* arg: RRGGBB / none */
    SLASH_KC_MODE,           /* arg: KingCON's letters */
    SLASH_THEME,             /* arg: a theme's name; empty: the requester */
    SLASH_PROFILE,           /* arg: a profile's name */
    SLASH_FIND,              /* arg: the text */
    SLASH_HELP,              /* arg: a command's name, or empty for all */
    SLASH_SIZE,              /* arg: COLSxROWS */
    SLASH_LINK_OPEN,         /* arg: the command for a link (%s the address), or "none" */
    SLASH_TERM,              /* arg: TERM for programs vsh starts here, or "none" */
    SLASH_COLORS,            /* arg: rgb | 256 | 16 (COLORTERM, TERM for 16), or "none" */
    SLASH_SETUP,             /* the window's settings and where each came from */
    SLASH_STATE              /* arg: a key, or empty for all: the window's action counters */
};

typedef struct slash_value {
    const char *word;
    long id;   /* the menu item (or SLASH_*) it runs */
    int on;    /* a checkmark item's state after it */
} slash_value;

enum { SL_CHOICE, SL_ACTION, SL_ARG };

typedef struct slash_def {
    const char *name;
    int kind;                  /* SL_CHOICE: one of values; SL_ACTION: id, no argument;
                                * SL_ARG: id with the rest of the line */
    long id;                   /* SL_ACTION, SL_ARG */
    int need_arg;              /* SL_ARG: an argument is required */
    const slash_value *values; /* SL_CHOICE, NULL-word terminated */
    const char *args;          /* for the help: what may follow */
    const char *what;          /* for the help: one line */
} slash_def;

typedef struct slash_cmd {
    const slash_def *def;
    long id;
    int on;
    char arg[200];             /* SL_ARG: the rest, blanks trimmed */
} slash_cmd;

enum { SLASH_NOT_OURS = 0, SLASH_OK, SLASH_ERROR };

/* line, len bytes (a trailing newline is ignored). SLASH_OK fills cmd;
 * SLASH_ERROR writes why into err (errcap bytes, NUL-terminated). */
int slash_parse(const char *line, int len, slash_cmd *cmd, char *err, int errcap);

/* The help: every command (name empty), or the one named; NUL-terminated
 * lines ending in "\n". The bytes written, or -1 for an unknown name. */
int slash_help(const char *name, char *out, int cap);

/* Completion of the word that ends at len in line: the candidates,
 * NUL-separated, into out (cap bytes); *from the word's start in line.
 * Names first ("/cur" -> "/cursor", "/cursor-blink", "/cursor-color"),
 * then a command's values; `extra` (n names) answers the commands whose
 * values are not in the table (profile: the file's profiles). The
 * number of candidates; 0 when the line is not a command line. */
int slash_complete(const char *line, int len, const char *const *extra, int n, char *out, int cap,
                   int *from);

/* the table, for listings: n entries */
const slash_def *slash_table(int *n);

/* W46: what the window tells the programs vsh starts in it, and the
 * /setup listing. slash_term_ok: a terminal type's name (1 to 31 letters,
 * digits and . _ + -). slash_colors_ok: "rgb", "256" or "16". slash_env_list
 * writes the ACTION_VTCON_ENV list for the window's term and colors ("" =
 * not set): TERM=<term>; COLORTERM=truecolor for rgb, COLORTERM= (unset) for
 * 256 and 16. dflt_term ("" = none) is the TERM told when the window has no /term:
 * vtcon-amiga for an AMIGA personality window, whose terminal does not
 * erase to the program's background (no bce). 16 tells TERM=<dflt_term>-16color
 * (xterm-16color when dflt_term is ""), over a /term too: a 256-colour TERM
 * would leave /colors 16 without effect. utf8: the window decodes UTF-8 (an XTERM window
 * with neither LATIN1 nor CP437), told as LC_CTYPE=C.UTF-8 so its programs write UTF-8
 * (libixcompat's setlocale; without it they write Latin-1, the C locale's); LC_CTYPE, not
 * LANG: the encoding is the window's, the language the user's. Returns its bytes (the
 * closing NUL included), 0 when nothing is told. */
int  slash_term_ok(const char *name);
int  slash_colors_ok(const char *name);
long slash_env_list(const char *term, const char *colors, const char *dflt_term, int utf8, char *out, long max);

/* Where a setting's value came from: "this window" when it differs from
 * the profile's (profile: its value, 0 when the profile has no such key;
 * dflt: the built-in value), else "profile" or "default". */
const char *slash_source(const char *value, const char *profile, const char *dflt);

/* W46: the Settings menu's Terminal type and Colour mode items are the
 * commands they stand for. slash_menu_line writes the command line a menu
 * item (a MENU_SET_TERM_* or MENU_SET_COLORS_* id) runs ("/term
 * xterm-256color"); 0 when the id is not one of them. slash_menu_checked:
 * 1 when the item is the window's current setting (term, colors: the
 * window's values, "" = not set), 0 when not, -1 when the id is not one. */
int slash_menu_line(long id, char *out, int cap);
int slash_menu_checked(long id, const char *term, const char *colors);

/* Help > Slash commands: where the /help list goes. raw: the window reads
 * raw keys; tty: a program holds a termios mode (vim, less). Either way a
 * program has the screen, so it gets no text and no keystroke: a requester
 * says to type /help at a shell prompt (SLASH_HELP_REQUESTER). Otherwise
 * the list is printed at the prompt (SLASH_HELP_PRINT). */
enum { SLASH_HELP_PRINT, SLASH_HELP_REQUESTER };
int slash_menu_help_where(int raw, int tty);

typedef struct slash_row {
    const char *name, *value, *source;
} slash_row;
/* The /setup answer: one line per row, names and values in columns, the
 * source in brackets. Returns the bytes written (NUL-terminated). */
int slash_setup_text(const slash_row *rows, int n, char *out, int cap);

/* W47: the window's action counters, for scripts that ask "did the user do it"
 * (the tutorial's window lesson, plan 2026-10-19-tutorial-window-lesson.md). Counters only
 * go up, so a check compares them with a value saved before the step. The HOST window
 * owns the block and is its only writer; it pushes the block to every tab on a change, and
 * a tab answers /state from that copy. tabs and active (the shown tab's number, 1 = first, as
 * Right Amiga 1-9) are the host's, copied in by the push.
 * Keys (the /state names): tabs-opened tab-switches menu-picks last-menu slash-count
 * last-slash copies pastes tabs active km-hits last-key font-size. km-hits counts the keymap bindings the
 * handler ran (Left Amiga keys, plan 2026-10-22-amarchy-commodity); last-key is the last one's
 * (KM_Q_* mask << 8) | raw code. outline-own counts the glyphs UP-Term's own TrueType engine
 * drew for the answering window's outline font (render/outline.h vo_own; plan
 * 2026-10-22-handler-rasterizer.md R3.4). */
enum { STATE_TAB_OPENED = 1, STATE_SWITCH, STATE_MENU, STATE_SLASH, STATE_COPY, STATE_PASTE, STATE_KEYMAP };

typedef struct slash_state {
    long tabs_opened, tab_switches, menu_picks, last_menu, slash_count, copies, pastes, tabs, active, km_hits, last_km;
    long font_size;            /* the answering window's font height in pixels: its own, filled when /state answers */
    long outline_own;          /* the answering window's own-engine glyph count, filled as font_size */
    char last_slash[24];       /* the last typed command's name, "" before the first */
} slash_state;

/* One action: ev is a STATE_*; arg the menu item's id (STATE_MENU), or the key (STATE_KEYMAP); name the command's (STATE_SLASH). */
void slash_state_bump(slash_state *s, int ev, long arg, const char *name);

/* The /state answer: key empty = every key, "key value" per line; a key = its bare value and "\n".
 * Returns the bytes written (NUL-terminated), or -1 for an unknown key (out: the message). */
int slash_state_text(const slash_state *s, const char *key, char *out, int cap);

/* The name in a typed command line ("/help x" -> "help"; len bytes): its length, 0 when line is not one. */
int slash_line_name(const char *line, int len, char *out, int cap);

#endif
