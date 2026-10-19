/* handler/slash: a typed /command is UP-Term's, a path stays the shell's. */
#include <string.h>
#include "harness.h"
#include "../handler/slash.h"

static slash_cmd cmd;
static char err[200];

static int parse(const char *s)
{
    err[0] = 0;
    return slash_parse(s, (int)strlen(s), &cmd, err, sizeof(err));
}

/* on AmigaDOS "/" is the parent directory: those lines are the shell's */
static void paths_stay_the_shells(void)
{
    CHECK_INT(parse("/"), SLASH_NOT_OURS);
    CHECK_INT(parse("/\n"), SLASH_NOT_OURS);
    CHECK_INT(parse("//"), SLASH_NOT_OURS);
    CHECK_INT(parse("/Work"), SLASH_NOT_OURS);          /* upper case: a name the user typed */
    CHECK_INT(parse("/c/dir"), SLASH_NOT_OURS);         /* a path through a drawer */
    CHECK_INT(parse("/cursor/x"), SLASH_NOT_OURS);
    CHECK_INT(parse("/cd"), SLASH_NOT_OURS);            /* not a command of ours */
    CHECK_INT(parse("//cursor bar"), SLASH_NOT_OURS);
    CHECK_INT(parse(" /cursor bar"), SLASH_NOT_OURS);   /* the leading blank: as typed */
    CHECK_INT(parse("cd /"), SLASH_NOT_OURS);
    CHECK_INT(parse("/cursor.info"), SLASH_NOT_OURS);
}

/* a setting runs the menu item it is */
static void settings_name_their_menu_items(void)
{
    CHECK_INT(parse("/cursor bar\n"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_BAR);
    CHECK_INT(parse("/cursor-blink off"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_BLINK);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/bell   visual  "), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_BELL_VISUAL);
    CHECK_INT(parse("/reflow off"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_REFLOW);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/wheel ignore"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_WHEEL);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/scrollbar hide"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_SCROLLBAR);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/scrollbar show"), SLASH_OK);
    CHECK_INT(cmd.on, 1);
    CHECK_INT(parse("/completion kingcon"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_KINGCON);
    CHECK_INT(parse("/kingcon-cache purge"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_KC_PURGE);
    CHECK_INT(parse("/tab previous"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_TAB_PREV);
    CHECK_INT(parse("/clear scrollback"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_CLEAR_SB);
    CHECK_INT(parse("/demo"), SLASH_OK); /* Help > Demo tour */
    CHECK_INT(cmd.id, MENU_DEMO);
    CHECK_INT(parse("/demo now"), SLASH_ERROR);
    CHECK_INT(parse("/claude"), SLASH_OK); /* UP-Term > Claude */
    CHECK_INT(cmd.id, MENU_CLAUDE);
    CHECK_INT(parse("/claude now"), SLASH_ERROR);
    CHECK_INT(parse("/tutorial"), SLASH_OK); /* Help > Shell tutorial */
    CHECK_INT(cmd.id, MENU_TUTORIAL);
    CHECK_INT(parse("/tutorial now"), SLASH_ERROR);
    CHECK_INT(parse("/font-size bigger"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_FONT_BIGGER);
    CHECK_INT(parse("/size 132x43"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_SIZE);
    CHECK_STR(cmd.arg, "132x43");
    CHECK_INT(parse("/save"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_SAVE);
    /* the profile keys the settings audit found with no command: their
     * values are the profile's words, each the menu item it is */
    CHECK_INT(parse("/backspace bs"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_BS_BS);
    CHECK_INT(parse("/backspace del"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_BS_DEL);
    CHECK_INT(parse("/amiga-keys off"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_AMIGA_KEYS);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/amiga-keys on"), SLASH_OK);
    CHECK_INT(cmd.on, 1);
    /* W-T9 and W-T14 */
    CHECK_INT(parse("/dither off"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_DITHER);
    CHECK_INT(cmd.on, 0);
    CHECK_INT(parse("/dither on"), SLASH_OK);
    CHECK_INT(cmd.on, 1);
    CHECK_INT(parse("/dither maybe"), SLASH_ERROR);
    CHECK_INT(parse("/padding-color extend"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_PAD_EXTEND);
    CHECK_INT(parse("/padding-color background"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_PAD_BG);
    CHECK_INT(parse("/padding-color"), SLASH_ERROR);
    CHECK_INT(parse("/padding-color edge"), SLASH_ERROR);
    CHECK_INT(parse("/program-clipboard read-write"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_CLIP_READ_WRITE);
    CHECK_INT(parse("/program-clipboard off"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_CLIP_OFF);
    CHECK_INT(parse("/program-clipboard write"), SLASH_OK);
    CHECK_INT(cmd.id, MENU_SET_CLIP_WRITE);
}

static void arguments_are_handed_on_trimmed(void)
{
    CHECK_INT(parse("/font  topaz 11 "), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_FONT);
    CHECK_STR(cmd.arg, "topaz 11");
    CHECK_INT(parse("/font"), SLASH_OK); /* no name: the requester */
    CHECK_STR(cmd.arg, "");
    CHECK_INT(parse("/scrollback 3000"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_SCROLLBACK);
    CHECK_STR(cmd.arg, "3000");
    CHECK_INT(parse("/profile vim"), SLASH_OK);
    CHECK_STR(cmd.arg, "vim");
    CHECK_INT(parse("/find two words"), SLASH_OK);
    CHECK_STR(cmd.arg, "two words");
    CHECK_INT(parse("/link-open  Run >NIL: OpenURL %s "), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_LINK_OPEN);
    CHECK_STR(cmd.arg, "Run >NIL: OpenURL %s");
}

static void a_wrong_value_says_what_fits(void)
{
    CHECK_INT(parse("/cursor round"), SLASH_ERROR);
    CHECK_STR(err, "cursor: block | underline | bar");
    CHECK_INT(parse("/cursor"), SLASH_ERROR);
    CHECK_INT(parse("/save now"), SLASH_ERROR);
    CHECK_STR(err, "save: takes nothing after it");
    CHECK_INT(parse("/scrollback"), SLASH_ERROR);
    CHECK_STR(err, "scrollback: LINES | none");
    CHECK_INT(parse("/backspace ^H"), SLASH_ERROR);
    CHECK_STR(err, "backspace: del | bs");
    CHECK_INT(parse("/program-clipboard read"), SLASH_ERROR);
    CHECK_STR(err, "program-clipboard: write | read-write | off");
    CHECK_INT(parse("/link-open"), SLASH_ERROR);
    CHECK_STR(err, "link-open: COMMAND | none");
}

static void the_help_lists_every_command(void)
{
    static char out[4096];
    int n, k, i;
    const slash_def *t = slash_table(&k);
    n = slash_help("", out, sizeof(out));
    CHECK(n > 0);
    for (i = 0; i < k; i++) {
        char want[40];
        strcpy(want, "/");
        strcat(want, t[i].name);
        CHECK(strstr(out, want) != 0);
    }
    for (i = 1; i < k; i++)
        CHECK(strcmp(t[i - 1].name, t[i].name) < 0); /* sorted: completion offers them so */
    n = slash_help("cursor", out, sizeof(out));
    CHECK(n > 0 && strstr(out, "block | underline | bar") != 0);
    CHECK_INT(slash_help("nope", out, sizeof(out)), -1);
}

static void tab_completes_names_then_values(void)
{
    static const char *const profiles[] = { "default", "vim", "vintage" };
    char out[512];
    int from = -1, n;
    n = slash_complete("/cur", 4, 0, 0, out, sizeof(out), &from);
    CHECK_INT(n, 3);
    CHECK_INT(from, 0);
    CHECK_STR(out, "/cursor");
    CHECK_STR(out + 8, "/cursor-blink");
    n = slash_complete("/cursor b", 9, 0, 0, out, sizeof(out), &from);
    CHECK_INT(n, 2); /* block, bar */
    CHECK_INT(from, 8);
    CHECK_STR(out, "block");
    n = slash_complete("/profile vi", 11, profiles, 3, out, sizeof(out), &from);
    CHECK_INT(n, 2);
    CHECK_STR(out, "vim");
    n = slash_complete("/scrollback ", 12, 0, 0, out, sizeof(out), &from);
    CHECK_INT(n, 1);
    CHECK_STR(out, "none");
    n = slash_complete("/program-clipboard r", 20, 0, 0, out, sizeof(out), &from);
    CHECK_INT(n, 1);
    CHECK_STR(out, "read-write");
    n = slash_complete("/link-open n", 12, 0, 0, out, sizeof(out), &from);
    CHECK_INT(n, 1);
    CHECK_STR(out, "none");
    CHECK_INT(slash_complete("/Wo", 3, 0, 0, out, sizeof(out), &from), 0);
    CHECK_INT(slash_complete("dir /", 5, 0, 0, out, sizeof(out), &from), 0);
}

/* W46: /term and /colors tell vsh's programs, /setup says where values came from */
static void term_colors_and_setup_are_wired_and_pure(void)
{
    char buf[128];
    slash_row rows[3];
    long n;
    CHECK_INT(parse("/term xterm-256color"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_TERM);
    CHECK_STR(cmd.arg, "xterm-256color");
    CHECK_INT(parse("/term"), SLASH_ERROR);
    CHECK_INT(parse("/colors rgb"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_COLORS);
    CHECK_INT(parse("/setup"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_SETUP);
    CHECK_INT(slash_term_ok("screen-256color"), 1);
    CHECK_INT(slash_term_ok("vtcon"), 1);
    CHECK_INT(slash_term_ok(""), 0);
    CHECK_INT(slash_term_ok("a b"), 0);
    CHECK_INT(slash_term_ok("x;rm"), 0);
    CHECK_INT(slash_term_ok("0123456789012345678901234567890123"), 0);
    CHECK_INT(slash_colors_ok("rgb"), 1);
    CHECK_INT(slash_colors_ok("256"), 1);
    CHECK_INT(slash_colors_ok("16"), 1);
    CHECK_INT(slash_colors_ok("8"), 0);
    n = slash_env_list("xterm-256color", "rgb", "", 0, buf, sizeof(buf));
    CHECK_INT(n, 20 + 20 + 1);
    CHECK_STR(buf, "TERM=xterm-256color");
    CHECK_STR(buf + 20, "COLORTERM=truecolor");
    CHECK_INT(buf[n - 1], 0);
    n = slash_env_list("", "256", "", 0, buf, sizeof(buf));
    CHECK_STR(buf, "COLORTERM="); /* unset */
    CHECK_INT(slash_env_list("", "", "", 0, buf, sizeof(buf)), 0);
    CHECK_INT(slash_env_list("vtcon", "rgb", "", 0, buf, 10), 0); /* does not fit */
    /* B4: an AMIGA window has no back colour erase; its TERM says so, and a /term beats it */
    n = slash_env_list("", "", "vtcon-amiga", 0, buf, sizeof(buf));
    CHECK_STR(buf, "TERM=vtcon-amiga");
    CHECK_INT(slash_env_list("", "", "", 0, buf, sizeof(buf)), 0); /* XTERM: vsh's own vtcon stays */
    slash_env_list("screen", "", "vtcon-amiga", 0, buf, sizeof(buf));
    CHECK_STR(buf, "TERM=screen");
    /* W46 (c): 16 colours is a TERM (the kit's xterm-16color), over a /term too */
    n = slash_env_list("xterm-256color", "16", "", 0, buf, sizeof(buf));
    CHECK_INT(n, 19 + 11 + 1);
    CHECK_STR(buf, "TERM=xterm-16color");
    CHECK_STR(buf + 19, "COLORTERM=");
    slash_env_list("", "16", "vtcon-amiga", 0, buf, sizeof(buf));
    CHECK_STR(buf, "TERM=vtcon-amiga-16color"); /* no bce in an AMIGA window */
    /* a UTF-8 window tells its programs to write UTF-8 (upkg-ui's chrome, nano, less); a
     * Latin-1 or CP437 window leaves them in the C locale */
    n = slash_env_list("", "", "", 1, buf, sizeof(buf));
    CHECK_INT(n, 17 + 1);
    CHECK_STR(buf, "LC_CTYPE=C.UTF-8");
    n = slash_env_list("xterm-256color", "rgb", "", 1, buf, sizeof(buf));
    CHECK_STR(buf + 20 + 20, "LC_CTYPE=C.UTF-8");
    CHECK_INT(slash_env_list("", "", "", 0, buf, sizeof(buf)), 0);
    CHECK_STR(slash_source("xterm", "vtcon", ""), "this window");
    CHECK_STR(slash_source("vtcon", "vtcon", ""), "profile");
    CHECK_STR(slash_source("", 0, ""), "default");
    rows[0].name = "term"; rows[0].value = "xterm"; rows[0].source = "this window";
    rows[1].name = "colors"; rows[1].value = ""; rows[1].source = "default";
    n = slash_setup_text(rows, 2, buf, sizeof(buf));
    CHECK_STR(buf, "term    xterm  [this window]\ncolors  -  [default]\n");
    CHECK_INT(n, (long)strlen(buf));
    CHECK_INT(slash_setup_text(rows, 2, buf, 10), 0); /* no room: no half line */
}

/* W46: the Settings menu's Terminal type and Colour mode items run the
 * typed commands: item -> line -> slash_parse -> the env vsh is told */
static void settings_menu_items_run_the_typed_commands(void)
{
    char line[40], buf[128];
    CHECK_INT(slash_menu_line(MENU_SET_TERM_XTERM, line, sizeof(line)), 1);
    CHECK_STR(line, "/term xterm-256color");
    CHECK_INT(parse(line), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_TERM);
    CHECK_STR(cmd.arg, "xterm-256color");
    CHECK_INT(slash_term_ok(cmd.arg), 1);
    CHECK_INT(slash_env_list(cmd.arg, "", "", 0, buf, sizeof(buf)) > 0, 1);
    CHECK_STR(buf, "TERM=xterm-256color");
    CHECK_INT(slash_menu_line(MENU_SET_TERM_NONE, line, sizeof(line)), 1);
    CHECK_INT(parse(line), SLASH_OK);
    CHECK_STR(cmd.arg, "none");
    CHECK_INT(slash_menu_line(MENU_SET_COLORS_RGB, line, sizeof(line)), 1);
    CHECK_STR(line, "/colors rgb");
    CHECK_INT(parse(line), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_COLORS);
    CHECK_INT(slash_colors_ok(cmd.arg), 1);
    slash_env_list("", cmd.arg, "", 0, buf, sizeof(buf));
    CHECK_STR(buf, "COLORTERM=truecolor");
    CHECK_INT(slash_menu_line(MENU_SET_COLORS_256, line, sizeof(line)), 1);
    CHECK_STR(line, "/colors 256");
    CHECK_INT(slash_menu_line(MENU_SET_COLORS_16, line, sizeof(line)), 1);
    CHECK_STR(line, "/colors 16");
    CHECK_INT(slash_menu_line(MENU_SET_COLORS_NONE, line, sizeof(line)), 1);
    CHECK_INT(slash_menu_line(MENU_SET_BOLD, line, sizeof(line)), 0); /* not one of them */
    /* Help > Slash commands is the typed /help: same line, same list */
    CHECK_INT(slash_menu_line(MENU_HELP_SLASH, line, sizeof(line)), 1);
    /* ... but only at a prompt: in vim or less the list and its newline
     * went into the program's screen and input */
    CHECK_INT(slash_menu_help_where(0, 0), SLASH_HELP_PRINT);
    CHECK_INT(slash_menu_help_where(1, 0), SLASH_HELP_REQUESTER);
    CHECK_INT(slash_menu_help_where(0, 1), SLASH_HELP_REQUESTER);
    CHECK_INT(slash_menu_help_where(1, 1), SLASH_HELP_REQUESTER);
    CHECK_STR(line, "/help");
    CHECK_INT(parse(line), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_HELP);
    {
        static char help[4096];
        CHECK_INT(slash_help(cmd.arg, help, sizeof(help)) > 0, 1);
        CHECK_INT(strstr(help, "about") != 0, 1);
    }
    CHECK_INT(slash_menu_line(MENU_SET_TERM_VTCON, line, 8), 0);      /* no room */
    /* the checkmark follows the window's value; empty is the "none" item */
    CHECK_INT(slash_menu_checked(MENU_SET_TERM_XTERM, "xterm-256color", ""), 1);
    CHECK_INT(slash_menu_checked(MENU_SET_TERM_VTCON, "xterm-256color", ""), 0);
    CHECK_INT(slash_menu_checked(MENU_SET_TERM_NONE, "", ""), 1);
    CHECK_INT(slash_menu_checked(MENU_SET_TERM_NONE, "vtcon", ""), 0);
    CHECK_INT(slash_menu_checked(MENU_SET_COLORS_256, "", "256"), 1);
    CHECK_INT(slash_menu_checked(MENU_SET_COLORS_NONE, "x", ""), 1);
    CHECK_INT(slash_menu_checked(MENU_SET_COLORS_16, "", "16"), 1);
    CHECK_INT(slash_menu_checked(MENU_SET_COLORS_256, "", "16"), 0);
    CHECK_INT(slash_menu_checked(MENU_SET_COLORS_RGB, "rgb", ""), 0); /* term does not leak into colors */
    CHECK_INT(slash_menu_checked(MENU_SET_BOLD, "", ""), -1);
}

/* W47: /state answers from the counters: all keys, one key bare, an unknown key refused */
static void state_formats_the_counters(void)
{
    static char out[512];
    char name[32];
    slash_state s;
    memset(&s, 0, sizeof(s));
    s.tabs = 2;
    slash_state_bump(&s, STATE_TAB_OPENED, 0, 0);
    slash_state_bump(&s, STATE_TAB_OPENED, 0, 0);
    slash_state_bump(&s, STATE_SWITCH, 0, 0);
    slash_state_bump(&s, STATE_MENU, 7, 0);
    slash_state_bump(&s, STATE_MENU, 49, 0);
    slash_state_bump(&s, STATE_SLASH, 0, "help");
    slash_state_bump(&s, STATE_COPY, 0, 0);
    slash_state_bump(&s, STATE_PASTE, 0, 0);
    slash_state_bump(&s, STATE_PASTE, 0, 0);
    s.active = 1;
    /* the command is in the table and takes no argument or a key */
    CHECK_INT(parse("/state"), SLASH_OK);
    CHECK_INT(cmd.id, SLASH_STATE);
    CHECK_INT(parse("/state copies"), SLASH_OK);
    CHECK_STR(cmd.arg, "copies");
    /* every key, "key value" per line */
    CHECK_INT(slash_state_text(&s, "", out, sizeof(out)) > 0, 1);
    CHECK_STR(out, "tabs-opened 2\ntab-switches 1\nmenu-picks 2\nlast-menu 49\nslash-count 1\n"
                   "last-slash help\ncopies 1\npastes 2\ntabs 2\nactive 1\n");
    /* one key: the bare value */
    CHECK_INT(slash_state_text(&s, "pastes", out, sizeof(out)), 2);
    CHECK_STR(out, "2\n");
    CHECK_INT(slash_state_text(&s, "last-slash", out, sizeof(out)) > 0, 1);
    CHECK_STR(out, "help\n");
    memset(&s, 0, sizeof(s));
    slash_state_text(&s, "last-slash", out, sizeof(out));
    CHECK_STR(out, "-\n"); /* nothing typed yet */
    /* an unknown key: -1 and a message that names the keys */
    CHECK_INT(slash_state_text(&s, "nope", out, sizeof(out)), -1);
    CHECK(strstr(out, "no key nope") != 0);
    CHECK(strstr(out, "tab-switches") != 0);
    /* no room: no half line */
    CHECK(slash_state_text(&s, "", out, 30) < 30);
    /* the typed line's name, for last-slash */
    CHECK_INT(slash_line_name("/help cursor", 12, name, sizeof(name)), 4);
    CHECK_STR(name, "help");
    CHECK_INT(slash_line_name("/cursor-blink", 13, name, sizeof(name)), 12);
    CHECK_INT(slash_line_name("/", 1, name, sizeof(name)), 0);
}

void suite_slash(void)
{
    term_colors_and_setup_are_wired_and_pure();
    paths_stay_the_shells();
    settings_menu_items_run_the_typed_commands();
    settings_name_their_menu_items();
    arguments_are_handed_on_trimmed();
    a_wrong_value_says_what_fits();
    the_help_lists_every_command();
    tab_completes_names_then_values();
    state_formats_the_counters();
}
