/* vw_cli -- the options hl and mdv share (colour, theme, colour depth,
 * character set) and how they are settled against the terminal. */
#ifndef VW_CLI_H
#define VW_CLI_H

#include "hl_style.h"
#include "vw_text.h"

typedef struct vw_cli {
    int color;              /* -1 auto (a console), 0 never, 1 always */
    int depth;              /* 0: from TERM / COLORTERM */
    int cs;                 /* -1: from TERM / LANG; VW_UTF8, VW_LATIN1 */
    const char *theme;      /* a built-in name or a file; 0: HL_THEME, then ansi */
    hl_theme th;
} vw_cli;

void vw_cli_init(vw_cli *c);
/* One of the shared options at argv[*i] (its value may be argv[*i + 1]):
 * 1 taken (*i moved past it), 0 not one of them, -1 wrong (said why). */
int vw_cli_option(vw_cli *c, int argc, char **argv, int *i);
/* The help lines for them. */
extern const char vw_cli_help[];
/* Settles colour, depth, theme and character set for an output that is
 * (tty) or is not a console, and starts o on standard output. 0, or -1
 * when the theme could not be read (said why). */
int vw_cli_start(vw_cli *c, int tty, vw_out *o);
/* the terminal reads UTF-8 (TERM, LC_ALL / LC_CTYPE / LANG) */
int vw_term_utf8(void);
/* a number option's value: -1 when it is not one */
long vw_number(const char *s);

#endif
