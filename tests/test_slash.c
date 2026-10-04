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

void suite_slash(void)
{
    paths_stay_the_shells();
    settings_name_their_menu_items();
    arguments_are_handed_on_trimmed();
    a_wrong_value_says_what_fits();
    the_help_lists_every_command();
    tab_completes_names_then_values();
}
