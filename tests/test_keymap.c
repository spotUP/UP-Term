/* The keymap parser (config/keymap.c): syntax, expansion, refusals,
 * overrides, caps, and the shipped dist/keymap. No OS calls: a host suite. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../config/upconf.h"
#include "../config/keymap.h"
#include "../handler/keymap_act.h"
#include "../handler/menu_ids.h"
#include "../config/cxdesc.h"
#include "../handler/slash.h"
#include "../prefs/settings_def.h"
#include "../render/emoji.h"
#include "../engine/vtengine.h"
#include "harness.h"
#include "../render/gthemes_core.h"
#include "../config/amconf_core.h"

#define RAW_K     0x27
#define RAW_T     0x14
#define RAW_SPACE 0x40
#define RAW_1     0x01
#define RAW_9     0x09

static keymap km;

static int parse(const char *text)
{
    return keymap_parse(&km, text, (long)strlen(text));
}

static void comments_crlf_and_fields(void)
{
    const km_bind *b;
    CHECK(parse("; a comment\r\n[keymap]\r\nversion = 1\r\nsuper = lamiga\r\n[bind]\r\n"
                "super+k = viewer ; Key bindings | menu ; trailing\r\n"
                "SUPER+Shift+Space = slash:/font bigger ; Bigger font | style\r\n"));
    CHECK_INT(km.version, 1);
    CHECK_INT(km.nbind, 2);
    CHECK_INT(km.skipped, 0);
    b = keymap_find(&km, KM_Q_SUPER, RAW_K);
    CHECK(b != 0);
    if (b) {
        CHECK_INT(b->kind, KA_VIEWER);
        CHECK_STR(b->label, "Key bindings");
        CHECK_STR(b->cat, "menu");
    }
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT, RAW_SPACE);
    CHECK(b != 0);
    if (b) {
        CHECK_INT(b->kind, KA_SLASH);
        CHECK_STR(b->arg, "/font bigger");
        CHECK_STR(b->label, "Bigger font");
        CHECK_STR(b->cat, "style");
    }
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_T) == 0);        /* the qualifiers are part of the key */
    CHECK(keymap_find(&km, KM_Q_SHIFT, RAW_K) == 0);
}

static void digits_expand_with_percent_n(void)
{
    const km_bind *b;
    CHECK(parse("[bind]\nsuper+1..9 = tab:goto %n ; Switch to tab %n | workspace\n"));
    CHECK_INT(km.nbind, 9);
    b = keymap_find(&km, KM_Q_SUPER, RAW_1);
    CHECK(b != 0);
    if (b) {
        CHECK_INT(b->kind, KA_TAB);
        CHECK_STR(b->arg, "goto 1");
        CHECK_STR(b->label, "Switch to tab 1");
    }
    b = keymap_find(&km, KM_Q_SUPER, RAW_9);
    CHECK(b != 0 && !strcmp(b->arg, "goto 9") && !strcmp(b->label, "Switch to tab 9"));
    /* a range runs forward only */
    parse("[bind]\nsuper+9..1 = tab:goto %n\n");
    CHECK_INT(km.nbind, 0);
    CHECK_INT(km.skipped, 1);
}

static void junk_file_keeps_the_old_keymap(void)
{
    CHECK(!parse("this is not a keymap\nrandom junk\n"));      /* no section at all */
    CHECK(!parse("[bind]\nsuper+a = bogus:thing\nnonsense\n")); /* every row bad */
    CHECK(parse("[bind]\n"));                                  /* an empty [bind] stays legal */
    CHECK(parse("# only a comment\n\n"));
    CHECK(parse("[bind]\nsuper+a = bogus:thing\nsuper+d = viewer\n")); /* mixed: junk skipped */
    CHECK_INT(km.nbind, 1);
}

static void bad_rows_are_skipped_and_counted(void)
{
    CHECK(parse("[bind]\n"
                "super+a = bogus:thing\n"             /* unknown action prefix */
                "super+b = tab:fly\n"                 /* unknown tab verb */
                "super+nosuchkey = viewer\n"          /* unknown key name */
                "shift+c = viewer\n"                  /* no super */
                "super+d = viewer\n"));
    CHECK_INT(km.nbind, 1);
    CHECK_INT(km.skipped, 4);
    CHECK_INT(km.refused, 0);
    CHECK_INT(km.first_bad, 2);
    /* unknown sections and unknown [keymap] keys are ignored, not skipped rows */
    CHECK(parse("[keymap]\nfuture = 7\n[colors]\nx = y\n[bind]\nsuper+d = viewer\n"));
    CHECK_INT(km.nbind, 1);
    CHECK_INT(km.skipped, 0);
    /* a super other than lamiga is rejected */
    parse("[keymap]\nsuper = ramiga\n[bind]\nsuper+d = viewer\n");
    CHECK_INT(km.skipped, 1);
}

static void the_safety_refusals(void)
{
    CHECK(parse("[bind]\n"
                "super+m = viewer\n"                  /* Intuition's */
                "super+n = viewer\n"
                "super+shift+m = viewer\n"
                "super+mouse = viewer\n"              /* Workbench's */
                "super+lmb = viewer\n"
                "super+k = viewer\n"));
    CHECK_INT(km.nbind, 1);
    CHECK_INT(km.skipped, 5);
    CHECK_INT(km.refused, 5);
    CHECK(keymap_find(&km, KM_Q_SUPER, 0x37) == 0);    /* m */
    /* Left Amiga + Ctrl is a plain chord (Left Amiga + Alt is Intuition's mouse button); the reset
     * needs Right Amiga too, which no row can name and the matchers refuse */
    CHECK(parse("[bind]\nsuper+ctrl+t = viewer\nsuper+shift+ctrl+k = viewer\n"));
    CHECK_INT(km.nbind, 2);
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_CTRL, RAW_T) != 0);
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT | KM_Q_CTRL, RAW_K) != 0);
}

/* A super+alt row is refused with a one-line error naming the line; other rows still load. */
static void a_super_alt_row_is_refused_and_super_ctrl_loads(void)
{
    CHECK(parse("[bind]\nsuper+ctrl+t = viewer\nsuper+alt+k = viewer\nsuper+shift+alt+j = viewer\nsuper+ctrl+shift+k = viewer\n"));
    CHECK_INT(km.nbind, 2);
    CHECK_INT(km.skipped, 2);
    CHECK_INT(km.refused, 2);
    CHECK_INT(km.first_bad, 3);
    CHECK(strcmp(km.err, "line 3: super+alt is the mouse-click emulation") == 0);
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_CTRL, RAW_T) != 0);
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, RAW_K) == 0);
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT | KM_Q_CTRL, RAW_K) != 0);
}

/* Regression: Left Amiga + Alt is Intuition's left mouse button, so a chord with it clicks whatever
 * is under the pointer (it left a close gadget selected and blocked AmTile's ChangeWindowBox).
 * Parse the shipped file; no binding may combine Super with either Alt. */
static long slurp(const char *path, char *buf, long cap);

static void the_shipped_keymap_has_no_super_alt_chord(void)
{
    static char file[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    int i, ctrl = 0;
    CHECK(n > 0);
    CHECK(keymap_parse(&km, file, n));
    for (i = 0; i < km.nbind; i++) {
        CHECK(!(km.bind[i].qual & KM_Q_ALT));
        if (km.bind[i].qual & KM_Q_CTRL)
            ctrl++;
    }
    CHECK_INT(ctrl, 44);                /* the 28 former Super+Alt chords, now Super+Ctrl, super+ctrl+help (window shot), and the 14 super+ctrl+shift snap, float and swap keys (P3-D1), and super+ctrl+shift+q (Quit Patina) */
}

static void a_newer_major_is_rejected_whole(void)
{
    CHECK(!parse("[keymap]\nversion = 2\n[bind]\nsuper+k = viewer\n"));
    CHECK_INT(km.newer, 1);
    CHECK_INT(km.nbind, 0);
    CHECK(parse("[keymap]\nversion = 1\n[bind]\nsuper+k = viewer\n"));
    CHECK_INT(km.newer, 0);
    CHECK(parse("[bind]\nsuper+k = viewer\n"));        /* no version: taken as ours */
    CHECK_INT(km.version, 0);
    CHECK_INT(km.nbind, 1);
    CHECK(!keymap_parse(&km, 0, 0));                    /* no file */
    CHECK(parse(""));                                   /* an empty one: usable, no rows */
    CHECK_INT(km.nbind, 0);
}

static void a_later_row_replaces_and_none_unbinds(void)
{
    const km_bind *b;
    CHECK(parse("[bind]\nsuper+k = viewer\nsuper+t = tab:new\nsuper+k = tab:close ; Closer | window\n"));
    CHECK_INT(km.nbind, 2);                              /* replaced, not appended */
    b = keymap_find(&km, KM_Q_SUPER, RAW_K);
    CHECK(b != 0 && b->kind == KA_TAB && !strcmp(b->arg, "close"));
    CHECK(km.bind[0].code == RAW_K);                     /* in place */
    parse("[bind]\nsuper+k = viewer\nsuper+t = tab:new\nsuper+k = none\n");
    CHECK_INT(km.nbind, 1);
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_K) == 0);
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_T) != 0);
    CHECK_INT(km.skipped, 0);
}

static void the_user_file_overrides_the_shipped_one(void)
{
    static const char shipped[] =
        "[keymap]\nversion = 1\n[bind]\nsuper+k = viewer ; Keys | menu\nsuper+t = tab:new\n"
        "[menu]\napps.upkg = Install | run:upkg-browse\napps.tmux = Tmux | run:tmux\n";
    static const char user[] =
        "[bind]\nsuper+t = none\nsuper+k = run:mykeys ; Mine | apps\nsuper+shift+q = run:Reboot\nsuper+m = viewer\n"
        "[menu]\napps.upkg = Add-ons | run:upkg\nwindow.tab = New tab | tab:new\n";
    const km_bind *b;
    CHECK(keymap_parse(&km, shipped, (long)strlen(shipped)));
    CHECK_INT(km.nbind, 2);
    CHECK_INT(km.nmenu, 2);
    CHECK(keymap_overlay(&km, user, (long)strlen(user)));
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_T) == 0);     /* unbound */
    b = keymap_find(&km, KM_Q_SUPER, RAW_K);
    CHECK(b != 0 && b->kind == KA_RUN && !strcmp(b->arg, "mykeys") && !strcmp(b->cat, "apps"));
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT, 0x10) != 0);   /* a new row, q */
    CHECK_INT(km.skipped, 1);                            /* super+m, refused in the user file too */
    CHECK_INT(km.refused, 1);
    CHECK_INT(km.nmenu, 3);                              /* replaced by id, one added */
    CHECK_STR(km.menu[0].id, "apps.upkg");
    CHECK_STR(km.menu[0].label, "Add-ons");
    CHECK_STR(km.menu[0].arg, "upkg");
    CHECK_INT(km.menu[0].kind, KA_RUN);
    /* a newer-version user file changes nothing */
    CHECK(!keymap_overlay(&km, "[keymap]\nversion = 9\n[bind]\nsuper+k = none\n", 41));
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_K) != 0);
}

static void the_tables_have_caps(void)
{
    static char big[KM_MAX_BIND * 40 + 4000];
    char *o = big;
    int i;
    /* distinct specs: 5 qualifier sets x 40 keys is more than KM_MAX_BIND */
    strcpy(o, "[bind]\n");
    o += strlen(o);
    for (i = 0; i < KM_MAX_BIND + 8; i++) {
        static const char *const q[] = { "super", "super+shift", "super+ctrl", "super+rshift",
                                         "super+shift+ctrl", "super+ctrl+rshift", "super+shift+rshift",
                                         "super+shift+ctrl+rshift" };
        static const char *const key[] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l",
                                           "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
                                           "1", "2", "3", "4", "5", "6", "7", "8", "9" };
        strcpy(o, q[i / 33]);
        strcat(o, "+");
        strcat(o, key[i % 33]);
        strcat(o, " = viewer\n");
        o += strlen(o);
    }
    CHECK(keymap_parse(&km, big, (long)strlen(big)));
    CHECK_INT(km.nbind, KM_MAX_BIND);
    CHECK_INT(km.skipped, 8);
    CHECK(km.overflow);
    /* a value longer than the field is not silently cut into another action */
    parse("[bind]\nsuper+a = run:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
    CHECK_INT(km.nbind, 0);
    CHECK_INT(km.skipped, 1);
    CHECK(km.overflow);
}

static int fake_swedish(char ch)
{
    return ch == '+' ? 0x0B : ch == '-' ? 0x3A : -1;
}

static int fake_none(char ch)
{
    (void)ch;
    return -1;
}

/* symbol names follow the key that prints them; letters and digits stay put */
static void symbol_names_follow_the_printed_key(void)
{
    keymap_set_layout(fake_swedish);
    CHECK_INT(keymap_key_code("plus"), 0x0B);
    CHECK_INT(keymap_key_code("minus"), 0x3A);
    CHECK_INT(keymap_key_code("comma"), 0x38);      /* no answer: US position */
    CHECK_INT(keymap_key_code("1"), 0x01);
    CHECK_INT(keymap_key_code("k"), 0x27);
    CHECK_STR(keymap_key_name(0x3A), "minus");
    CHECK_STR(keymap_key_name(0x0B), "plus");
    parse("[bind]\nsuper+minus = menu:root\n");
    CHECK_INT(km.nbind, 1);
    CHECK_INT(km.bind[0].code, 0x3A);
    keymap_set_layout(fake_none);
    CHECK_INT(keymap_key_code("minus"), 0x0B);
    CHECK_INT(keymap_key_code("plus"), 0x0C);
    keymap_set_layout(0);
    CHECK_INT(keymap_key_code("minus"), 0x0B);
    CHECK_INT(keymap_key_code("plus"), 0x0C);
}

static void key_names_are_raw_codes(void)
{
    CHECK_INT(keymap_key_code("k"), 0x27);
    CHECK_INT(keymap_key_code("q"), 0x10);
    CHECK_INT(keymap_key_code("z"), 0x31);
    CHECK_INT(keymap_key_code("0"), 0x0A);
    CHECK_INT(keymap_key_code("space"), 0x40);
    CHECK_INT(keymap_key_code("return"), 0x44);
    CHECK_INT(keymap_key_code("f1"), 0x50);
    CHECK_INT(keymap_key_code("f10"), 0x59);
    CHECK_INT(keymap_key_code("up"), 0x4C);
    CHECK_INT(keymap_key_code("left"), 0x4F);
    CHECK_INT(keymap_key_code("K"), 0x27);               /* names are case-insensitive */
    CHECK_INT(keymap_key_code("nosuch"), -1);
    CHECK_STR(keymap_key_name(0x27), "k");
    CHECK_STR(keymap_key_name(0x40), "space");
    CHECK(keymap_key_name(0x7F) == 0);
    CHECK_INT(keymap_action_scope(KA_TAB), KS_TERMINAL);
    CHECK_INT(keymap_action_scope(KA_OPEN), KS_COMMODITY);
    CHECK_INT(keymap_action_scope(KA_RUN), KS_BOTH);
}

/* the file the release installs: every row must load, and every
 * menuid: must name a MENU_ id that handler/menu_ids.h has */
static long slurp(const char *path, char *buf, long cap)
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

/* B2: a binding to the action the handler runs (handler/keymap_act.c) */
static kact resolve1(const char *text, int row)
{
    kact a = {0, 0, 0};
    CHECK(parse(text));
    CHECK(row < km.nbind);
    kact_resolve(&km.bind[row], &a);
    return a;
}

static int win_row_ok(const char *action)
{
    char buf[200];
    snprintf(buf, sizeof buf, "[bind]\nsuper+d = win:%s\n", action);
    return parse(buf) && km.nbind == 1 && km.skipped == 0 && km.bind[0].kind == KA_WIN && !strcmp(km.bind[0].arg, action);
}

static int win_row_bad(const char *action)
{
    char buf[200];
    snprintf(buf, sizeof buf, "[bind]\nsuper+d = win:%s\n", action);
    parse(buf);
    return km.nbind == 0 && km.skipped == 1;
}

static void bar_actions_parse(void)
{
    static const char *const good[] = { "on", "off", "toggle" };
    static const char *const bad[] = { "", "status", "Toggle", "toggle x" };
    char row[64];
    size_t i;
    for (i = 0; i < sizeof good / sizeof good[0]; i++) {
        snprintf(row, sizeof row, "[bind]\nsuper+b = bar:%s\n", good[i]);
        CHECK(parse(row) && km.nbind == 1 && km.bind[0].kind == KA_BAR && !strcmp(km.bind[0].arg, good[i]));
    }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        snprintf(row, sizeof row, "[bind]\nsuper+b = bar:%s\n", bad[i]);
        parse(row);
        CHECK_INT(km.nbind, 0);
    }
    CHECK_INT(keymap_action_scope(KA_BAR), KS_COMMODITY);
    {   /* the shipped keymap binds super+b to bar:toggle */
        static char file[32768];
        long n = slurp("dist/keymap", file, sizeof(file));
        int found = 0;
        CHECK(n > 0 && keymap_parse(&km, file, n));
        for (i = 0; i < (size_t)km.nbind; i++)
            if (km.bind[i].kind == KA_BAR && !strcmp(km.bind[i].arg, "toggle"))
                found++;
        CHECK_INT(found, 1);
    }
}

static void win_actions_parse(void)
{
    static const char *const good[] = {
        "cycle next", "cycle prev", "front", "back", "zip", "fullwidth", "fullscreen", "save", "restore",
        "move left", "move right", "move up", "move down", "move left 1", "move down 9999", "move up 0032",
        "size left", "size right", "size up", "size down", "size right 64", "size down 1234",
        "focus left", "focus right", "focus up", "focus down", "focus next", "focus prev",
        "closedrawers", "screen next", "screen prev", "screen workbench", "screen back",
        "title toggle", "title show", "title hide",
        "snap left", "snap right", "snap up", "snap down", "snap tl", "snap tr", "snap bl", "snap br", "snap center", 0 };
    static const char *const bad[] = {
        "bogus", "cycle", "cycle left", "cycle next now", "front now", "zip 1", "save x", "restore 3", "closedrawers all",
        "move", "move sideways", "move left 0", "move left 00000", "move left 12345", "move left -5", "move left 3x",
        "move left 32 64", "size", "size next", "size up 0", "size up 10000", "size up -1",
        "focus", "focus left 5", "focus sideways", "screen", "screen left", "screen next 1",
        "title", "title flip", "title hide now", "Front", " front",
        "snap", "snap middle", "snap left 5", "snap next", "snap Left", 0 };
    int i;
    for (i = 0; good[i]; i++) {
        if (!win_row_ok(good[i]))
            printf("  win good form refused: %s\n", good[i]);
        CHECK(win_row_ok(good[i]));
    }
    for (i = 0; bad[i]; i++) {
        if (!win_row_bad(bad[i]))
            printf("  win bad form accepted: %s\n", bad[i]);
        CHECK(win_row_bad(bad[i]));
    }
    CHECK(win_row_bad(""));                              /* empty */
}

static const km_menu *shipped_row(const char *id);

static int range9(const char *p, long len)
{
    long i;
    for (i = 0; i + 4 <= len; i++)
        if (!memcmp(p + i, "1..9", 4))
            return 1;
    return 0;
}

/* P3-D1/D3/E3: the shipped snap, focus, float and swap hotkeys. Each key finds its row, runs the action the owner decision
 * names (D1: super+ctrl+shift arrows, 1-4, c; D2: focus on super+arrows, float t, swap h/j/k/l), no chord is bound twice
 * (a later duplicate would replace the earlier row, so the bind lines of the file equal nbind), none uses Alt (Left Amiga
 * + Alt is the mouse click), and super+esc (menu:system) and the tab keys super+comma and super+period stay. */
static void the_shipped_snap_focus_float_and_swap_keys_are_bound_and_clash_free(void)
{
    static char file[32768];
    static const struct { unsigned qual; unsigned code; int kind; const char *arg; } want[] = {
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x4F, KA_WIN, "snap left" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x4E, KA_WIN, "snap right" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x4C, KA_WIN, "snap up" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x4D, KA_WIN, "snap down" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x01, KA_WIN, "snap tl" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x02, KA_WIN, "snap tr" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x03, KA_WIN, "snap bl" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x04, KA_WIN, "snap br" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x33, KA_WIN, "snap center" },
        { KM_Q_SUPER, 0x4F, KA_WIN, "focus left" },
        { KM_Q_SUPER, 0x4E, KA_WIN, "focus right" },
        { KM_Q_SUPER, 0x4C, KA_WIN, "focus up" },
        { KM_Q_SUPER, 0x4D, KA_WIN, "focus down" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x14, KA_RUN, "UP-Term:bin/AmTile FLOAT TOGGLE" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x25, KA_RUN, "UP-Term:bin/AmTile SWAP LEFT" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x26, KA_RUN, "UP-Term:bin/AmTile SWAP DOWN" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x27, KA_RUN, "UP-Term:bin/AmTile SWAP UP" },
        { KM_Q_SUPER | KM_Q_CTRL | KM_Q_SHIFT, 0x28, KA_RUN, "UP-Term:bin/AmTile SWAP RIGHT" },
    };
    static const char *const menu[] = { "window.snapleft", "window.snapright", "window.snaptop", "window.snapbottom",
        "window.snaptopleft", "window.snaptopright", "window.snapbottomleft", "window.snapbottomright", "window.snapcenter",
        "window.float", "window.swapleft", "window.swapright", "window.swapup", "window.swapdown", 0 };
    long n = slurp("dist/keymap", file, sizeof(file));
    unsigned i;
    int lines = 0, in_bind = 0;
    const char *p;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    CHECK_INT(km.skipped, 0);
    for (i = 0; i < sizeof want / sizeof want[0]; i++) {
        const km_bind *b = keymap_find(&km, want[i].qual, want[i].code);
        CHECK(b != 0);
        if (b) {
            CHECK_INT(b->kind, want[i].kind);
            CHECK_STR(b->arg, want[i].arg);
        }
    }
    for (i = 0; menu[i]; i++)
        CHECK(shipped_row(menu[i]) != 0);
    CHECK(keymap_find(&km, KM_Q_SUPER, 0x45) != 0 && keymap_find(&km, KM_Q_SUPER, 0x45)->kind == KA_MENU);   /* super+esc */
    CHECK(keymap_find(&km, KM_Q_SUPER, 0x38) != 0 && keymap_find(&km, KM_Q_SUPER, 0x38)->kind == KA_TAB);    /* super+comma */
    CHECK(keymap_find(&km, KM_Q_SUPER, 0x39) != 0 && keymap_find(&km, KM_Q_SUPER, 0x39)->kind == KA_TAB);    /* super+period */
    for (p = file; p < file + n;) {
        const char *e = memchr(p, '\n', (size_t)(file + n - p));
        long len = e ? e - p : file + n - p;
        if (len > 0 && p[0] == '[')
            in_bind = !strncmp(p, "[bind]", 6);
        else if (in_bind && len > 0 && p[0] != ';' && p[0] != '\r' && memchr(p, '=', (size_t)len))
            lines += range9(p, len) ? 9 : 1;        /* a "1..9" row binds nine keys */
        p += len + 1;
    }
    CHECK_INT(lines, km.nbind);                 /* one row per chord: nothing replaced an earlier row */
    for (i = 0; i < (unsigned)km.nbind; i++)
        CHECK(!(km.bind[i].qual & KM_Q_ALT));
}

static void win_scope_is_commodity(void)
{
    CHECK_INT(keymap_action_scope(KA_WIN), KS_COMMODITY);
}

static void win_defers_in_the_handler(void)
{
    kact a = resolve1("[bind]\nsuper+d = win:zip\n", 0);
    CHECK_INT(a.kind, KACT_DEFER);
}

static void actions_resolve_to_menu_ids(void)
{
    kact a;
    a = resolve1("[bind]\nsuper+t = tab:new ; T | w\n", 0);
    CHECK_INT(a.kind, KACT_MENU);
    CHECK_INT(a.id, MENU_TAB_NEW);
    a = resolve1("[bind]\nsuper+w = tab:close ; C | w\n", 0);
    CHECK_INT(a.id, MENU_TAB_CLOSE);
    a = resolve1("[bind]\nsuper+period = tab:next ; N | w\n", 0);
    CHECK_INT(a.id, MENU_TAB_NEXT);
    a = resolve1("[bind]\nsuper+comma = tab:prev ; P | w\n", 0);
    CHECK_INT(a.id, MENU_TAB_PREV);
    a = resolve1("[bind]\nsuper+f3 = tab:goto 3 ; G | w\n", 0);
    CHECK_INT(a.kind, KACT_MENU);
    CHECK_INT(a.id, MENU_TAB_GOTO0 + 3);
    a = resolve1("[bind]\nsuper+c = menuid:MENU_COPY ; C | e\n", 0);
    CHECK_INT(a.id, MENU_COPY);
    a = resolve1("[bind]\nsuper+c = menuid:MENU_NO_SUCH_ITEM ; C | e\n", 0);
    CHECK_INT(a.kind, KACT_NONE);
    a = resolve1("[bind]\nsuper+f3 = tab:goto 10 ; G | w\n", 0);
    CHECK_INT(a.kind, KACT_NONE);
    a = resolve1("[bind]\nsuper+f3 = tab:goto 0 ; G | w\n", 0);
    CHECK_INT(a.kind, KACT_NONE);
    a = resolve1("[bind]\nsuper+h = slash:/help ; H | m\n", 0);
    CHECK_INT(a.kind, KACT_MENU);
    CHECK_INT(a.id, MENU_HELP_SLASH);
    a = resolve1("[bind]\nsuper+shift+t = slash:/theme ; T | s\n", 0);
    CHECK_INT(a.kind, KACT_SLASH);
    CHECK_STR(a.line, "/theme");
    a = resolve1("[bind]\nsuper+e = run:nvim ; E | a\n", 0);
    CHECK_INT(a.kind, KACT_DEFER);
    a = resolve1("[bind]\nsuper+k = viewer ; K | m\n", 0);
    CHECK_INT(a.kind, KACT_DEFER);
    /* the ids did not move when the enum became a list */
    CHECK_INT(MENU_COPY, 1);
    CHECK_INT(MENU_PASTE, 2);
    CHECK_STR(kact_menu_name(MENU_FIND), "MENU_FIND");
    CHECK_INT(kact_menu_id("MENU_FIND"), MENU_FIND);
    CHECK_INT(kact_menu_id("MENU_NOPE"), 0);
}

static int comm_is(const char *key, long id, const char *want)
{
    char ck[2] = {0, 0};
    int got = kact_comm_key(&km, id, ck);
    (void)key;
    return want ? (got && ck[0] == want[0]) : !got;
}

static void the_shipped_file_resolves_and_derives_menu_keys(void)
{
    char buf[32768];
    long n = slurp("dist/keymap", buf, sizeof(buf));
    int i;
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nbind; i++) {
        kact a;
        CHECK(kact_resolve(&km.bind[i], &a) != KACT_NONE);
    }
    /* the keys the menu bar shows are the file's, not a second table */
    CHECK(comm_is("period", MENU_TAB_NEXT, "."));
    CHECK(comm_is("comma", MENU_TAB_PREV, ","));
    CHECK(comm_is("c", MENU_COPY, "C"));
    CHECK(comm_is("v", MENU_PASTE, "V"));
    CHECK(comm_is("a", MENU_SELECT_ALL, "A"));
    CHECK(comm_is("g", MENU_FIND_NEXT, "G"));
    CHECK(comm_is("plus", MENU_FONT_BIGGER, "+"));
    CHECK(comm_is("minus", MENU_FONT_SMALLER, "-"));
    CHECK(comm_is("p", MENU_PREFS, "P"));
    CHECK(comm_is("q", MENU_CLOSE, "Q"));
    /* a key with a Shift twin has none: Intuition ignores Shift in a command key */
    CHECK(comm_is("t", MENU_TAB_NEW, 0));
    CHECK(comm_is("f", MENU_FIND, 0));
    CHECK(comm_is("l", MENU_AGENT, 0));
    CHECK(comm_is("-", MENU_CLEAR_SCREEN, 0));
}

/* ---- Omarchy groups in the shipped [menu] (Learn, System, Install) ---- */
static const km_menu *shipped_row(const char *id)
{
    int i;
    for (i = 0; i < km.nmenu; i++)
        if (!strcmp(km.menu[i].id, id))
            return &km.menu[i];
    return 0;
}

static void check_row(const char *id, int kind, const char *arg)
{
    const km_menu *m = shipped_row(id);
    CHECK(m != 0);
    if (!m)
        return;
    CHECK_INT(m->kind, kind);
    CHECK(!strcmp(m->arg, arg));
}

static void the_window_group_rows_and_hotkeys(void)
{
    check_row("window.next", KA_WIN, "cycle next");
    check_row("window.previous", KA_WIN, "cycle prev");
    check_row("window.front", KA_WIN, "front");
    check_row("window.back", KA_WIN, "back");
    check_row("window.zip", KA_WIN, "zip");
    check_row("window.fullwidth", KA_WIN, "fullwidth");
    check_row("window.fullscreen", KA_WIN, "fullscreen");
    check_row("window.save", KA_WIN, "save");
    check_row("window.restore", KA_WIN, "restore");
    check_row("window.moveleft", KA_WIN, "move left");
    check_row("window.moveright", KA_WIN, "move right");
    check_row("window.moveup", KA_WIN, "move up");
    check_row("window.movedown", KA_WIN, "move down");
    check_row("window.narrower", KA_WIN, "size left");
    check_row("window.wider", KA_WIN, "size right");
    check_row("window.shorter", KA_WIN, "size up");
    check_row("window.taller", KA_WIN, "size down");
    check_row("window.focusleft", KA_WIN, "focus left");
    check_row("window.focusright", KA_WIN, "focus right");
    check_row("window.focusup", KA_WIN, "focus up");
    check_row("window.focusdown", KA_WIN, "focus down");
    check_row("window.closedrawer", KA_REXX, "address WORKBENCH 'WINDOW ACTIVE CLOSE'");
    check_row("window.closedrawers", KA_WIN, "closedrawers");
    check_row("window.tile", KA_RUN, "UP-Term:bin/AmTile TOGGLE");
    check_row("window.layoutnext", KA_RUN, "UP-Term:bin/AmTile LAYOUT NEXT");
    check_row("window.layoutdwindle", KA_RUN, "UP-Term:bin/AmTile LAYOUT dwindle");
    check_row("window.layoutmaster", KA_RUN, "UP-Term:bin/AmTile LAYOUT master-stack");
    check_row("window.layoutmirrored", KA_RUN, "UP-Term:bin/AmTile LAYOUT mirrored");
    check_row("window.layoutcolumns", KA_RUN, "UP-Term:bin/AmTile LAYOUT columns");
    check_row("window.layoutrows", KA_RUN, "UP-Term:bin/AmTile LAYOUT rows");
    check_row("window.layoutgrid", KA_RUN, "UP-Term:bin/AmTile LAYOUT grid");
    check_row("window.layoutmonocle", KA_RUN, "UP-Term:bin/AmTile LAYOUT monocle");
    {   /* the 15 AmTile layout hotkeys (Super+Ctrl family); the plain Super+<key> bindings keep their old kinds */
        static const struct { unsigned code; const char *arg; } at[] = {
            { 0x28, "UP-Term:bin/AmTile LAYOUT NEXT" },
            { 0x01, "UP-Term:bin/AmTile LAYOUT dwindle" },
            { 0x02, "UP-Term:bin/AmTile LAYOUT master-stack" },
            { 0x03, "UP-Term:bin/AmTile LAYOUT mirrored" },
            { 0x04, "UP-Term:bin/AmTile LAYOUT columns" },
            { 0x05, "UP-Term:bin/AmTile LAYOUT rows" },
            { 0x06, "UP-Term:bin/AmTile LAYOUT grid" },
            { 0x07, "UP-Term:bin/AmTile LAYOUT monocle" },
            { 0x0B, "UP-Term:bin/AmTile RATIO -5" },
            { 0x0C, "UP-Term:bin/AmTile RATIO +5" },
            { 0x26, "UP-Term:bin/AmTile NMASTER -1" },
            { 0x27, "UP-Term:bin/AmTile NMASTER +1" },
            { 0x44, "UP-Term:bin/AmTile PROMOTE" },
            { 0x38, "UP-Term:bin/AmTile CYCLE PREV" },
            { 0x39, "UP-Term:bin/AmTile CYCLE NEXT" },
        };
        unsigned i;
        for (i = 0; i < sizeof at / sizeof at[0]; i++) {
            const km_bind *b = keymap_find(&km, KM_Q_SUPER | KM_Q_CTRL, at[i].code);
            CHECK(b != 0);
            if (b) {
                CHECK_INT(b->kind, KA_RUN);
                CHECK(!strcmp(b->arg, at[i].arg));
            }
        }
        {   /* plain Super+minus, Super+plus, Super+L stay what they were */
            const km_bind *m = keymap_find(&km, KM_Q_SUPER, 0x0B);
            const km_bind *p = keymap_find(&km, KM_Q_SUPER, 0x0C);
            CHECK(m != 0 && m->kind == KA_MENUID);
            CHECK(p != 0 && p->kind == KA_MENUID);
        }
    }
    {   /* Super+Alt+T runs the same command; Super+T stays New tab (the plan's free-or-clash check) */
        const km_bind *b = keymap_find(&km, KM_Q_SUPER | KM_Q_CTRL, 0x14);
        const km_bind *t = keymap_find(&km, KM_Q_SUPER, 0x14);
        CHECK(b != 0);
        if (b) {
            CHECK_INT(b->kind, KA_RUN);
            CHECK(!strcmp(b->arg, "UP-Term:bin/AmTile TOGGLE"));
        }
        CHECK(t != 0);
        if (t)
            CHECK_INT(t->kind, KA_TAB);
    }
    {   /* the 16 Window hotkeys: each key is a KA_WIN bind with its verb (plan 3) */
        static const struct { unsigned qual; unsigned code; const char *arg; } hk[] = {
            { KM_Q_SUPER, 0x18, "front" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x18, "back" },
            { KM_Q_SUPER, 0x31, "zip" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x23, "fullwidth" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x21, "save" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x13, "restore" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x42, "cycle next" },
            { KM_Q_SUPER|KM_Q_CTRL|KM_Q_SHIFT, 0x42, "cycle prev" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4F, "move left" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4E, "move right" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4C, "move up" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4D, "move down" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x4F, "size left" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x4E, "size right" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x4C, "size up" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x4D, "size down" },
        };
        unsigned i;
        for (i = 0; i < sizeof hk / sizeof hk[0]; i++) {
            const km_bind *b = keymap_find(&km, hk[i].qual, hk[i].code);
            CHECK(b != 0);
            if (b) {
                CHECK_INT(b->kind, KA_WIN);
                CHECK(!strcmp(b->arg, hk[i].arg));
            }
        }
    }
}

static void the_screens_group_rows_and_hotkeys(void)
{
    check_row("screens.next", KA_WIN, "screen next");
    check_row("screens.previous", KA_WIN, "screen prev");
    check_row("screens.workbench", KA_WIN, "screen workbench");
    check_row("screens.back", KA_WIN, "screen back");
    check_row("screens.titlebar", KA_WIN, "title toggle");
    check_row("screens.own", KA_SLASH, "/screen own");
    {   /* the 3 Screens hotkeys (plan 3): super+tab, super+shift+tab, super+alt+b */
        static const struct { unsigned qual; unsigned code; const char *arg; } hk[] = {
            { KM_Q_SUPER, 0x42, "screen next" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x42, "screen prev" },
            { KM_Q_SUPER|KM_Q_CTRL, 0x35, "title toggle" },
        };
        unsigned i;
        for (i = 0; i < sizeof hk / sizeof hk[0]; i++) {
            const km_bind *b = keymap_find(&km, hk[i].qual, hk[i].code);
            CHECK(b != 0);
            if (b) {
                CHECK_INT(b->kind, KA_WIN);
                CHECK(!strcmp(b->arg, hk[i].arg));
            }
        }
    }
}

/* B1: the 42 Workbench rows (plan 2.2), each the exact WORKBENCH port command; Delete and Empty Trash are menu only. */
static void the_workbench_group_rows(void)
{
    static const struct { const char *id; const char *item; int root; } wb[] = {
        { "workbench.backdrop", "WORKBENCH.BACKDROP", 1 },
        { "workbench.execute", "WORKBENCH.EXECUTE", 1 },
        { "workbench.redrawall", "WORKBENCH.REDRAWALL", 1 },
        { "workbench.updateall", "WORKBENCH.UPDATEALL", 1 },
        { "workbench.lastmessage", "WORKBENCH.LASTMESSAGE", 1 },
        { "workbench.about", "WORKBENCH.ABOUT", 1 },
        { "workbench.reset", "TOOLS.RESETWB", 1 },
        { "workbench.autoarrange", "UP-Term:Rexx/wb-autoarrange.rexx", 2 },   /* a WBStartup item, not a menu name: a script */
        { "workbench.openparent", "WINDOW.OPENPARENT", 0 },
        { "workbench.newdrawer", "WINDOW.NEWDRAWER", 0 },
        { "workbench.closewindow", "WINDOW.CLOSE", 0 },
        { "workbench.updatewindow", "WINDOW.UPDATE", 0 },
        { "workbench.selectcontents", "WINDOW.SELECTCONTENTS", 0 },
        { "workbench.clearselection", "WINDOW.CLEARSELECTION", 0 },
        { "workbench.resizetofit", "WINDOW.RESIZETOFIT", 0 },
        { "workbench.cleanupcolumn", "WINDOW.CLEANUPBY.COLUMN", 0 },
        { "workbench.cleanupname", "WINDOW.CLEANUPBY.NAME", 0 },
        { "workbench.cleanupdate", "WINDOW.CLEANUPBY.DATE", 0 },
        { "workbench.cleanupsize", "WINDOW.CLEANUPBY.SIZE", 0 },
        { "workbench.cleanuptype", "WINDOW.CLEANUPBY.TYPE", 0 },
        { "workbench.snapshotwindow", "WINDOW.SNAPSHOT.WINDOW", 0 },
        { "workbench.snapshotall", "WINDOW.SNAPSHOT.ALL", 0 },
        { "workbench.showicons", "WINDOW.SHOW.ONLYICONS", 0 },
        { "workbench.showall", "WINDOW.SHOW.ALLFILES", 0 },
        { "workbench.viewicon", "WINDOW.VIEWBY.ICON", 0 },
        { "workbench.viewname", "WINDOW.VIEWBY.NAME", 0 },
        { "workbench.viewdate", "WINDOW.VIEWBY.DATE", 0 },
        { "workbench.viewsize", "WINDOW.VIEWBY.SIZE", 0 },
        { "workbench.viewtype", "WINDOW.VIEWBY.TYPE", 0 },
        { "workbench.sortforward", "WINDOW.SORTORDER.FORWARD", 0 },
        { "workbench.sortreverse", "WINDOW.SORTORDER.REVERSE", 0 },
        { "workbench.find", "WINDOW.FIND", 0 },
        { "workbench.iconopen", "ICONS.OPEN", 0 },
        { "workbench.iconcopy", "ICONS.COPY", 0 },
        { "workbench.iconrename", "ICONS.RENAME", 0 },
        { "workbench.iconinfo", "ICONS.INFORMATION", 0 },
        { "workbench.iconsnapshot", "ICONS.SNAPSHOT", 0 },
        { "workbench.iconunsnapshot", "ICONS.UNSNAPSHOT", 0 },
        { "workbench.iconleaveout", "ICONS.LEAVEOUT", 0 },
        { "workbench.iconputaway", "ICONS.PUTAWAY", 0 },
        { "workbench.delete", "ICONS.DELETE", 0 },
        { "workbench.emptytrash", "ICONS.EMPTYTRASH", 0 },
    };
    unsigned i;
    char want[96];
    CHECK_INT((int)(sizeof wb / sizeof wb[0]), 42);
    for (i = 0; i < sizeof wb / sizeof wb[0]; i++) {
        if (wb[i].root == 2) {
            check_row(wb[i].id, KA_REXX, wb[i].item);
            continue;
        }
        strcpy(want, "address WORKBENCH 'MENU WINDOW ");
        strcat(want, wb[i].root ? "ROOT" : "ACTIVE");
        strcat(want, " INVOKE ");
        strcat(want, wb[i].item);
        strcat(want, "'");
        check_row(wb[i].id, KA_REXX, want);
        CHECK(strlen(want) <= 79);
    }
    {   /* no row of the group is a hotkey, and every row of the group is in the table above */
        int j, n = 0;
        for (j = 0; j < km.nmenu; j++)
            if (!strncmp(km.menu[j].id, "workbench.", 10))
                n++;
        CHECK_INT(n, 42);
        for (j = 0; j < km.nbind; j++)
            CHECK(!strstr(km.bind[j].arg, "WORKBENCH"));
    }
}

static void cx_installs_only_the_commodity_rows(void)
{
    char buf[32768];
    long n = slurp("dist/keymap", buf, sizeof(buf));
    int i, term = 0, comm = 0;
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nbind; i++) {
        int sc = keymap_action_scope(km.bind[i].kind);
        CHECK(cx_row_installed(&km.bind[i]) == (sc != KS_TERMINAL || (km.bind[i].kind == KA_TAB && !strcmp(km.bind[i].arg, "new"))
                                                || (km.bind[i].kind == KA_MENUID && (!strcmp(km.bind[i].arg, "MENU_PREFS") || !strcmp(km.bind[i].arg, "MENU_AGENT")))));
        if (sc == KS_TERMINAL)
            term++;
        else
            comm++;
    }
    CHECK(term > 0 && comm > 0);   /* the shipped file has both, so the skip is exercised */
    {   /* s80: super+t (tab:new) with no terminal open reaches Patina, which opens one; other tab rows stay the terminal's */
        km_bind t = km.bind[0];
        t.kind = KA_TAB;
        t.arg = "new";
        CHECK(cx_row_installed(&t));
        t.arg = "close";
        CHECK(!cx_row_installed(&t));
        t.kind = KA_MENUID;   /* super+p: Patina shows the rich Settings where no terminal has focus */
        t.arg = "MENU_PREFS";
        CHECK(cx_row_installed(&t));
        t.arg = "MENU_CLOSE";
        CHECK(!cx_row_installed(&t));
    }
}

/* UPTERM.active counts a window while it has focus, and only an INACTIVEWINDOW
 * or ua_set(c, 0) takes it back. A window closed while active gets no
 * INACTIVEWINDOW, so every CloseWindow of a con's own window must be preceded
 * (within 300 bytes, same function) by ua_set(c, 0). The handler is not
 * host-linked, so the invariant is checked on its source. */
static void ua_count_released_before_own_window_closes(void)
{
    static char src[900000];
    long n = slurp("handler/vtcon_handler.c", src, sizeof(src) - 1);
    const char *p = src, *hit;
    int seen = 0;
    CHECK(n > 0);
    src[n > 0 ? n : 0] = 0;
    while ((hit = strstr(p, "CloseWindow(c->own_win)")) != 0) {
        const char *fn = hit, *u;
        int ok = 0;
        while (fn > src && !(fn[-1] == '\n' && fn[0] == '}' ) && !(fn[-1] == '\n' && fn[0] != ' ' && fn[0] != '\t' && fn[0] != '/' && fn[0] != '\n' && fn[0] != '#' && fn[0] != '}'))
            fn--;
        for (u = (hit - fn > 300 ? hit - 300 : fn); u < hit; u++)
            if (!strncmp(u, "ua_set(c, 0)", 12)) { ok = 1; break; }
        CHECK(ok);
        seen++;
        p = hit + 1;
    }
    CHECK(seen >= 2);
}

/* The handler request port's portable half (handler/keymap_act.c upreq_*): the same action a
 * bind row resolves to, with no bind row at all, junk refused. */
static void req_of(upreq_body *r, const char *action)
{
    memset(r, 0, sizeof *r);
    r->version = UPREQ_VERSION;
    strncpy(r->action, action, sizeof r->action - 1);
}

static void a_request_resolves_like_its_bind_row(void)
{
    static const char *acts[] = { "tab:new", "tab:goto 3", "menuid:MENU_COPY", "slash:/theme" };
    int i;
    for (i = 0; i < 4; i++) {
        char row[160];
        upreq_body r;
        km_bind tmp;
        kact viarow = {0, 0, 0}, viareq = {0, 0, 0};
        snprintf(row, sizeof row, "[bind]\nsuper+t = %s ; x | y\n", acts[i]);
        CHECK(parse(row));
        CHECK(km.nbind == 1 && kact_resolve(&km.bind[0], &viarow) != KACT_NONE);
        req_of(&r, acts[i]);
        CHECK_INT(upreq_resolve(&r, &viareq, &tmp), UPREQ_OK);
        CHECK_INT(viareq.kind, viarow.kind);
        CHECK_INT((int)viareq.id, (int)viarow.id);
        if (viarow.line)
            CHECK_STR(viareq.line, viarow.line);
    }
}

static void a_request_needs_no_bind_row(void)
{
    upreq_body r;
    km_bind tmp;
    kact a = {0, 0, 0};
    CHECK(parse("[bind]\n"));
    CHECK_INT(km.nbind, 0);
    req_of(&r, "slash:/theme");
    CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_OK);
    CHECK_INT(a.kind, KACT_SLASH);
    CHECK_STR(a.line, "/theme");
    req_of(&r, "tab:next");
    CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_OK);
    CHECK_INT((int)a.id, MENU_TAB_NEXT);
}

static void an_unknown_request_is_an_error_code(void)
{
    static const char *bad[] = { "", "tab", ":new", "tab:", "tab:sideways", "tab:goto 10", "menuid:MENU_NOPE",
                                 "slash:theme", "frob:x", "tab:goto 0" };
    upreq_body r;
    km_bind tmp;
    kact a = {0, 0, 0};
    int i;
    for (i = 0; i < 10; i++) {
        req_of(&r, bad[i]);
        CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_UNKNOWN);
        CHECK_INT(a.kind, KACT_NONE);
    }
    memset(&r, 'x', sizeof r);                       /* no terminator: refused, no overrun */
    r.version = UPREQ_VERSION;
    CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_UNKNOWN);
    {   /* the longest argument a binding holds fits; one byte more is refused, not copied */
        char act[KM_ARG + 8];
        memset(act, 'a', sizeof act);
        memcpy(act, "slash:/", 7);
        act[7 + KM_ARG - 2] = 0;
        req_of(&r, act);
        CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_OK);
        act[7 + KM_ARG - 2] = 'a';
        act[7 + KM_ARG - 1] = 0;
        req_of(&r, act);
        CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_UNKNOWN);
    }
    req_of(&r, "tab:new");
    r.version = UPREQ_VERSION + 1;
    CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_BADVERSION);
    req_of(&r, "run:C:Echo hi");
    CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_NOTMINE);
}

static void the_menu_builds_the_request_of_a_leaf(void)
{
    upreq_body r;
    CHECK(upreq_fill(&r, KA_SLASH, "/theme"));
    CHECK_STR(r.action, "slash:/theme");
    CHECK_INT((int)r.version, UPREQ_VERSION);
    CHECK(upreq_fill(&r, KA_MENUID, "MENU_COPY"));
    CHECK_STR(r.action, "menuid:MENU_COPY");
    CHECK(!upreq_fill(&r, KA_RUN, "C:Echo"));
}


/* DOPUS D2 (plan 2026-10-25-patina-dopus-integration.md): the three Directory Opus hotkeys parse from the
 * shipped dist/keymap, each is one rexx: row, and no other [bind] row (counted from the parsed table, not
 * grepped) takes the same key. The parser keeps the last of two rows for one key without a word, so a
 * clash would silently unbind a key: this count is the guard. */
static void dopus_binds_parse(void)
{
    static char file[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    static const struct { unsigned qual; const char *key, *script; } want[] = {
        { KM_Q_SUPER | KM_Q_SHIFT, "d", "UP-Term:Rexx/dopus-newlister.rexx" },
        { KM_Q_SUPER | KM_Q_SHIFT, "o", "UP-Term:Rexx/dopus-listerhere.rexx" },
        { KM_Q_SUPER, "d", "UP-Term:Rexx/dopus-front.rexx" },
    };
    int i, j;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    CHECK_INT(km.refused, 0);
    for (i = 0; i < 3; i++) {
        int code = keymap_key_code(want[i].key), same = 0;
        const km_bind *b = keymap_find(&km, want[i].qual, (unsigned)code);
        for (j = 0; j < km.nbind; j++)
            if (km.bind[j].qual == want[i].qual && km.bind[j].code == (unsigned)code)
                same++;
        CHECK_INT(same, 1);
        CHECK(b != 0);
        if (b) {
            CHECK_INT(b->kind, KA_REXX);
            CHECK_STR(b->arg, want[i].script);
        }
    }
}

/* DOPUS D4: the ten rows of the group, their labels and commands as the plan fixed them after D1, ids unique
 * over the whole [menu] table, every script a row runs shipped in dist/rexx, a glyph for the group. */
static void the_dopus_rows_are_shipped(void)
{
    static char file[32768], icons[32768], sc[4096];
    static const struct { const char *id, *label, *script; } rows[] = {
        { "dopus.newlister", "Open lister", "newlister" },
        { "dopus.listerhere", "Open lister at current folder", "listerhere" },
        { "dopus.front", "Bring Directory Opus to front", "front" },
        { "dopus.back", "Send Directory Opus to back", "back" },
        { "dopus.parent", "Parent folder in active lister", "parent" },
        { "dopus.refresh", "Refresh active lister", "refresh" },
        { "dopus.devicelist", "Show devices", "devicelist" },
        { "dopus.ftp", "FTP connect", "ftp" },
        { "dopus.configure", "Configure lister display format", "configure" },
    };
    long n = slurp("dist/keymap", file, sizeof(file)), m = slurp("dist/menu-icons", icons, sizeof(icons));
    int i, j, dopus = 0;
    CHECK(n > 0 && m > 0 && keymap_parse(&km, file, n));
    CHECK_INT(km.skipped, 0);
    for (i = 0; i < km.nmenu; i++) {
        for (j = i + 1; j < km.nmenu; j++)
            CHECK(strcmp(km.menu[i].id, km.menu[j].id) != 0);
        if (!strncmp(km.menu[i].id, "dopus.", 6))
            dopus++;
    }
    CHECK_INT(dopus, 11);                       /* ten rows plus the dopus.listers group (patina/index_core.h) */
    for (i = 0; i < 9; i++) {
        const km_menu *r = shipped_row(rows[i].id);
        char path[96], arg[96];
        CHECK(r != 0);
        if (!r)
            continue;
        CHECK_INT(r->kind, KA_REXX);
        CHECK_STR(r->label, rows[i].label);
        snprintf(arg, sizeof arg, "UP-Term:Rexx/dopus-%s.rexx", rows[i].script);
        CHECK_STR(r->arg, arg);
        snprintf(path, sizeof path, "dist/rexx/dopus-%s.rexx", rows[i].script);
        CHECK(slurp(path, sc, sizeof sc) > 0);
        if (!strcmp(rows[i].script, "refresh"))
            CHECK(strstr(sc, "'lister refresh all'") != 0);
        if (!strcmp(rows[i].script, "ftp"))
            CHECK(strstr(sc, "TLS") == 0 || strstr(sc, "no TLS") != 0);
    }
    check_row("dopus.shell", KA_OPEN, "uterm");
    check_row("dopus.listers", KA_MENU, "dopus.listers");
    CHECK(strstr(icons, "\ndopus ") != 0 && strstr(icons, "\ndopus.newlister") != 0);
}

/* Owner's A1200, 2026-10-22: Preferences from the quick menu did nothing the first time. The
 * menu window that closed was blanked out of the focus record, and the next menu window then
 * took the blank as "the terminal before me": the request found no port. The record keeps the
 * live windows in order, so the terminal stays the menu's target whatever closed in between. */
static void the_quick_menu_finds_its_terminal_after_an_earlier_menu_window_closed(void)
{
    unsigned long l[UPMRU_N] = {0};
    char name[24];
    unsigned long a = 0x1000, b = 0x2000, m1 = 0x3000, m2 = 0x4000;
    int i;
    upmru_focus(l, a);
    CHECK(upmru_target(l, 1) == 0);                 /* one window: nothing before it */
    upmru_focus(l, m1);
    CHECK(upmru_target(l, 1) == a);
    upmru_forget(l, m1);                       /* the first menu closes, no window active */
    upmru_focus(l, m2);                        /* the second menu, straight after */
    CHECK(upmru_target(l, 1) == a);
    upmru_forget(l, m2);
    upmru_focus(l, b);                         /* another terminal, then back to the first */
    upmru_focus(l, a);
    upmru_focus(l, m1);
    CHECK(upmru_target(l, 1) == a);
    upmru_forget(l, a);                        /* the target closes: the one before it */
    CHECK(upmru_target(l, 1) == b);
    upmru_forget(l, b);
    CHECK(upmru_target(l, 1) == 0);                 /* only the menu left */
    for (i = 1; i <= UPMRU_N + 3; i++)         /* more windows than slots: the newest stay */
        upmru_focus(l, (unsigned long)i * 16);
    CHECK(l[0] == (unsigned long)(UPMRU_N + 3) * 16 && upmru_target(l, 1) == (unsigned long)(UPMRU_N + 2) * 16);
    upreq_portname(0x0812abcdUL, name);
    CHECK_STR(name, "UPTERM.req.0812abcd");
}

/* TERM-ROWS-ONLY-OVER-TERMINAL: the graphical menu is a plain Intuition window that never enters
 * the record, so with one terminal open the record is [term] and the terminal is the FRONT entry;
 * the text menu is in the record ([menu, term]) and its terminal is the one behind it. */
static void the_target_is_the_front_entry_for_a_menu_outside_the_record(void)
{
    unsigned long l[UPMRU_N] = {0};
    unsigned long term = 0x1000, menu = 0x2000;
    upmru_focus(l, term);
    CHECK(upmru_target(l, 0) == term);         /* graphical menu, one terminal */
    CHECK(upmru_target(l, 1) == 0);            /* a text menu would find nothing before it */
    upmru_focus(l, menu);
    CHECK(upmru_target(l, 1) == term);         /* text menu in the record: the one behind */
    upmru_forget(l, term);
    upmru_forget(l, menu);
    CHECK(upmru_target(l, 0) == 0);            /* empty record: no target either way */
}

/* Owner's A1200, 2026-10-22: Style > Background just exited. /bg takes a colour the row does
 * not carry; the window refused, replied OK and the menu closed with nothing said. A refusal is
 * never OK: a missing argument asks for it (the usage goes back), any other refusal says why. */
static void a_refused_slash_request_answers_why_and_a_missing_value_is_asked_for(void)
{
    upreq_body r, sent;
    CHECK(upreq_fill(&r, KA_SLASH, "/bg"));
    /* A1200 retest of de3a4cc6: the value prompt read "Background ():". The window's answer
     * lives in the message's copy of the body (handler/upreq.c upreq_send); the sender reads
     * its own request, so the reply must come back into it, and the answer is the table's
     * usage alone, not slash_run's "bg: ..." line. */
    sent = r;
    CHECK_INT((int)upreq_slash_answer(&sent, 2, "bg: RRGGBB | none\n", "RRGGBB | none"), UPREQ_NEEDARG);
    sent.rc = UPREQ_NEEDARG;
    CHECK_INT((int)upreq_take_reply(&r, &sent), UPREQ_NEEDARG);
    CHECK_STR(r.answer, "RRGGBB | none");
    CHECK_INT((int)upreq_slash_answer(&r, 2, "bg: 12345 is not RRGGBB\n", 0), UPREQ_FAILED);
    CHECK_STR(r.answer, "bg: 12345 is not RRGGBB");
    CHECK_INT((int)upreq_slash_answer(&r, 1, "bg: 102030\n", 0), UPREQ_OK);
    CHECK_STR(r.answer, "");
    CHECK(upreq_with_arg(&r, "slash:/bg", "102030"));
    CHECK_STR(r.action, "slash:/bg 102030");
    CHECK_INT((int)r.version, UPREQ_VERSION);
    {
        kact a;
        km_bind tmp;
        CHECK_INT(upreq_resolve(&r, &a, &tmp), UPREQ_OK);
        CHECK(a.kind == KACT_SLASH && !strcmp(a.line, "/bg 102030"));
    }
    CHECK(!upreq_with_arg(&r, "slash:/bg", ""));
}

/* The table sits on the heap sized to its content, and the commodity's reload
 * (keymap_reload: parse into a scratch, keymap_swap on success, keymap_free of
 * what is left) keeps the old map on a bad edit and leaks nothing over many
 * reloads. Counted by the host build's allocation counter (VT_COUNT_ALLOC). */
static void a_reload_swaps_whole_keeps_the_old_map_on_a_bad_edit_and_leaks_nothing(void)
{
    static const char good[] = "[bind]\nsuper+k = viewer ; Keys | menu\nsuper+t = tab:new ; New tab | tab\n[menu]\napps.x = X | run:x\n";
    static const char other[] = "[bind]\nsuper+j = run:j ; J | apps\n";
    static const char newer[] = "[keymap]\nversion = 9\n[bind]\nsuper+q = run:q\n";
    long base = vt_count_live, one, i;
    keymap live, scratch;
    memset(&live, 0, sizeof live);
    memset(&scratch, 0, sizeof scratch);
    CHECK(keymap_parse(&live, good, (long)strlen(good)));
    one = vt_count_live - base;
    CHECK(one > 0 && one < 4096);               /* sized to 2 binds + 1 menu, not to the caps */
    for (i = 0; i < 50; i++) {
        /* a bad edit: refused whole, the live table is untouched, nothing stays held */
        CHECK(!keymap_parse(&scratch, newer, (long)strlen(newer)));
        keymap_free(&scratch);
        CHECK_INT(live.nbind, 2);
        CHECK(keymap_find(&live, KM_Q_SUPER, RAW_K) != 0);
        CHECK_INT((int)(vt_count_live - base), (int)one);
        /* a good edit: swapped in, the old one released */
        CHECK(keymap_parse(&scratch, good, (long)strlen(good)));
        keymap_swap(&live, &scratch);
        CHECK_INT((int)(vt_count_live - base), (int)one);
        CHECK(scratch.bind == 0 && scratch.pool == 0);
    }
    CHECK(keymap_parse(&scratch, other, (long)strlen(other)));
    keymap_swap(&live, &scratch);
    CHECK_INT(live.nbind, 1);
    CHECK(keymap_find(&live, KM_Q_SUPER, RAW_K) == 0);
    keymap_free(&live);
    CHECK_INT((int)(vt_count_live - base), 0);
}

/* The Patina commodity holds only its own rows, at exact size: the heap bytes of dist/keymap
 * after keymap_compact(cx_row_installed) stay under a ceiling (the rig measured 17944 B for the
 * parsed table, 8288 B on the host), carry no
 * [menu] rows, no slack beyond the block headers, and every kept row is found unchanged. */
#define PATINA_KEYMAP_HEAP_MAX 5400 /* P3-D1: 68 rows, 4996 B measured (was 3600 for 49 rows) */  /* 49 rows (the three cap: hotkeys, R4.7; was 3400 for 46) (8 + the 18 window hotkeys + the three Directory Opus hotkeys + the three Screens hotkeys + the 15 AmTile layout hotkeys); 3365 B measured; the full parsed table is larger */
static void the_commodity_keymap_of_dist_keymap_is_exact_and_small(void)
{
    static char file[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    long base = vt_count_live, parsed, held, want = 0;
    int i, kept = 0, ref_n;
    keymap km, ref;
    memset(&km, 0, sizeof km);
    memset(&ref, 0, sizeof ref);
    CHECK(n > 0);
    CHECK(keymap_parse(&ref, file, n));
    CHECK(keymap_parse(&km, file, n));
    parsed = vt_count_live - base;
    for (i = 0; i < ref.nbind; i++)
        if (cx_row_installed(&ref.bind[i])) {
            const km_bind *b = &ref.bind[i];
            kept++;
            want += (long)sizeof(km_bind) + (long)strlen(b->arg) + (long)strlen(b->label) + (long)strlen(b->cat) + 3;
        }
    CHECK(kept > 0 && kept < ref.nbind);
    ref_n = ref.nbind;
    CHECK(keymap_compact(&km, cx_row_installed));
    keymap_free(&ref);
    held = vt_count_live - base;
    CHECK_INT(km.nbind, kept);
    CHECK_INT(km.nmenu, 0);
    CHECK(km.menu == 0 && km.mcap == 0 && km.bcap == kept);
    for (i = 0; i < km.nbind; i++) {
        const km_bind *b = &km.bind[i];
        CHECK(cx_row_installed(b));
        CHECK(keymap_find(&km, b->qual, b->code) == b);
    }
    CHECK(kept < ref_n);
    CHECK(held < parsed);
    CHECK(held <= want + 64);                   /* exact: only the string block header is slack */
    CHECK(held <= PATINA_KEYMAP_HEAP_MAX);
    keymap_free(&km);
    CHECK_INT((int)(vt_count_live - base), 0);
}

static void a_scan_leaf_parses_to_ka_scan_and_a_detach_leaf_stays_ka_detach(void)
{
    CHECK(parse("[keymap]\nversion = 1\n[menu]\napps.rescan = Rescan apps | scan:patina-index\napps.bg = Bg | detach:foo\n"));
    CHECK_INT(km.nmenu, 2);
    CHECK_INT(km.menu[0].kind, KA_SCAN);
    CHECK_STR(km.menu[0].arg, "patina-index");
    CHECK_INT(km.menu[1].kind, KA_DETACH);
}

void suite_keymap(void)
{
    a_scan_leaf_parses_to_ka_scan_and_a_detach_leaf_stays_ka_detach();
    the_commodity_keymap_of_dist_keymap_is_exact_and_small();
    a_reload_swaps_whole_keeps_the_old_map_on_a_bad_edit_and_leaks_nothing();
    comments_crlf_and_fields();
    digits_expand_with_percent_n();
    junk_file_keeps_the_old_keymap();
    bad_rows_are_skipped_and_counted();
    the_safety_refusals();
    a_super_alt_row_is_refused_and_super_ctrl_loads();
    the_shipped_keymap_has_no_super_alt_chord();
    a_newer_major_is_rejected_whole();
    a_later_row_replaces_and_none_unbinds();
    the_user_file_overrides_the_shipped_one();
    the_tables_have_caps();
    key_names_are_raw_codes();
    symbol_names_follow_the_printed_key();
    actions_resolve_to_menu_ids();
    win_actions_parse();
    bar_actions_parse();
    win_scope_is_commodity();
    the_shipped_snap_focus_float_and_swap_keys_are_bound_and_clash_free();
    win_defers_in_the_handler();
    the_shipped_file_resolves_and_derives_menu_keys();
    the_window_group_rows_and_hotkeys();
    the_screens_group_rows_and_hotkeys();
    the_workbench_group_rows();
    cx_installs_only_the_commodity_rows();
    ua_count_released_before_own_window_closes();
    a_request_resolves_like_its_bind_row();
    a_request_needs_no_bind_row();
    an_unknown_request_is_an_error_code();
    the_menu_builds_the_request_of_a_leaf();
    dopus_binds_parse();
    the_dopus_rows_are_shipped();
    the_quick_menu_finds_its_terminal_after_an_earlier_menu_window_closed();
    the_target_is_the_front_entry_for_a_menu_outside_the_record();
    a_refused_slash_request_answers_why_and_a_missing_value_is_asked_for();
}
