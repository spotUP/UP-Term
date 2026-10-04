/* hl_style -- token classes, themes and the SGR that draws them, for hl and
 * mdv. A theme maps every class to a style: a foreground and background
 * (default, one of the 16 ANSI colours, a 256-colour index or #rrggbb) and
 * attributes. The output's colour depth decides what is sent: richer
 * colours are brought down to what the terminal has. Portable C89. */
#ifndef HL_STYLE_H
#define HL_STYLE_H

enum hl_class {
    HL_PLAIN, HL_COMMENT, HL_KEYWORD, HL_TYPE, HL_BUILTIN, HL_STRING, HL_ESCAPE,
    HL_NUMBER, HL_PREPROC, HL_FUNCTION, HL_LABEL, HL_VARIABLE, HL_KEY, HL_SECTION,
    HL_TAG, HL_ATTR, HL_HEADING, HL_EMPH, HL_LINK, HL_CODE, HL_META, HL_ADDED,
    HL_REMOVED,
    /* hl's gutter */
    HL_LINENO,
    /* mdv */
    MD_H1, MD_H2, MD_H3, MD_H4, MD_H5, MD_H6, MD_QUOTE, MD_BULLET, MD_RULE,
    MD_CODESPAN, MD_CODEBLOCK, MD_URL, MD_IMAGE, MD_TABLE, MD_TH, MD_TASK,
    HL_NCLASS
};

extern const char *const hl_class_names[HL_NCLASS];

#define HL_DEFAULT (-1L)
#define HL_RGB(r, g, b) (0x1000000L | ((long)(r) << 16) | ((long)(g) << 8) | (long)(b))
#define HL_IS_RGB(c) ((c) >= 0x1000000L)

#define HL_A_BOLD 1
#define HL_A_DIM 2
#define HL_A_ITALIC 4
#define HL_A_UNDER 8
#define HL_A_REVERSE 16
#define HL_A_STRIKE 32

typedef struct hl_sty {
    long fg, bg;            /* HL_DEFAULT, 0..255, HL_RGB() */
    unsigned attr;
} hl_sty;

typedef struct hl_theme {
    hl_sty s[HL_NCLASS];
} hl_theme;

/* the built-in themes: "ansi" (the 16 colours, which follow the window's
 * palette: the default), "mono" (attributes only), "rich" (#rrggbb). 0 for
 * an unknown name. */
int hl_theme_builtin(hl_theme *t, const char *name);
const char *hl_theme_names(void);
/* A theme file over *t: lines "class = word ...", words bold dim italic
 * underline reverse strike plain, a colour (black red green yellow blue
 * magenta cyan white, bright-<name>, grey, 0-255, #rrggbb), "on <colour>"
 * for the background. ';' and '#' at a line's start comment it out.
 * Returns 0, or -1 with err naming the line. */
int hl_theme_parse(hl_theme *t, const char *text, long n, char *err, int errlen);
int hl_class_find(const char *name, int len);

/* "\033[0;...m" for s at depth 16, 256 or 24 (true colour) into out (64
 * bytes room): its length */
int hl_sgr(char *out, const hl_sty *s, int depth);
/* the depth a terminal says it has: COLORTERM truecolor/24bit is 24,
 * TERM *256color* or vtcon 256, anything else 16 */
int hl_depth_from_env(const char *term, const char *colorterm);
/* the nearest of the 256 colours to r,g,b (16..255), and of the 16 */
int hl_rgb_to_256(int r, int g, int b);
int hl_rgb_to_16(int r, int g, int b);
void hl_index_rgb(int i, int *r, int *g, int *b);

#endif
