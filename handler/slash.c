#include <string.h>
#include "slash.h"

#define ONOFF(id) { { "on", (id), 1 }, { "off", (id), 0 }, { 0, 0, 0 } }

static const slash_value v_cursor[] = {
    { "block", MENU_SET_BLOCK, 1 }, { "underline", MENU_SET_UNDERLINE, 1 }, { "bar", MENU_SET_BAR, 1 },
    { 0, 0, 0 }
};
static const slash_value v_blink[] = ONOFF(MENU_SET_BLINK);
static const slash_value v_bell[] = {
    { "none", MENU_SET_BELL_NONE, 1 }, { "beep", MENU_SET_BELL_BEEP, 1 },
    { "visual", MENU_SET_BELL_VISUAL, 1 }, { 0, 0, 0 }
};
static const slash_value v_bold[] = ONOFF(MENU_SET_BOLD);
static const slash_value v_meta[] = {
    { "amiga", MENU_SET_META_AMIGA, 1 }, { "alt", MENU_SET_META_ALT, 1 }, { 0, 0, 0 }
};
static const slash_value v_copy[] = ONOFF(MENU_SET_COPY);
static const slash_value v_reflow[] = ONOFF(MENU_SET_REFLOW);
static const slash_value v_backspace[] = {
    { "del", MENU_SET_BS_DEL, 1 }, { "bs", MENU_SET_BS_BS, 1 }, { 0, 0, 0 }
};
static const slash_value v_clipboard[] = {
    { "write", MENU_SET_CLIP_WRITE, 1 }, { "read-write", MENU_SET_CLIP_READ_WRITE, 1 },
    { "off", MENU_SET_CLIP_OFF, 1 }, { 0, 0, 0 }
};
static const slash_value v_wheel[] = {
    { "scroll", MENU_SET_WHEEL, 1 }, { "ignore", MENU_SET_WHEEL, 0 }, { 0, 0, 0 }
};
static const slash_value v_completion[] = {
    { "unix", MENU_SET_UNIX, 1 }, { "kingcon", MENU_SET_KINGCON, 1 }, { 0, 0, 0 }
};
static const slash_value v_kcinfo[] = {
    { "show", MENU_KC_INFO, 1 }, { "hide", MENU_KC_INFO, 0 }, { 0, 0, 0 }
};
static const slash_value v_kccache[] = {
    { "on", MENU_KC_CACHE, 1 }, { "off", MENU_KC_CACHE, 0 }, { "reset", MENU_KC_RESET, 1 },
    { "purge", MENU_KC_PURGE, 1 }, { 0, 0, 0 }
};
static const slash_value v_clear[] = {
    { "screen", MENU_CLEAR_SCREEN, 1 }, { "scrollback", MENU_CLEAR_SB, 1 }, { 0, 0, 0 }
};
static const slash_value v_fontsize[] = {
    { "bigger", MENU_FONT_BIGGER, 1 }, { "smaller", MENU_FONT_SMALLER, 1 }, { 0, 0, 0 }
};
static const slash_value v_screen[] = {
    { "workbench", MENU_SCREEN_WB, 1 }, { "own", MENU_SCREEN_OWN, 1 },
    { "fullscreen", MENU_SCREEN_FULL, 1 }, { 0, 0, 0 }
};
static const slash_value v_tab[] = {
    { "new", MENU_TAB_NEW, 1 }, { "next", MENU_TAB_NEXT, 1 }, { "previous", MENU_TAB_PREV, 1 },
    { "close", MENU_TAB_CLOSE, 1 }, { 0, 0, 0 }
};

/* Alphabetical: the help lists it in this order, completion offers it so. */
static const slash_def table[] = {
    { "about", SL_ACTION, MENU_ABOUT, 0, 0, "", "the build" },
    { "backspace", SL_CHOICE, 0, 0, v_backspace, "del | bs", "what the Backspace key sends (^? or ^H)" },
    { "bell", SL_CHOICE, 0, 0, v_bell, "none | beep | visual", "the bell" },
    { "bg", SL_ARG, SLASH_BG, 1, 0, "RRGGBB | none", "the background colour" },
    { "bold-bright", SL_CHOICE, 0, 0, v_bold, "on | off", "bold text takes the bright colours (xterm)" },
    { "clear", SL_CHOICE, 0, 0, v_clear, "screen | scrollback", "clear the screen or the scrollback" },
    { "completion", SL_CHOICE, 0, 0, v_completion, "unix | kingcon", "the Tab key's completion style" },
    { "copy", SL_ACTION, MENU_COPY, 0, 0, "", "copy the selection to the clipboard" },
    { "copy-on-select", SL_CHOICE, 0, 0, v_copy, "on | off", "a drag ends with the text on the clipboard" },
    { "cursor", SL_CHOICE, 0, 0, v_cursor, "block | underline | bar", "the cursor's shape" },
    { "cursor-blink", SL_CHOICE, 0, 0, v_blink, "on | off", "the cursor blinks" },
    { "cursor-color", SL_ARG, SLASH_CURSOR_COLOR, 1, 0, "RRGGBB | none", "the cursor's colour (none: inverted)" },
    { "fg", SL_ARG, SLASH_FG, 1, 0, "RRGGBB | none", "the text colour" },
    { "find", SL_ARG, SLASH_FIND, 0, 0, "[TEXT]", "find in the scrollback (again: the next)" },
    { "font", SL_ARG, SLASH_FONT, 0, 0, "[NAME SIZE]", "the font (no name: the requester)" },
    { "font-fallback", SL_ARG, SLASH_FALLBACK, 1, 0, "NAME | none",
      "an outline font for what the font cannot show" },
    { "font-size", SL_CHOICE, 0, 0, v_fontsize, "bigger | smaller", "the font's next designed size" },
    { "help", SL_ARG, SLASH_HELP, 0, 0, "[COMMAND]", "these commands" },
    { "kingcon-cache", SL_CHOICE, 0, 0, v_kccache, "on | off | reset | purge", "KingCON's directory cache" },
    { "kingcon-info", SL_CHOICE, 0, 0, v_kcinfo, "show | hide", "KingCON lists .info files" },
    { "kingcon-mode", SL_ARG, SLASH_KC_MODE, 1, 0, "LETTERS (W L B C S)", "KingCON's completion style" },
    { "link-open", SL_ARG, SLASH_LINK_OPEN, 1, 0, "COMMAND | none",
      "what a Ctrl + click on a link runs (%s: the URL)" },
    { "meta", SL_CHOICE, 0, 0, v_meta, "amiga | alt", "the key that is Meta (ESC prefix)" },
    { "paste", SL_ACTION, MENU_PASTE, 0, 0, "", "type the clipboard in" },
    { "prefs", SL_ACTION, MENU_PREFS, 0, 0, "", "open UP-Term Prefs" },
    { "profile", SL_ARG, SLASH_PROFILE, 1, 0, "NAME", "switch to a profile" },
    { "program-clipboard", SL_CHOICE, 0, 0, v_clipboard, "write | read-write | off",
      "programs' clipboard access (OSC 52)" },
    { "reflow", SL_CHOICE, 0, 0, v_reflow, "on | off", "a resize re-wraps the lines and the scrollback" },
    { "reset", SL_ACTION, MENU_RESET, 0, 0, "", "reset the terminal (RIS)" },
    { "save", SL_ACTION, MENU_SET_SAVE, 0, 0, "", "save the settings to the profile" },
    { "screen", SL_CHOICE, 0, 0, v_screen, "workbench | own | fullscreen",
      "the window on the Workbench, a screen of its own, or full screen" },
    { "scrollback", SL_ARG, SLASH_SCROLLBACK, 1, 0, "LINES | none", "the lines kept" },
    { "select-all", SL_ACTION, MENU_SELECT_ALL, 0, 0, "", "select the scrollback and the screen" },
    { "selection-bg", SL_ARG, SLASH_SEL_BG, 1, 0, "RRGGBB | none", "the selection's background" },
    { "selection-fg", SL_ARG, SLASH_SEL_FG, 1, 0, "RRGGBB | none", "the selected text's colour" },
    { "size", SL_ARG, SLASH_SIZE, 1, 0, "COLSxROWS", "the window sized to the grid (80x24)" },
    { "tab", SL_CHOICE, 0, 0, v_tab, "new | next | previous | close", "tabs" },
    { "theme", SL_ARG, SLASH_THEME, 0, 0, "[NAME]", "a colour theme (no name: the requester)" },
    { "wheel", SL_CHOICE, 0, 0, v_wheel, "scroll | ignore", "the mouse wheel moves the scrollback" }
};

#define NDEFS ((int)(sizeof(table) / sizeof(table[0])))

const slash_def *slash_table(int *n)
{
    *n = NDEFS;
    return table;
}

static int is_blank(char ch)
{
    return ch == ' ' || ch == '\t';
}

static int name_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || ch == '-';
}

static const slash_def *find_def(const char *name, int n)
{
    int i;
    for (i = 0; i < NDEFS; i++)
        if ((int)strlen(table[i].name) == n && !strncmp(table[i].name, name, (size_t)n))
            return &table[i];
    return 0;
}

/* err = a + b + c, cut to cap */
static void say(char *err, int cap, const char *a, const char *b, const char *c)
{
    int k = 0;
    const char *p[3];
    int i;
    p[0] = a;
    p[1] = b;
    p[2] = c;
    for (i = 0; i < 3; i++)
        for (; p[i] && *p[i] && k < cap - 1; p[i]++)
            err[k++] = *p[i];
    if (cap > 0)
        err[k] = 0;
}

int slash_parse(const char *line, int len, slash_cmd *cmd, char *err, int errcap)
{
    int i, ne, a, b;
    const slash_def *d;
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        len--;
    if (len < 2 || line[0] != '/' || !(line[1] >= 'a' && line[1] <= 'z'))
        return SLASH_NOT_OURS;
    for (ne = 1; ne < len && name_char(line[ne]); ne++)
        ;
    if (ne < len && !is_blank(line[ne]))
        return SLASH_NOT_OURS; /* "/c/dir", "/work.info": a path */
    d = find_def(line + 1, ne - 1);
    if (!d)
        return SLASH_NOT_OURS; /* a program in the parent directory, say */
    for (a = ne; a < len && is_blank(line[a]); a++)
        ;
    for (b = len; b > a && is_blank(line[b - 1]); b--)
        ;
    cmd->def = d;
    cmd->id = d->id;
    cmd->on = 1;
    cmd->arg[0] = 0;
    if (d->kind == SL_CHOICE) {
        for (i = 0; d->values[i].word; i++)
            if ((int)strlen(d->values[i].word) == b - a && !strncmp(d->values[i].word, line + a, (size_t)(b - a))) {
                cmd->id = d->values[i].id;
                cmd->on = d->values[i].on;
                strcpy(cmd->arg, d->values[i].word); /* for the answer */
                return SLASH_OK;
            }
        say(err, errcap, d->name, ": ", d->args);
        return SLASH_ERROR;
    }
    if (d->kind == SL_ACTION) {
        if (b > a) {
            say(err, errcap, d->name, ": takes nothing after it", 0);
            return SLASH_ERROR;
        }
        return SLASH_OK;
    }
    if (b - a >= (int)sizeof(cmd->arg)) {
        say(err, errcap, d->name, ": too long", 0);
        return SLASH_ERROR;
    }
    if (d->need_arg && b == a) {
        say(err, errcap, d->name, ": ", d->args);
        return SLASH_ERROR;
    }
    memcpy(cmd->arg, line + a, (size_t)(b - a));
    cmd->arg[b - a] = 0;
    return SLASH_OK;
}

/* one help entry: "  /name args", the description at column 32 (an
 * 80-column window holds the longest); a longer command puts it on the
 * next line, at the same column */
static int help_line(const slash_def *d, char *out, int cap)
{
    char t[200];
    int n, col = 0;
    say(t, sizeof(t), "  /", d->name, d->args[0] ? " " : "");
    n = (int)strlen(t);
    say(t + n, (int)sizeof(t) - n, d->args, 0, 0);
    n = (int)strlen(t);
    col = n;
    if (col > 30) {
        t[n++] = '\n';
        col = 0;
    }
    while (col < 32 && n < (int)sizeof(t) - 1) {
        t[n++] = ' ';
        col++;
    }
    t[n] = 0;
    say(t + n, (int)sizeof(t) - n, d->what, "\n", 0);
    n = (int)strlen(t);
    if (n >= cap)
        return -1;
    memcpy(out, t, (size_t)n + 1);
    return n;
}

int slash_help(const char *name, char *out, int cap)
{
    int i, k = 0, n;
    if (name && name[0]) {
        const slash_def *d = find_def(name[0] == '/' ? name + 1 : name,
                                      (int)strlen(name[0] == '/' ? name + 1 : name));
        return d ? help_line(d, out, cap) : -1;
    }
    say(out, cap, "UP-Term commands (a leading blank sends the line to the program as it is):\n", 0, 0);
    k = (int)strlen(out);
    for (i = 0; i < NDEFS; i++) {
        n = help_line(&table[i], out + k, cap - k);
        if (n < 0)
            break;
        k += n;
    }
    return k;
}

static int add(char *out, int cap, int *k, const char *pre, const char *word)
{
    int n = (int)strlen(pre) + (int)strlen(word);
    if (*k + n + 1 > cap)
        return 0;
    strcpy(out + *k, pre);
    strcat(out + *k, word);
    *k += n + 1;
    return 1;
}

int slash_complete(const char *line, int len, const char *const *extra, int nextra, char *out, int cap,
                   int *from)
{
    int ne, a, i, k = 0, count = 0;
    const slash_def *d;
    if (len < 1 || line[0] != '/')
        return 0;
    for (ne = 1; ne < len && name_char(line[ne]); ne++)
        ;
    if (ne == len) {
        /* the name itself */
        *from = 0;
        for (i = 0; i < NDEFS; i++)
            if (!strncmp(table[i].name, line + 1, (size_t)(len - 1)) && add(out, cap, &k, "/", table[i].name))
                count++;
        return count;
    }
    if (!is_blank(line[ne]) || !(d = find_def(line + 1, ne - 1)))
        return 0;
    for (a = ne; a < len && is_blank(line[a]); a++)
        ;
    for (i = a; i < len; i++)
        if (is_blank(line[i]))
            return 0; /* past the value: nothing to offer */
    *from = a;
    if (d->kind == SL_CHOICE) {
        for (i = 0; d->values[i].word; i++)
            if (!strncmp(d->values[i].word, line + a, (size_t)(len - a)) && add(out, cap, &k, "", d->values[i].word))
                count++;
    } else if (d->id == SLASH_PROFILE || d->id == SLASH_HELP) {
        if (d->id == SLASH_HELP) {
            for (i = 0; i < NDEFS; i++)
                if (!strncmp(table[i].name, line + a, (size_t)(len - a)) && add(out, cap, &k, "", table[i].name))
                    count++;
        } else {
            for (i = 0; i < nextra; i++)
                if (!strncmp(extra[i], line + a, (size_t)(len - a)) && add(out, cap, &k, "", extra[i]))
                    count++;
        }
    } else if (d->id == SLASH_SCROLLBACK || d->id == SLASH_FALLBACK || d->id == SLASH_FG ||
               d->id == SLASH_BG || d->id == SLASH_CURSOR_COLOR || d->id == SLASH_SEL_FG ||
               d->id == SLASH_SEL_BG || d->id == SLASH_LINK_OPEN) {
        if (!strncmp("none", line + a, (size_t)(len - a)) && add(out, cap, &k, "", "none"))
            count++;
    }
    return count;
}
