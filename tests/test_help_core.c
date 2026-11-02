/* AMARCHY-WELCOME (amarchy/help_core.c): placeholder expansion from the live keymap, sections, the welcome-shown key
 * and the checkbox key handling. */
#include <stdio.h>
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

static long help_slurp(const char *path, char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    n = (long)fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return n;
}

/* every placeholder in the shipped dist/amarchy.help names a real binding or menu row in the shipped keymap */
static void test_shipped_text_placeholders(void)
{
    static char km_file[32768], txt[16384];
    static const char *names[2] = { "welcome", "help" };
    keymap k;
    long tn = help_slurp("dist/amarchy.help", txt, sizeof txt), kn = help_slurp("dist/keymap", km_file, sizeof km_file);
    int s, seen = 0;
    CHECK(tn > 0);
    CHECK(kn > 0);
    memset(&k, 0, sizeof k);
    CHECK(keymap_parse(&k, km_file, kn) != 0);
    for (s = 0; s < 2; s++) {
        long blen = 0, i = 0;
        const char *body = hc_section(txt, tn, names[s], &blen);
        CHECK(body != 0);
        if (!body)
            continue;
        while (i < blen) {
            const char *open = memchr(body + i, '{', (size_t)(blen - i)), *close;
            char ph[128], out[200];
            int pl;
            if (!open)
                break;
            close = memchr(open, '}', (size_t)(body + blen - open));
            CHECK(close != 0);
            if (!close)
                break;
            pl = (int)(close - open) + 1;
            memcpy(ph, open, (size_t)pl);
            ph[pl] = 0;
            i = (open - body) + pl;
            hc_expand(&k, ph, out, sizeof out);
            seen++;
            /* W11 (gui:help key) and W11b (Quit Amarchy key) are phase 3: their keys do not exist yet. When those
             * land, delete this exception so every placeholder must resolve. */
            if (!strcmp(ph, "{key:gui:help}") || !strcmp(ph, "{key:amarchy:quit}")) { /* W11, W11b */
                CHECK_STR(out, HC_UNBOUND);
                continue;
            }
            if (!strcmp(out, HC_UNBOUND))
                printf("[FAIL] placeholder %s is unbound in dist/keymap\n", ph);
            CHECK(strcmp(out, HC_UNBOUND) != 0);
        }
    }
    CHECK(seen >= 15);
    keymap_free(&k);
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
    test_shipped_text_placeholders();
}
