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
static const slash_value v_amigakeys[] = ONOFF(MENU_SET_AMIGA_KEYS);
static const slash_value v_dither[] = ONOFF(MENU_SET_DITHER);
static const slash_value v_outline_aa[] = ONOFF(MENU_SET_OUTLINE_AA);
static const slash_value v_padding[] = {
    { "background", MENU_SET_PAD_BG, 1 }, { "extend", MENU_SET_PAD_EXTEND, 1 }, { 0, 0, 0 }
};
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
static const slash_value v_scrollbar[] = {
    { "show", MENU_SET_SCROLLBAR, 1 }, { "hide", MENU_SET_SCROLLBAR, 0 }, { 0, 0, 0 }
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
static const slash_value v_agent[] = {
    { "claude", MENU_SET_AGENT_CLAUDE, 1 }, { "pi", MENU_SET_AGENT_PI, 1 }, { 0, 0, 0 }
};
static const slash_value v_tab[] = {
    { "new", MENU_TAB_NEW, 1 }, { "next", MENU_TAB_NEXT, 1 }, { "previous", MENU_TAB_PREV, 1 },
    { "close", MENU_TAB_CLOSE, 1 }, { 0, 0, 0 }
};

/* Alphabetical: the help lists it in this order, completion offers it so. */
static const slash_def table[] = {
    { "about", SL_ACTION, MENU_ABOUT, 0, 0, "", "the build" },
    { "agent", SL_CHOICE, 0, 0, v_agent, "claude | pi", "the AI agent the Super + L key opens" },
    { "amiga-keys", SL_CHOICE, 0, 0, v_amigakeys, "on | off",
      "Shift+Left / Right send Home / End, as in the AmigaShell (off: xterm's codes)" },
    { "ask", SL_ARG, SLASH_ASK, 0, 0, "[TASK]", "the AI agent in a new tab, started on TASK" },
    { "backspace", SL_CHOICE, 0, 0, v_backspace, "del | bs", "what the Backspace key sends (^? or ^H)" },
    { "bell", SL_CHOICE, 0, 0, v_bell, "none | beep | visual", "the bell" },
    { "bg", SL_ARG, SLASH_BG, 1, 0, "RRGGBB | none", "the background colour" },
    { "bold-bright", SL_CHOICE, 0, 0, v_bold, "on | off", "bold text takes the bright colours (xterm)" },
    { "claude", SL_ACTION, MENU_CLAUDE, 0, 0, "", "Claude, in a new tab" },
    { "clear", SL_CHOICE, 0, 0, v_clear, "screen | scrollback", "clear the screen or the scrollback" },
    { "colors", SL_ARG, SLASH_COLORS, 1, 0, "rgb | 256 | 16 | none",
      "the colour depth programs are told (rgb: COLORTERM=truecolor, 16: TERM=xterm-16color)" },
    { "completion", SL_CHOICE, 0, 0, v_completion, "unix | kingcon", "the Tab key's completion style" },
    { "copy", SL_ACTION, MENU_COPY, 0, 0, "", "copy the selection to the clipboard" },
    { "copy-on-select", SL_CHOICE, 0, 0, v_copy, "on | off", "a drag ends with the text on the clipboard" },
    { "cursor", SL_CHOICE, 0, 0, v_cursor, "block | underline | bar", "the cursor's shape" },
    { "cursor-blink", SL_CHOICE, 0, 0, v_blink, "on | off", "the cursor blinks" },
    { "cursor-color", SL_ARG, SLASH_CURSOR_COLOR, 1, 0, "RRGGBB | none", "the cursor's colour (none: inverted)" },
    { "demo", SL_ACTION, MENU_DEMO, 0, 0, "", "the tour of what UP-Term does, in a new tab" },
    { "dither", SL_CHOICE, 0, 0, v_dither, "on | off",
      "a screen of 16 colours or fewer dithers colours no pen shows closely (off: the nearest pen)" },
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
    { "outline-antialias", SL_CHOICE, 0, 0, v_outline_aa, "on | off",
      "outline-font glyphs are drawn smoothed on a screen of 16 colours or more (off: hard edges)" },
    { "padding-color", SL_CHOICE, 0, 0, v_padding, "background | extend",
      "the strips beside the character grid: the background, or the colour of the cell next to them" },
    { "paste", SL_ACTION, MENU_PASTE, 0, 0, "", "type the clipboard in" },
    { "pi", SL_ACTION, MENU_PI, 0, 0, "", "pi, in a new tab" },
    { "prefs", SL_ACTION, MENU_PREFS, 0, 0, "", "open UP-Term Prefs" },
    { "profile", SL_ARG, SLASH_PROFILE, 1, 0, "NAME", "switch to a profile" },
    { "program-clipboard", SL_CHOICE, 0, 0, v_clipboard, "write | read-write | off",
      "programs' clipboard access (OSC 52)" },
    { "reflow", SL_CHOICE, 0, 0, v_reflow, "on | off", "a resize re-wraps the lines and the scrollback" },
    { "reset", SL_ACTION, MENU_RESET, 0, 0, "", "reset the terminal (RIS)" },
    { "save", SL_ACTION, MENU_SET_SAVE, 0, 0, "", "save the settings to the profile" },
    { "screen", SL_CHOICE, 0, 0, v_screen, "workbench | own | fullscreen",
      "the window on the Workbench, a screen of its own, or full screen" },
    { "screen-text", SL_ARG, SLASH_SCREEN_TEXT, 1, 0, "FILE", "write the visible rows as text, one line per row, to FILE (UPTerm screen FILE x)" },
    { "scrollback", SL_ARG, SLASH_SCROLLBACK, 1, 0, "LINES | none", "the lines kept" },
    { "scrollbar", SL_CHOICE, 0, 0, v_scrollbar, "show | hide", "the scroll bar in the window's border" },
    { "select-all", SL_ACTION, MENU_SELECT_ALL, 0, 0, "", "select the scrollback and the screen" },
    { "selection-bg", SL_ARG, SLASH_SEL_BG, 1, 0, "RRGGBB | none", "the selection's background" },
    { "selection-fg", SL_ARG, SLASH_SEL_FG, 1, 0, "RRGGBB | none", "the selected text's colour" },
    { "setup", SL_ACTION, SLASH_SETUP, 0, 0, "", "what this window has set, and where each came from" },
    { "size", SL_ARG, SLASH_SIZE, 1, 0, "COLSxROWS", "the window sized to the grid (80x24)" },
    { "state", SL_ARG, SLASH_STATE, 0, 0, "[KEY]", "what the user did in this window (counters; UPTerm state)" },
    { "tab", SL_CHOICE, 0, 0, v_tab, "new | next | previous | close", "tabs" },
    { "term", SL_ARG, SLASH_TERM, 1, 0, "NAME | none",
      "TERM for programs vsh starts here (vtcon, xterm-256color, screen-256color)" },
    { "theme", SL_ARG, SLASH_THEME, 0, 0, "[NAME]", "a colour theme (no name: a list, Up/Down and Enter)" },
    { "tutorial", SL_ACTION, MENU_TUTORIAL, 0, 0, "", "the shell tutorial, in a new tab" },
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
               d->id == SLASH_SEL_BG || d->id == SLASH_LINK_OPEN || d->id == SLASH_TERM ||
               d->id == SLASH_COLORS) {
        if (!strncmp("none", line + a, (size_t)(len - a)) && add(out, cap, &k, "", "none"))
            count++;
    }
    return count;
}

/* ---- W46: /term, /colors, /setup ---------------------------------------- */

int slash_term_ok(const char *name)
{
    int n = 0;
    for (; name[n]; n++) {
        char c = name[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '+' || c == '-'))
            return 0;
    }
    return n >= 1 && n <= 31;
}

int slash_colors_ok(const char *name)
{
    return !strcmp(name, "rgb") || !strcmp(name, "256") || !strcmp(name, "16");
}

long slash_env_list(const char *term, const char *colors, const char *dflt_term, int utf8, char *out, long max)
{
    long k = 0;
    const char *e[3];
    char t[48];
    int i, ne = 0, c16 = !strcmp(colors, "16");
    if (!term[0] || c16)
        term = dflt_term;
    if (c16 && !term[0])
        term = "xterm";
    if (term[0] && strlen(term) < 32) {
        strcpy(t, "TERM=");
        strcat(t, term);
        if (c16)
            strcat(t, "-16color"); /* the kit's entries: tools/gen_16color_terminfo.py */
        e[ne++] = t;
    }
    if (!strcmp(colors, "rgb"))
        e[ne++] = "COLORTERM=truecolor";
    else if (!strcmp(colors, "256") || c16)
        e[ne++] = "COLORTERM=";
    if (utf8)
        e[ne++] = "LC_CTYPE=C.UTF-8";
    for (i = 0; i < ne; i++) {
        long n = (long)strlen(e[i]) + 1;
        if (k + n + 1 > max)
            return 0;
        memcpy(out + k, e[i], (size_t)n);
        k += n;
    }
    if (!k)
        return 0;
    out[k++] = 0;
    return k;
}

static const struct {
    long id;
    int is_term;
    const char *value; /* the command's argument; "none" clears */
} menu_settings[] = {
    { MENU_SET_TERM_VTCON, 1, "vtcon" },
    { MENU_SET_TERM_XTERM, 1, "xterm-256color" },
    { MENU_SET_TERM_SCREEN, 1, "screen-256color" },
    { MENU_SET_TERM_NONE, 1, "none" },
    { MENU_SET_COLORS_RGB, 0, "rgb" },
    { MENU_SET_COLORS_256, 0, "256" },
    { MENU_SET_COLORS_16, 0, "16" },
    { MENU_SET_COLORS_NONE, 0, "none" }
};

int slash_menu_line(long id, char *out, int cap)
{
    unsigned i;
    if (id == MENU_HELP_SLASH) { /* Help > Slash commands: the typed /help */
        if (cap < 6)
            return 0;
        strcpy(out, "/help");
        return 1;
    }
    for (i = 0; i < sizeof(menu_settings) / sizeof(menu_settings[0]); i++)
        if (menu_settings[i].id == id) {
            const char *cmd = menu_settings[i].is_term ? "/term " : "/colors ";
            if ((int)(strlen(cmd) + strlen(menu_settings[i].value)) >= cap)
                return 0;
            strcpy(out, cmd);
            strcat(out, menu_settings[i].value);
            return 1;
        }
    return 0;
}

int slash_menu_help_where(int raw, int tty)
{
    return raw || tty ? SLASH_HELP_REQUESTER : SLASH_HELP_PRINT;
}

int slash_menu_checked(long id, const char *term, const char *colors)
{
    unsigned i;
    for (i = 0; i < sizeof(menu_settings) / sizeof(menu_settings[0]); i++)
        if (menu_settings[i].id == id) {
            const char *cur = menu_settings[i].is_term ? term : colors;
            if (!strcmp(menu_settings[i].value, "none"))
                return cur[0] == 0;
            return !strcmp(cur, menu_settings[i].value);
        }
    return -1;
}

const char *slash_source(const char *value, const char *profile, const char *dflt)
{
    const char *base = profile ? profile : dflt;
    if (strcmp(value, base))
        return "this window";
    return profile ? "profile" : "default";
}

int slash_setup_text(const slash_row *rows, int n, char *out, int cap)
{
    int i, k = 0, w = 0;
    out[0] = 0;
    for (i = 0; i < n; i++)
        if ((int)strlen(rows[i].name) > w)
            w = (int)strlen(rows[i].name);
    for (i = 0; i < n; i++) {
        const char *v = rows[i].value[0] ? rows[i].value : "-";
        int pad = w - (int)strlen(rows[i].name) + 2;
        int need = (int)strlen(rows[i].name) + pad + (int)strlen(v) + (int)strlen(rows[i].source) + 5;
        if (k + need >= cap)
            break;
        strcpy(out + k, rows[i].name);
        k += (int)strlen(rows[i].name);
        while (pad-- > 0)
            out[k++] = ' ';
        strcpy(out + k, v);
        k += (int)strlen(v);
        strcpy(out + k, "  [");
        k += 3;
        strcpy(out + k, rows[i].source);
        k += (int)strlen(rows[i].source);
        out[k++] = ']';
        out[k++] = '\n';
        out[k] = 0;
    }
    return k;
}

/* ---- W47: the action counters ---- */

void slash_state_bump(slash_state *s, int ev, long arg, const char *name)
{
    switch (ev) {
    case STATE_TAB_OPENED: s->tabs_opened++; break;
    case STATE_SWITCH: s->tab_switches++; break;
    case STATE_MENU: s->menu_picks++; s->last_menu = arg; break;
    case STATE_SLASH:
        s->slash_count++;
        strncpy(s->last_slash, name ? name : "", sizeof(s->last_slash) - 1);
        s->last_slash[sizeof(s->last_slash) - 1] = 0;
        break;
    case STATE_COPY: s->copies++; break;
    case STATE_PASTE: s->pastes++; break;
    case STATE_KEYMAP: s->km_hits++; s->last_km = arg; break;
    }
}

int slash_line_name(const char *line, int len, char *out, int cap)
{
    int i = 0;
    if (len < 2 || line[0] != '/' || cap < 2)
        return 0;
    while (i + 1 < len && i + 1 < cap && name_char(line[i + 1])) {
        out[i] = line[i + 1];
        i++;
    }
    out[i] = 0;
    return i;
}

static const char *const state_keys[] = { "tabs-opened", "tab-switches", "menu-picks", "last-menu", "slash-count",
                                           "last-slash", "copies", "pastes", "tabs", "active", "km-hits", "last-key", "font-size",
                                           "outline-own", "outline-aa" };

static void put_long(char *v, long n)
{
    char tmp[16];
    int k = 0, j = 0;
    unsigned long u = n < 0 ? (unsigned long)-n : (unsigned long)n;
    do
        tmp[k++] = (char)('0' + u % 10);
    while ((u /= 10) != 0);
    if (n < 0)
        v[j++] = '-';
    while (k)
        v[j++] = tmp[--k];
    v[j] = 0;
}

static void state_value(const slash_state *s, int k, char *v)
{
    long n = 0;
    switch (k) {
    case 0: n = s->tabs_opened; break;
    case 1: n = s->tab_switches; break;
    case 2: n = s->menu_picks; break;
    case 3: n = s->last_menu; break;
    case 4: n = s->slash_count; break;
    case 5: strcpy(v, s->last_slash[0] ? s->last_slash : "-"); return;
    case 6: n = s->copies; break;
    case 7: n = s->pastes; break;
    case 8: n = s->tabs; break;
    case 9: n = s->active; break;
    case 10: n = s->km_hits; break;
    case 11: n = s->last_km; break;
    case 12: n = s->font_size; break;
    case 13: n = s->outline_own; break;
    case 14: n = s->outline_aa; break;
    }
    put_long(v, n);
}

/* append str to out at *k (cap bytes): 0 when it does not fit with a closing NUL */
static int put_str(char *out, int cap, int *k, const char *str)
{
    int l = (int)strlen(str);
    if (*k + l >= cap)
        return 0;
    strcpy(out + *k, str);
    *k += l;
    return 1;
}

int slash_state_text(const slash_state *s, const char *key, char *out, int cap)
{
    int i, k = 0, n = (int)(sizeof(state_keys) / sizeof(state_keys[0]));
    char v[32];
    out[0] = 0;
    if (!key[0]) {
        for (i = 0; i < n; i++) {
            state_value(s, i, v);
            if (k + (int)strlen(state_keys[i]) + (int)strlen(v) + 2 >= cap)
                break; /* no half line */
            put_str(out, cap, &k, state_keys[i]);
            put_str(out, cap, &k, " ");
            put_str(out, cap, &k, v);
            put_str(out, cap, &k, "\n");
        }
        return k;
    }
    for (i = 0; i < n && strcmp(key, state_keys[i]); i++)
        ;
    if (i < n) {
        state_value(s, i, v);
        put_str(out, cap, &k, v);
        put_str(out, cap, &k, "\n");
        return k;
    }
    put_str(out, cap, &k, "state: no key ");
    put_str(out, cap, &k, key);
    put_str(out, cap, &k, "; keys:");
    for (i = 0; i < n; i++) {
        put_str(out, cap, &k, " ");
        put_str(out, cap, &k, state_keys[i]);
    }
    put_str(out, cap, &k, "\n");
    return -1;
}
