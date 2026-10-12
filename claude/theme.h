/* theme -- the screen's colours (A4 1.10, /theme), as Claude Code's
 * themes on a 16-colour terminal: every colour the input box, the status
 * line, the menus and the transcript use is one SGR string here. The names
 * are Claude Code's settings values ("theme" in settings.json): dark,
 * light, dark-daltonized, light-daltonized, dark-ansi, light-ansi; and
 * monochrome (no colour at all: a 2- or 4-colour Workbench screen).
 * Portable C89. */
#ifndef CL_THEME_H
#define CL_THEME_H

typedef struct cl_theme {
    const char *name;           /* the settings value */
    const char *label;          /* the picker's line */
    const char *box;            /* the input box's frame */
    const char *spin;           /* the spinner */
    const char *sel;            /* a menu's selection */
    const char *accent;         /* the welcome box, in-progress todos */
    const char *accept;         /* accept edits mode */
    const char *plan;           /* plan mode */
    const char *bash;           /* ! bash mode: the frame and the prompt */
    const char *memory;         /* # memory: the frame and the prompt */
    const char *ok;             /* a tool call that worked */
    const char *err;            /* one that failed, error text */
    const char *add;            /* a diff's added lines */
    const char *del;            /* removed lines */
    const char *dim;            /* hints, user turns, queued prompts */
    const char *hl;             /* the code highlighter's theme (view/hl_style) */
} cl_theme;

#define THEME_COUNT 7

extern const cl_theme cl_themes[THEME_COUNT];

/* the theme of that name, the default (dark) for an unknown one */
const cl_theme *theme_get(const char *name);
/* its index in cl_themes */
int theme_index(const cl_theme *t);

/* Claude Code's named colours (an agent's "color", /color's prompt bar):
 * red, blue, green, yellow, purple, orange, pink, cyan */
#define THEME_NAMED 8
extern const char *const theme_named_names[THEME_NAMED];
/* the SGR of a named colour (any case), 0 for another name */
const char *theme_named(const char *name);


/* ---- pi's themes (amiga-pi plan N0, N6) ----
 * pi colours its screen by roles (themes.md: 57 of them, a "name", "vars"
 * and "colors" JSON). A role is a colour in one of pi's spellings -- a
 * 256-palette index, #rgb / #rrggbb, "" for the terminal's own, a variable,
 * okhsl() / okhsl() (decoded by the caller's hook) -- and a faint flag. The
 * ONE place a colour is brought down to this screen's palette is
 * pitheme_sgr(): hl_rgb_to_16 / hl_rgb_to_256 by the depth (16: the
 * window's own ANSI colours, UP-Term's default). pitheme_system() is pi's
 * default theme as pi draws it on a terminal that reports no colours: ANSI
 * indices 0-15, neutral roles faint, panels without a background. */
enum {
    PR_ACCENT, PR_BORDER, PR_BORDER_ACCENT, PR_BORDER_MUTED, PR_SUCCESS, PR_ERROR, PR_WARNING, PR_MUTED, PR_DIM,
    PR_TEXT, PR_THINKING_TEXT, PR_SELECTED_BG, PR_SCROLLBAR_TRACK, PR_SCROLLBAR_THUMB, PR_SEARCH_MATCH_BG,
    PR_SEARCH_MATCH_TEXT, PR_USER_MESSAGE_BG, PR_USER_MESSAGE_TEXT, PR_CUSTOM_MESSAGE_BG, PR_CUSTOM_MESSAGE_TEXT,
    PR_CUSTOM_MESSAGE_LABEL, PR_TOOL_PENDING_BG, PR_TOOL_SUCCESS_BG, PR_TOOL_ERROR_BG, PR_TOOL_TITLE,
    PR_TOOL_OUTPUT, PR_MD_HEADING, PR_MD_LINK, PR_MD_LINK_URL, PR_MD_CODE, PR_MD_CODE_BLOCK,
    PR_MD_CODE_BLOCK_BORDER, PR_MD_QUOTE, PR_MD_QUOTE_BORDER, PR_MD_HR, PR_MD_LIST_BULLET, PR_TOOL_DIFF_ADDED,
    PR_TOOL_DIFF_REMOVED, PR_TOOL_DIFF_CONTEXT, PR_SYNTAX_COMMENT, PR_SYNTAX_KEYWORD, PR_SYNTAX_FUNCTION,
    PR_SYNTAX_VARIABLE, PR_SYNTAX_STRING, PR_SYNTAX_NUMBER, PR_SYNTAX_TYPE, PR_SYNTAX_OPERATOR,
    PR_SYNTAX_PUNCTUATION, PR_THINKING_OFF, PR_THINKING_MINIMAL, PR_THINKING_LOW, PR_THINKING_MEDIUM,
    PR_THINKING_HIGH, PR_THINKING_XHIGH, PR_THINKING_MAX, PR_BASH_MODE,
    PR_COUNT
};

typedef struct cl_pitheme {
    char name[64];
    long color[PR_COUNT];       /* hl_style.h: HL_DEFAULT, 0..255, HL_RGB() */
    unsigned char dim[PR_COUNT];/* drawn faint (SGR 2) on top */
    int depth;                  /* 16 or 256: what pitheme_sgr sends */
} cl_pitheme;

extern const char *const pi_role_names[PR_COUNT];
/* the role of that JSON name, -1 none */
int pitheme_role(const char *name);
/* pi's default theme (see above) */
void pitheme_system(cl_pitheme *t);
/* a pi theme file (themes.md): 0, or -1 with err (pi's wording: a missing
 * required role, a missing or circular variable, an unreadable colour). ext
 * decodes the colour spellings theme.c does not (okhsl(), oklch()): 1 and
 * r, g, b, 0 not that spelling, -1 invalid; 0: those are errors. */
typedef int (*pitheme_ext)(const char *s, int *r, int *g, int *b);
int pitheme_load(cl_pitheme *t, const char *json, long n, pitheme_ext ext, char *err, int errlen);
/* the SGR that sets role r (the foreground; a *_BG role: the background),
 * "" for the terminal's own colour (and SGR 2 first for a faint role): the
 * length. out holds 48 bytes. */
int pitheme_sgr(const cl_pitheme *t, int role, char *out);
/* a colour (HL_DEFAULT, 0..255, HL_RGB) as the SGR that sets it: the one place a colour of pi's is brought
 * down to this screen's palette (t->depth). out holds 48 bytes. */
int pitheme_color(const cl_pitheme *t, long color, int bg, char *out);
/* the md / code classes of an hl_theme coloured by the roles (view/hl_style.h) */
struct hl_theme;
void pitheme_hl(const cl_pitheme *t, struct hl_theme *h);

#endif
