/* vw_text -- the output side shared by hl and mdv: a buffered writer, the
 * SGR for a style (themes in hl_style.h), and text measured the way the
 * terminal draws it (engine/vtwidth.h). Portable C89, host-tested. */
#ifndef VW_TEXT_H
#define VW_TEXT_H

#include "hl_style.h"

typedef void (*vw_sink)(void *u, const char *s, long n);

/* The character set the output is in: UTF-8 (an UP-Term window, the xterm
 * dialect's default), or Latin-1 (a ROM CON: window, UP-Term's LATIN1 or
 * AMIGA dialect). Input bytes that are not UTF-8 are taken as Latin-1, the
 * Amiga's own text, and converted; characters Latin-1 lacks become '?'. */
enum { VW_UTF8, VW_LATIN1 };

typedef struct vw_out {
    vw_sink sink;
    void *u;
    int cs;                 /* VW_UTF8 / VW_LATIN1 */
    int depth;              /* 0 no colour, 16, 256, 24 (hl_sgr) */
    const hl_theme *theme;
    hl_sty cur;             /* the style the terminal has now */
    int styled;             /* cur is not the default */
    int osc8;               /* hyperlinks are sent (vo_link) */
    char link[512];         /* the open OSC 8 hyperlink, "" none */
    short sgrlen[HL_NCLASS]; /* each class's SGR, made once (-1 not yet) */
    char sgr[HL_NCLASS][64];
    int n;
    char buf[2048];
} vw_out;

void vo_init(vw_out *o, vw_sink sink, void *u, int cs, int depth, const hl_theme *theme);
/* bytes as they are (escape sequences, already converted text) */
void vo_raw(vw_out *o, const char *s, long n);
void vo_rawz(vw_out *o, const char *s);
/* text from a file: converted to the output's character set */
void vo_text(vw_out *o, const char *s, long n);
void vo_textz(vw_out *o, const char *s);
void vo_spaces(vw_out *o, int n);
/* a style: SGR only when it differs from the terminal's (none at depth 0) */
void vo_style(vw_out *o, const hl_sty *s);
void vo_class(vw_out *o, int cls);
void vo_reset(vw_out *o);
/* an OSC 8 hyperlink around what follows (0 or "" closes the open one) */
void vo_link(vw_out *o, const char *url);
void vo_flush(vw_out *o);

/* One character at s (n > 0 bytes left): its length in bytes, *cp its code
 * point. A byte that does not start valid UTF-8 is one Latin-1 character. */
int  vw_char(const char *s, long n, unsigned long *cp);
int  vw_cp_width(unsigned long cp);
/* display columns of s[0..n) */
int  vw_width(const char *s, long n);
/* the bytes of s[0..n) that fit in cols columns (whole characters) */
long vw_fit(const char *s, long n, int cols);
/* code point as UTF-8 into out (4 bytes room): its length */
int  vw_put_utf8(char *out, unsigned long cp);

#endif
