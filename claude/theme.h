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

#endif
