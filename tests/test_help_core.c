/* AMARCHY-WELCOME (amarchy/help_core.c): placeholder expansion from the live keymap, sections, the welcome-shown key
 * and the checkbox key handling. */
#include <string.h>
#include "../amarchy/help_core.h"
#include "harness.h"

static const char km_text[] =
    "[keymap]\nversion = 1\nsuper = lamiga\n[bind]\n"
    "super+space = menu:root ; Menu | menu\n"
    "super+shift+left = win:snap left ; Snap left | window\n"
    "super+k = viewer ; Key bindings | menu\n"
    "[menu]\n"
    "style = Style Menu | menu:style\n"
    "style.theme = Theme | gtheme:x\n";

static void load(keymap *k, const char *txt)
{
    memset(k, 0, sizeof *k);
    CHECK(keymap_parse(k, txt, (long)strlen(txt)) != 0);
}

static void test_expand(void)
{
    keymap k;
    char out[200];
    load(&k, km_text);
    hc_expand(&k, "Press {key:menu:root} to open it.", out, sizeof out);
    CHECK_STR(out, "Press Super+Space to open it.");
    hc_expand(&k, "{key:win:snap left}|{key:viewer}", out, sizeof out);
    CHECK_STR(out, "Super+Shift+Left|Super+K");
    hc_expand(&k, "See {menu:style.theme}.", out, sizeof out);
    CHECK_STR(out, "See Style Menu > Theme.");
    hc_expand(&k, "{menu:nope.deeper}", out, sizeof out);
    CHECK_STR(out, "Nope > Deeper");          /* a segment the tree lacks reads capitalised */
    hc_expand(&k, "no placeholders { here } {key:", out, sizeof out);
    CHECK_STR(out, "no placeholders { here } {key:");
    keymap_free(&k);
}

static void test_unbound(void)
{
    keymap k;
    char out[100];
    load(&k, km_text);
    hc_expand(&k, "Quit: {key:menuid:MENU_QUIT}.", out, sizeof out);
    CHECK_STR(out, "Quit: (unbound).");
    hc_expand(&k, "{key:}{menu:}", out, sizeof out);
    CHECK_STR(out, "(unbound)(unbound)");
    keymap_free(&k);
}

static void test_rebound(void)
{
    keymap k;
    char out[100];
    static const char re[] = "[keymap]\nversion = 1\nsuper = lamiga\n[bind]\nsuper+ctrl+g = menu:root ; Menu | menu\n";
    load(&k, re);
    hc_expand(&k, "{key:menu:root}", out, sizeof out);
    CHECK_STR(out, "Super+Ctrl+G");
    keymap_free(&k);
}

static void test_truncation(void)
{
    keymap k;
    char out[10];
    load(&k, km_text);
    CHECK_INT(hc_expand(&k, "ab {key:menu:root} cd", out, 10), 9);
    CHECK_STR(out, "ab Super+");               /* cut at cap - 1, NUL-terminated */
    CHECK_INT(hc_expand(&k, "abc", out, 1), 0);
    CHECK_STR(out, "");
    CHECK_INT(hc_expand(&k, "abc", out, 0), 0);
    CHECK_INT(hc_expand(&k, "abc", out, 4), 3);
    CHECK_STR(out, "abc");
    keymap_free(&k);
}

static void test_sections(void)
{
    static const char t[] = "; head\n[welcome]\nLine one\nLine two\n[help]\nA\n\nB";
    long n = 0;
    const char *b = hc_section(t, (long)strlen(t), "welcome", &n);
    hc_line l[8];
    CHECK(b != 0);
    CHECK_INT((int)n, 18);
    CHECK_INT(hc_lines(b, n, l, 8), 2);
    CHECK_INT(l[0].len, 8);
    CHECK(!strncmp(l[1].s, "Line two", l[1].len));
    b = hc_section(t, (long)strlen(t), "help", &n);
    CHECK(b != 0);
    CHECK_INT(hc_lines(b, n, l, 8), 3);        /* A, empty, B without newline */
    CHECK_INT(l[1].len, 0);
    CHECK_INT(l[2].len, 1);
    CHECK_INT(hc_lines(b, n, l, 2), 2);        /* capped at max */
    CHECK(hc_section(t, (long)strlen(t), "missing", &n) == 0);
    CHECK(hc_section(t, (long)strlen(t), "wel", &n) == 0);   /* a prefix of a name is not the name */
}

static void test_key_default_and_roundtrip(void)
{
    am_prefs p, q;
    static char txt[UC_MAX_FILE + 1];
    amconf_defaults(&p);
    CHECK_STR(am_keys[AM_WELCOME].name, "welcome-shown");
    CHECK_STR(am_keys[AM_WELCOME].def, "off");
    CHECK_INT(hc_should_welcome(&p), 1);
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);   /* a key at its default is not written */
    CHECK(strstr(txt, "welcome-shown") == 0);
    hc_mark_shown(&p);
    CHECK_INT(hc_should_welcome(&p), 0);
    CHECK_STR(p.val[AM_WELCOME], "on");
    CHECK(amconf_serialize(&p, txt, sizeof txt) > 0);
    CHECK(strstr(txt, "welcome-shown = on") != 0);
    amconf_parse(txt, (long)strlen(txt), &q);
    CHECK_STR(q.val[AM_WELCOME], "on");
    CHECK_INT(hc_should_welcome(&q), 0);
}

static void test_welcome_key(void)
{
    int ticked = 0, done = 0;
    CHECK_INT(hc_welcome_key(' ', &ticked, &done), 1);
    CHECK_INT(ticked, 1);
    CHECK_INT(done, 0);
    CHECK_INT(hc_welcome_key(' ', &ticked, &done), 1);
    CHECK_INT(ticked, 0);
    CHECK_INT(hc_welcome_key('x', &ticked, &done), 0);
    CHECK_INT(done, 0);
    CHECK_INT(hc_welcome_key('\r', &ticked, &done), 1);
    CHECK_INT(done, 1);
    done = 0;
    CHECK_INT(hc_welcome_key(27, &ticked, &done), 1);
    CHECK_INT(done, 1);
    CHECK_INT(ticked, 0);                       /* closing never changes the tick */
}

void suite_help_core(void)
{
    test_expand();
    test_unbound();
    test_rebound();
    test_truncation();
    test_sections();
    test_key_default_and_roundtrip();
    test_welcome_key();
}
