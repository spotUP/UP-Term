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
    SLASH_COLORS,            /* arg: rgb | 256 (COLORTERM for those programs), or "none" */
    SLASH_SETUP              /* the window's settings and where each came from */
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
 * digits and . _ + -). slash_colors_ok: "rgb" or "256". slash_env_list
 * writes the ACTION_VTCON_ENV list for the window's term and colors ("" =
 * not set): TERM=<term>; COLORTERM=truecolor for rgb, COLORTERM= (unset) for
 * 256. Returns its bytes (the closing NUL included), 0 when nothing is told. */
int  slash_term_ok(const char *name);
int  slash_colors_ok(const char *name);
long slash_env_list(const char *term, const char *colors, char *out, long max);

/* Where a setting's value came from: "this window" when it differs from
 * the profile's (profile: its value, 0 when the profile has no such key;
 * dflt: the built-in value), else "profile" or "default". */
const char *slash_source(const char *value, const char *profile, const char *dflt);

typedef struct slash_row {
    const char *name, *value, *source;
} slash_row;
/* The /setup answer: one line per row, names and values in columns, the
 * source in brackets. Returns the bytes written (NUL-terminated). */
int slash_setup_text(const slash_row *rows, int n, char *out, int cap);

#endif
