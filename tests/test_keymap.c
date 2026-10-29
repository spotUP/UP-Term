/* The keymap parser (config/keymap.c): syntax, expansion, refusals,
 * overrides, caps, and the shipped dist/keymap. No OS calls: a host suite. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../config/upconf.h"
#include "../config/keymap.h"
#include "../handler/keymap_act.h"
#include "../handler/menu_ids.h"
#include "../amarchy/cxdesc.h"
#include "../amarchy/bar_core.h"
#include "../handler/slash.h"
#include "../amarchy/menu_core.h"
#include "../prefs/settings_def.h"
#include "../amarchy/menu_geom.h"
#include "../amarchy/leaf.h"
#include "../amarchy/menu_icon.h"
#include "../amarchy/icon_core.h"
#include "../amarchy/index_core.h"
#include "../render/emoji.h"
#include "../engine/vtengine.h"
#include "harness.h"

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
                "super+ctrl+t = viewer\n"             /* both Amigas and Ctrl: reset */
                "super+shift+ctrl+k = viewer\n"
                "super+k = viewer\n"));
    CHECK_INT(km.nbind, 1);
    CHECK_INT(km.skipped, 7);
    CHECK_INT(km.refused, 7);
    CHECK(keymap_find(&km, KM_Q_SUPER, 0x37) == 0);    /* m */
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_CTRL, RAW_T) == 0);
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
        static const char *const q[] = { "super", "super+shift", "super+alt", "super+rshift",
                                         "super+shift+alt", "super+alt+rshift", "super+shift+rshift",
                                         "super+shift+alt+rshift" };
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

static int word_in(const char *hay, const char *word)
{
    size_t n = strlen(word);
    const char *p = hay;
    while ((p = strstr(p, word)) != 0) {
        char before = p == hay ? ' ' : p[-1];
        char after = p[n];
        int bw = (before >= 'A' && before <= 'Z') || before == '_' || (before >= '0' && before <= '9');
        int aw = (after >= 'A' && after <= 'Z') || after == '_' || (after >= '0' && after <= '9');
        if (!bw && !aw)
            return 1;
        p += n;
    }
    return 0;
}

static void the_shipped_keymap_loads_whole(void)
{
    static char file[32768], ids[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    long m = slurp("handler/menu_ids.h", ids, sizeof(ids));
    int i, scoped_menu = 0;
    CHECK(n > 0 && m > 0);
    if (n <= 0 || m <= 0)
        return;
    CHECK(keymap_parse(&km, file, n));
    CHECK_INT(km.skipped, 0);
    CHECK_INT(km.refused, 0);
    CHECK_INT(km.overflow, 0);
    CHECK_INT(km.version, KM_VERSION);
    CHECK(km.nbind >= 30 && km.nbind <= 100);
    CHECK(km.nmenu > 0);
    for (i = 0; i < km.nbind; i++) {
        const km_bind *b = &km.bind[i];
        CHECK(b->qual & KM_Q_SUPER);
        CHECK(!(b->qual & KM_Q_CTRL));
        CHECK(!(b->code == 0x37 || b->code == 0x36));   /* never m or n */
        CHECK(b->label[0] != 0);
        if (b->kind == KA_MENUID) {
            CHECK(word_in(ids, b->arg));
            scoped_menu++;
        }
    }
    CHECK(scoped_menu > 0);
    for (i = 0; i < km.nmenu; i++) {
        if (km.menu[i].kind == KA_MENUID)
            CHECK(word_in(ids, km.menu[i].arg));
    }
    /* every shipped run: row is a line lf_shell_command can hand to `vsh -c "..."`: a double quote
     * in the row ends that argument, so the leaf refuses it ("the command line is not valid",
     * Settings > Amarchy > Amarchy preferences shipped as run:"C:UP-Term-Prefs" AMARCHY); vsh
     * takes single quotes for a path with a space */
    for (i = 0; i < km.nbind; i++) {
        char line[512];
        if (km.bind[i].kind == KA_RUN || km.bind[i].kind == KA_JOB)
            CHECK(lf_shell_command(line, sizeof line, "UP-Term:bin/vsh", km.bind[i].arg));
    }
    for (i = 0; i < km.nmenu; i++) {
        char line[512];
        if (km.menu[i].kind == KA_RUN || km.menu[i].kind == KA_JOB)
            CHECK(lf_shell_command(line, sizeof line, "UP-Term:bin/vsh", km.menu[i].arg));
    }
    /* every shipped rexx: bind and row builds an RX line; a script path goes to RX bare, because RX
     * runs a quoted first argument as inline program text (the DOpus hotkeys and menu rows built
     * RX "UP-Term:Rexx/dopus-newlister.rexx" and never opened the script, A1200 2026-10-26) */
    {
        int scripts = 0;
        for (i = 0; i < km.nbind + km.nmenu; i++) {
            int kind = i < km.nbind ? km.bind[i].kind : km.menu[i - km.nbind].kind;
            const char *arg = i < km.nbind ? km.bind[i].arg : km.menu[i - km.nbind].arg;
            char line[512];
            if (kind != KA_REXX)
                continue;
            CHECK(lf_rexx_command(line, sizeof line, arg));
            if (!strncmp(arg, "UP-Term:Rexx/", 13)) {
                CHECK(strchr(line, '"') == 0);
                scripts++;
            }
        }
        CHECK(scripts >= 12);
    }
    /* the keys the plan names */
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_K) && keymap_find(&km, KM_Q_SUPER, RAW_K)->kind == KA_VIEWER);
    CHECK(keymap_find(&km, KM_Q_SUPER, RAW_SPACE) != 0);
    /* the low-value rows are shipped commented out */
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT, 0x12) == 0);   /* shift+e: emoji */
    CHECK(keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT, 0x10) == 0);   /* shift+q: reboot */
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

static void win_actions_parse(void)
{
    static const char *const good[] = {
        "cycle next", "cycle prev", "front", "back", "zip", "fullwidth", "fullscreen", "save", "restore",
        "move left", "move right", "move up", "move down", "move left 1", "move down 9999", "move up 0032",
        "size left", "size right", "size up", "size down", "size right 64", "size down 1234",
        "focus left", "focus right", "focus up", "focus down", "focus next", "focus prev",
        "closedrawers", "screen next", "screen prev", "screen workbench", "screen back",
        "title toggle", "title show", "title hide", 0 };
    static const char *const bad[] = {
        "bogus", "cycle", "cycle left", "cycle next now", "front now", "zip 1", "save x", "restore 3", "closedrawers all",
        "move", "move sideways", "move left 0", "move left 00000", "move left 12345", "move left -5", "move left 3x",
        "move left 32 64", "size", "size next", "size up 0", "size up 10000", "size up -1",
        "focus", "focus left 5", "focus sideways", "screen", "screen left", "screen next 1",
        "title", "title flip", "title hide now", "Front", " front", 0 };
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

static int root_has_group(const char *label, int term)
{
    mn_item it[MN_MAX_ITEMS];
    int i, n = mn_children_t(&km, "", it, MN_MAX_ITEMS, term);
    for (i = 0; i < n; i++)
        if (MN_IS_GROUP(&it[i]) && !strcmp(it[i].label, label))
            return 1;
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

static void count_level(const mn_item *items, int n, void *ctx)
{
    (void)items; (void)n;
    ++*(int *)ctx;
}

/* M1: a settings page group (settings.<page> = Label | settings:<page>) is a group in its parent and lists the
 * page's rows of the settings table, in table order, as MN_ROW_SETTING rows; count and order from the table. */
static void settings_page_group_lists_the_table_rows(void)
{
    mn_item it[MN_MAX_ITEMS];
    keymap pk;
    int page, i, n, want, seen_pages = 0;
    char text[1024], line[128];
    memset(&pk, 0, sizeof pk);
    strcpy(text, "[menu]\n");
    for (page = 0; page < PG_NPAGES; page++) {
        strcpy(line, "settings.");
        strcat(line, sd_page_name(page));
        strcat(line, " = Page X | settings:");
        strcat(line, sd_page_name(page));
        strcat(line, "\n");
        strcat(text, line);
    }
    strcat(text, "settings.other = Other | run:x\n");
    CHECK(keymap_parse(&pk, text, (long)strlen(text)));
    /* the page lines are groups under settings, labelled by the file */
    n = mn_children(&pk, "settings", it, MN_MAX_ITEMS);
    CHECK_INT(n, PG_NPAGES + 1);
    for (i = 0; i < PG_NPAGES; i++) {
        CHECK_INT(it[i].row, MN_ROW_GROUP);
        CHECK(!strncmp(it[i].label, "Page ", 5));
    }
    CHECK_INT(it[PG_NPAGES].row >= 0, 1);
    for (page = 0; page < PG_NPAGES; page++) {
        char node[32];
        int k = 0;
        strcpy(node, "settings.");
        strcat(node, sd_page_name(page));
        want = 0;
        for (i = 0; i < sd_nsettings; i++)
            want += sd_settings[i].page == page;
        n = mn_children(&pk, node, it, MN_MAX_ITEMS);
        CHECK_INT(n, want);
        CHECK(want > 0);
        for (i = 0; i < sd_nsettings; i++) {
            if (sd_settings[i].page != page)
                continue;
            if (k < n) {
                CHECK_INT(it[k].row, MN_ROW_SETTING);
                CHECK_INT(it[k].ix, sd_settings[i].id);
                CHECK(strchr(it[k].label, '_') == 0);
            }
            k++;
        }
        seen_pages += n;
    }
    /* every settings row of a page appears once across the six groups */
    for (want = 0, i = 0; i < sd_nsettings; i++)
        want += sd_settings[i].page >= 0 && sd_settings[i].page < PG_NPAGES;
    CHECK_INT(seen_pages, want);
    /* the walk the window sizes use visits the page levels too */
    {
        int levels = 0;
        mn_levels(&pk, "settings", count_level, &levels);
        CHECK_INT(levels, 1 + PG_NPAGES);
    }
    keymap_free(&pk);
}

/* Right enters what Return enters (a group, a cycle, colour or list setting) and nothing else: a leaf, an index
 * row, a choice row, a check/number/text setting stay put. */
static void right_enters_only_rows_with_children(void)
{
    mn_item it;
    int i, enter = 0;
    memset(&it, 0, sizeof it);
    it.row = MN_ROW_GROUP;
    CHECK_INT(mn_right_enters(&it), 1);
    it.row = 0;
    CHECK_INT(mn_right_enters(&it), 0);
    it.row = MN_ROW_INDEX;
    CHECK_INT(mn_right_enters(&it), 0);
    it.row = MN_ROW_CHOICE;
    CHECK_INT(mn_right_enters(&it), 0);
    it.row = MN_ROW_SETTING;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        int want = s->kind == SD_CYCLE || SD_HAS_PICKER(s) || SD_HAS_LIST(s);
        it.ix = s->id;
        CHECK_INT(mn_right_enters(&it), want);
        enter += want;
    }
    CHECK(enter > 0);
}

/* M9: the shipped dist/keymap carries the six Settings page groups, in table order, each listing its
 * settings-table rows, and the Preferences row that opens Prefs stays. */
static void shipped_settings_has_the_six_page_groups(void)
{
    mn_item it[MN_MAX_ITEMS], pg[MN_MAX_ITEMS];
    int i, n, p, k, want, at = -1;
    char node[40];
    const km_menu *pref = shipped_row("settings.amarchy.preferences");
    n = mn_children(&km, "settings", it, MN_MAX_ITEMS);
    for (p = 0; p < PG_NPAGES; p++) {
        int found = -1;
        for (i = 0; i < n; i++)
            if (it[i].row == MN_ROW_GROUP &&
                !strcmp(it[i].seg, sd_page_name(p)))
                found = i;
        CHECK(found >= 0);
        CHECK(found > at);
        at = found;
        strcpy(node, "settings.");
        strcat(node, sd_page_name(p));
        want = 0;
        for (i = 0; i < sd_nsettings; i++)
            want += sd_settings[i].page == p;
        /* M9b: Amarchy is one entry; the page rows come first, then its static children */
        if (p == PG_PAGE_AMARCHY) {
            int st = 0, j;
            for (j = 0; j < km.nmenu; j++)
                st += !strncmp(km.menu[j].id, "settings.amarchy.", 17);
            CHECK(st > 0);
            want += st;
        }
        k = mn_children(&km, node, pg, MN_MAX_ITEMS);
        CHECK_INT(k, want);
        CHECK(want > 0);
        if (p == PG_PAGE_AMARCHY && k == want) {
            CHECK_INT(pg[0].row, MN_ROW_SETTING);
            CHECK(pg[k - 1].row >= 0);
        }
    }
    /* one Amarchy entry under Settings: no second group, no settings.amarchysettings row */
    for (i = 0, k = 0; i < n; i++)
        k += it[i].row == MN_ROW_GROUP && !strcmp(it[i].seg, "amarchy");
    CHECK_INT(k, 1);
    CHECK(shipped_row("settings.amarchysettings") == 0);
    CHECK(pref != 0);
    if (pref) {
        CHECK_INT(pref->kind, KA_DETACH);
        CHECK(!strcmp(pref->arg, "C:UP-Term-Prefs AMARCHY"));
    }
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
    {   /* the 15 AmTile layout hotkeys (Super+Alt family); the plain Super+<key> bindings keep their old kinds */
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
            const km_bind *b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, at[i].code);
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
        const km_bind *b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, 0x14);
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
            { KM_Q_SUPER|KM_Q_ALT, 0x18, "back" },
            { KM_Q_SUPER, 0x31, "zip" },
            { KM_Q_SUPER|KM_Q_ALT, 0x23, "fullwidth" },
            { KM_Q_SUPER|KM_Q_ALT, 0x21, "save" },
            { KM_Q_SUPER|KM_Q_ALT, 0x13, "restore" },
            { KM_Q_SUPER|KM_Q_ALT, 0x42, "cycle next" },
            { KM_Q_SUPER|KM_Q_ALT|KM_Q_SHIFT, 0x42, "cycle prev" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4F, "move left" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4E, "move right" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4C, "move up" },
            { KM_Q_SUPER|KM_Q_SHIFT, 0x4D, "move down" },
            { KM_Q_SUPER|KM_Q_ALT, 0x4F, "size left" },
            { KM_Q_SUPER|KM_Q_ALT, 0x4E, "size right" },
            { KM_Q_SUPER|KM_Q_ALT, 0x4C, "size up" },
            { KM_Q_SUPER|KM_Q_ALT, 0x4D, "size down" },
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
            { KM_Q_SUPER|KM_Q_ALT, 0x35, "title toggle" },
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

static void the_shipped_menu_has_learn_system_and_install_groups(void)
{
    static char buf[32768];
    long n = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    CHECK(root_has_group("Learn", 1) && root_has_group("System", 1) && root_has_group("Install", 1));
    CHECK(root_has_group("Learn", 0) && root_has_group("System", 0) && root_has_group("Install", 0));
    check_row("learn.keys", KA_VIEWER, "");
    /* Super + L opens the configured AI agent (MENU_AGENT), not Claude directly; Claude and pi keep their Apps rows */
    {
        int i, agent = 0, claude = 0;
        for (i = 0; i < km.nbind; i++) {
            agent += km.bind[i].kind == KA_MENUID && !strcmp(km.bind[i].arg, "MENU_AGENT");
            claude += km.bind[i].kind == KA_MENUID && !strcmp(km.bind[i].arg, "MENU_CLAUDE");
        }
        CHECK_INT(agent, 1);
        CHECK_INT(claude, 0);
        check_row("apps.pi", KA_MENUID, "MENU_PI");
        check_row("apps.claude", KA_MENUID, "MENU_CLAUDE");
    }
    check_row("learn.tutorial", KA_MENUID, "MENU_TUTORIAL");
    check_row("learn.help", KA_MENUID, "MENU_HELP_SLASH");
    check_row("learn.demo", KA_SLASH, "/demo");
    check_row("system.reboot", KA_DETACH, "Reboot");  /* run:Reboot opened an UP-Term window (A1200 2026-10-26) */
    CHECK_INT((int)lf_plan(KA_DETACH), (int)LF_DETACH);  /* detach: NIL: in and out, no window */
    check_row("system.workbench", KA_SLASH, "/screen workbench");
    check_row("system.own", KA_SLASH, "/screen own");
    check_row("install.browse", KA_RUN, "upkg browse");
    check_row("install.upgrade", KA_JOB, "upkg upgrade");
    check_row("install.update", KA_JOB, "upkg update");
    check_row("trigger.clipboard", KA_MENU, "trigger.clipboard");   /* a group of ring rows (tests/test_clips.c) */
    CHECK(root_has_group("Trigger", 1) && root_has_group("Trigger", 0));
    CHECK(shipped_row("apps.upkg") == 0);                  /* moved under Install */
    CHECK_INT((int)lf_plan(KA_WIN), (int)LF_WIN);          /* a win: row goes to the commodity, not to a terminal request */
    CHECK_INT((int)lf_plan(KA_VIEWER), (int)LF_VIEWER);    /* a viewer row runs the viewer, not a terminal request */
    {   /* the viewer is started by its full path beside the menu, never by a bare name through a PATH */
        char vl[64];
        CHECK_INT(lf_viewer_command(vl, sizeof vl, "UP-Term:bin"), 1);
        CHECK_STR(vl, "\"UP-Term:bin/amarchy-gui\" viewer");
        CHECK_INT(lf_viewer_command(vl, sizeof vl, "VTC:"), 1);
        CHECK_STR(vl, "\"VTC:amarchy-gui\" viewer");
        CHECK_INT(lf_viewer_command(vl, sizeof vl, ""), 0);
        CHECK_INT(lf_viewer_command(vl, sizeof vl, "a\"b"), 0);
        CHECK_INT(lf_viewer_command(vl, 10, "UP-Term:bin"), 0);
        /* a lister row raises one lister with documented DOpus 5 commands; `lister activate` is not one */
        CHECK_INT(lf_lister_front_command(vl, sizeof vl, 0, 77), 1);
        CHECK_STR(vl, "dopus front");
        CHECK_INT(lf_lister_front_command(vl, sizeof vl, 1, 77), 1);
        CHECK_STR(vl, "lister set 77 visible off");
        CHECK_INT(lf_lister_front_command(vl, sizeof vl, 2, 77), 1);
        CHECK_STR(vl, "lister set 77 visible on");
        CHECK_INT(lf_lister_front_command(vl, sizeof vl, 3, 77), 0);
    }
    /* without a terminal window the menuid and slash rows hide; Learn keeps the viewer, System keeps Reboot */
    {
        mn_item it[MN_MAX_ITEMS];
        int i, nn = mn_children_t(&km, "learn", it, MN_MAX_ITEMS, 0), keys = 0, demo = 0;
        for (i = 0; i < nn; i++) {
            keys += !strcmp(it[i].label, "Key bindings");
            demo += !strcmp(it[i].label, "Demo tour");
        }
        CHECK_INT(keys, 1);
        CHECK_INT(demo, 0);
        nn = mn_children_t(&km, "learn", it, MN_MAX_ITEMS, 1);
        CHECK_INT(nn, 4);
    }
}
/* ---- end Omarchy groups ---- */

/* commodities.library MatchIX, as a care mask: ((ie ^ ix) & mask) == 0 for code and qualifier */
static int matchix(const cx_ixspec_t *x, unsigned code, unsigned qual)
{
    return !(((code ^ x->code) & x->code_mask) || ((qual ^ x->qual) & x->qual_mask));
}

static void cx_filter_models_matchix(void)
{
    cx_ixspec_t x;
    const km_bind *b;
    CHECK(parse("[bind]\nsuper+alt+j = viewer\n"));
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, 0x26);
    CHECK(b != 0);
    cx_ixspec(b, &x);
    CHECK(matchix(&x, 0x26, 0x0040 | 0x0010 | 0x8000));  /* Lamiga+Alt+J, relative-mouse bit set */
    CHECK(!matchix(&x, 0x26 | 0x80, 0x0040 | 0x0010 | 0x8000)); /* the key-up */
    CHECK(!matchix(&x, 0x27, 0x0040 | 0x0010));           /* another key */
    CHECK(!matchix(&x, 0x26, 0x0010));                    /* Left Amiga up */
}

static void cx_gate_takes_only_its_own_row(void)
{
    /* the super+h row: super+h only, not super+shift+h, not the up stroke */
    CHECK(cx_row_takes(0x25, KM_Q_SUPER, 0x25, 0x0040));
    CHECK(!cx_row_takes(0x25, KM_Q_SUPER, 0x25, 0x0041));
    CHECK(!cx_row_takes(0x25, KM_Q_SUPER, 0x25 | 0x80, 0x0040));
    CHECK(!cx_row_takes(0x25, KM_Q_SUPER, 0x27, 0x0040));
    CHECK(cx_row_takes(0x25, KM_Q_SUPER | KM_Q_SHIFT, 0x25, 0x0041));
}

static void cx_helper_path(void)
{
    char out[64];
    CHECK(cx_helper_cmd(out, sizeof(out), "UP-Term:bin", "amarchy-menu", "menu root"));
    CHECK_STR(out, "\"UP-Term:bin/amarchy-menu\" menu root");
    CHECK(cx_helper_cmd(out, sizeof(out), "UP-Term:", "amarchy-menu", "viewer"));
    CHECK_STR(out, "\"UP-Term:amarchy-menu\" viewer");
    CHECK(!cx_helper_cmd(out, 20, "UP-Term:bin", "amarchy-menu", "viewer"));   /* does not fit */
    CHECK(!cx_helper_cmd(out, sizeof(out), "", "amarchy-menu", "viewer"));
    /* TERM unset (as on an Amiga): the helper defaults to vtcon; a TERM that is set is kept */
    CHECK_STR(mn_term_default(0), "vtcon");
    CHECK_STR(mn_term_default(""), "vtcon");
    CHECK(mn_term_default("xterm") == 0);      /* no directory resolved */
}

/* G6: the command each of the four menu actions builds under each menu-style value; the style
 * parsed from upconf text; the new actions parsed from the keymap and shipped in dist/keymap. */
static void menu_style_commands(void)
{
    static const struct {
        int kind, style;
        const char *node, *want;
        int text;
    } t[] = {
        { KA_MENU,        UC_MENU_GRAPHICAL, "apps", "\"UP-Term:bin/amarchy-gui\" menu apps", 0 },
        { KA_VIEWER,      UC_MENU_GRAPHICAL, "",     "\"UP-Term:bin/amarchy-gui\" viewer", 0 },
        { KA_MENU,        UC_MENU_TEXT,      "apps", "\"UP-Term:bin/amarchy-menu\" menu apps", 1 },
        { KA_VIEWER,      UC_MENU_TEXT,      "",     "\"UP-Term:bin/amarchy-menu\" viewer", 1 },
        { KA_MENU,        UC_MENU_MINIMAL,   "apps", "\"UP-Term:bin/amarchy-gui\" menu apps", 0 },
        { KA_VIEWER,      UC_MENU_MINIMAL,   "",     "\"UP-Term:bin/amarchy-gui\" viewer", 0 },
    };
    char out[96];
    int i, text;
    for (i = 0; i < 6; i++) {
        text = -1;
        CHECK(cx_menu_cmd(out, sizeof(out), "UP-Term:bin", t[i].kind, t[i].node, t[i].style, &text));
        CHECK_STR(out, t[i].want);
        CHECK_INT(text, t[i].text);
    }
    CHECK(!cx_menu_cmd(out, sizeof(out), "UP-Term:bin", KA_RUN, "", UC_MENU_TEXT, &text));
    CHECK(!cx_menu_cmd(out, 20, "UP-Term:bin", KA_VIEWER, "", UC_MENU_TEXT, &text));
}

static void menu_style_parse(void)
{
#define STYLE(s) upconf_menu_style(s, (long)strlen(s))
    CHECK_INT(STYLE(""), UC_MENU_GRAPHICAL);
    CHECK_INT(STYLE("scrollback = 5\n"), UC_MENU_GRAPHICAL);
    CHECK_INT(STYLE("menu-style = text\n"), UC_MENU_TEXT);
    CHECK_INT(STYLE("[profile default]\nMenu-Style = TEXT\n"), UC_MENU_TEXT);
    CHECK_INT(STYLE("menu-style = text\nmenu-style = graphical\n"), UC_MENU_GRAPHICAL);   /* last wins */
    CHECK_INT(STYLE("menu-style = bogus\n"), UC_MENU_GRAPHICAL);
    CHECK_INT(STYLE("menu-style = minimal\n"), UC_MENU_MINIMAL);
    CHECK_INT(STYLE("menu-style = Minimal\nmenu-style = text\n"), UC_MENU_TEXT);        /* last wins */
    CHECK_INT(STYLE("menu-style = text\nmenu-style = minimal\n"), UC_MENU_MINIMAL);
    CHECK_INT(upconf_app_icons(""), UC_ICONS_GENERIC);
    CHECK_INT(upconf_app_icons("amiga"), UC_ICONS_AMIGA);
    CHECK_INT(upconf_app_icons("none"), UC_ICONS_NONE);
    CHECK_INT(upconf_app_icons("bogus"), UC_ICONS_GENERIC);
    CHECK_INT(STYLE("[profile vim]\nmenu-style = text\n"), UC_MENU_GRAPHICAL);            /* default profile only */
#undef STYLE
}

/* Menu in terminal: the style a menu hotkey opens is chosen by cx_menu_style, the function amarchy.c's
 * act_menu_kind passes to cx_menu_cmd; the other-style bindings are gone from the keymap. */
static void menu_in_terminal_setting(void)
{
    char out[96];
    int text = -1, style;
    /* windowed terminal: the menu-in-terminal setting, whatever the general style */
    CHECK_INT(cx_menu_style(UC_MENU_GRAPHICAL, UC_TERMMENU_TEXT, 1, 0), UC_MENU_TEXT);
    CHECK_INT(cx_menu_style(UC_MENU_TEXT, UC_TERMMENU_GRAPHICAL, 1, 0), UC_MENU_GRAPHICAL);
    /* a full-screen terminal always gets the text menu */
    CHECK_INT(cx_menu_style(UC_MENU_GRAPHICAL, UC_TERMMENU_GRAPHICAL, 1, 1), UC_MENU_TEXT);
    /* outside a terminal the general setting stands */
    CHECK_INT(cx_menu_style(UC_MENU_GRAPHICAL, UC_TERMMENU_TEXT, 0, 0), UC_MENU_GRAPHICAL);
    CHECK_INT(cx_menu_style(UC_MENU_MINIMAL, UC_TERMMENU_TEXT, 0, 1), UC_MENU_MINIMAL);
    /* through the command builder: the program that would run */
    style = cx_menu_style(UC_MENU_GRAPHICAL, UC_TERMMENU_TEXT, 1, 0);
    CHECK(cx_menu_cmd(out, sizeof(out), "UP-Term:bin", KA_MENU, "", style, &text));
    CHECK_STR(out, "\"UP-Term:bin/amarchy-menu\" menu root");
    CHECK_INT(text, 1);
    /* the setting's value parser: text unless graphical */
    CHECK_INT(upconf_term_menu_value(0), UC_TERMMENU_TEXT);
    CHECK_INT(upconf_term_menu_value("bogus"), UC_TERMMENU_TEXT);
    CHECK_INT(upconf_term_menu_value("Graphical"), UC_TERMMENU_GRAPHICAL);
    /* the other-style actions no longer parse, and the shipped keymap has no row for them */
    CHECK(!parse("[bind]\nsuper+alt+space = othermenu:root\n") || km.skipped == 1);
    CHECK(!parse("[bind]\nsuper+alt+k = otherviewer\n") || km.skipped == 1);
    {
        static char file[32768];
        long n = slurp("dist/keymap", file, sizeof(file));
        CHECK(n > 0);
        CHECK(strstr(file, "othermenu") == 0);
        CHECK(strstr(file, "otherviewer") == 0);
    }
}

static void cx_descriptions_and_qualifier_mask(void)
{
    char buf[32768];
    cx_ixspec_t x;
    const km_bind *b;
    long n;
    int i;
    CHECK(parse("[bind]\nsuper+k = viewer\nsuper+shift+space = menu:apps\n"
                "super+alt+t = run:C:Echo hi\n"));
    /* the filter description is a string ParseIX accepts: class and Left Amiga, never a number
     * (the rig: "rawkey lcommand 0x27" was refused, rows=0) */
    CHECK_STR(CX_FILTER_DESC, "rawkey lcommand");
    b = keymap_find(&km, KM_Q_SUPER, RAW_K);
    CHECK(b != 0);
    cx_ixspec(b, &x);
    CHECK_INT(x.code, RAW_K);
    CHECK_INT(x.code_mask, 0xffff);
    CHECK_INT(x.qual, 0x0040);
    CHECK_INT(x.qual_mask, 0x0040);
    cx_filter_models_matchix();
    cx_gate_takes_only_its_own_row();
    cx_helper_path();
    menu_style_commands();
    menu_style_parse();
    menu_in_terminal_setting();
    /* the mask is the handler's: Left Amiga needed; Ctrl, Right Amiga, repeat refuse */
    CHECK_INT((int)cx_qual_mask(0x0040), KM_Q_SUPER);
    CHECK_INT((int)cx_qual_mask(0x0041), KM_Q_SUPER | KM_Q_SHIFT);
    CHECK_INT((int)cx_qual_mask(0x0042), KM_Q_SUPER | KM_Q_RSHIFT);
    CHECK_INT((int)cx_qual_mask(0x0043), KM_Q_SUPER | KM_Q_SHIFT);
    CHECK_INT((int)cx_qual_mask(0x0050), KM_Q_SUPER | KM_Q_ALT);
    CHECK_INT((int)cx_qual_mask(0x0060), KM_Q_SUPER | KM_Q_ALT);
    CHECK_INT((int)cx_qual_mask(0x0001), 0);
    CHECK_INT((int)cx_qual_mask(0x0048), 0);
    CHECK_INT((int)cx_qual_mask(0x00c0), 0);
    CHECK_INT((int)cx_qual_mask(0x0240), 0);
    /* every row of the shipped file carries its own raw code into the filter */
    n = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nbind; i++) {
        cx_ixspec(&km.bind[i], &x);
        CHECK_INT(x.code, km.bind[i].code);
        CHECK_INT(x.qual, 0x0040);
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
        CHECK(cx_row_installed(&km.bind[i]) == (sc != KS_TERMINAL));
        if (sc == KS_TERMINAL)
            term++;
        else
            comm++;
    }
    CHECK(term > 0 && comm > 0);   /* the shipped file has both, so the skip is exercised */
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

/* Phase D: typing "th" at the menu's top level leaves Theme (prefix) before any substring hit */
static void typing_th_leaves_theme_first(void)
{
    mn_item it[MN_MAX_ITEMS];
    int idx[MN_MAX_ITEMS], n, c;
    CHECK(parse("[menu]\nstyle.bg = Bathtub | slash:/bg\nstyle.font = Font | slash:/font\nstyle.theme = Theme | slash:/theme\n"
                "style.other = Another | slash:/x\n"));
    n = mn_children(&km, "style", it, MN_MAX_ITEMS);
    CHECK_INT(n, 4);
    c = mn_filter(it, n, "th", idx, MN_MAX_ITEMS);
    CHECK_INT(c, 3);                               /* Theme, then Bathtub and Another (both contain "th") */
    CHECK_STR(it[idx[0]].label, "Theme");          /* prefix first */
    CHECK_STR(it[idx[1]].label, "Bathtub");
    CHECK_INT(mn_match("Theme", "TH"), 2);         /* case is ignored */
    CHECK_INT(mn_match("Bathtub", "th"), 1);       /* substring */
    CHECK_INT(mn_match("Font", "th"), 0);
    CHECK_INT(mn_match("Font", ""), 2);
}

/* TERM-ROWS-ONLY-OVER-TERMINAL: the shipped [menu] tree, with and without an UP-Term window under the
 * menu. Style and Window (tab:, menuid:, slash: rows) and their sections go; Apps stays, and the
 * Claude row (menuid:) goes with them. */
static int has_label(const mn_item *it, int n, const char *label)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].label, label))
            return 1;
    return 0;
}


/* T9: Style > Theme lists the GUI theme rows, each path "gtheme:<name>"; a gtheme: bind is a commodity-scope
 * action with a plain name, and lf_plan runs it as LF_GTHEME */
static void style_theme_lists_the_gui_theme_rows(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS];
    ix_index x;
    const char *labels[2] = { "nord (current)", "solo" };
    const char *names[2] = { "nord", "solo" };
    long n = slurp("dist/keymap", file, sizeof(file));
    int c;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    CHECK(ix_add_gthemes(&x, labels, names, 2));
    c = ix_level_t(&km, &x, "style.theme", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 2);
    CHECK_STR(it[0].label, "nord (current)");
    CHECK_INT(it[0].row, MN_ROW_INDEX);
    CHECK_STR(x.rows[it[0].ix].path, "gtheme:nord");
    CHECK_STR(x.rows[it[1].ix].path, "gtheme:solo");
    {   /* AM-GTHEME-ICON-REQ: the drawn menu asks ix_icon_path for the row's .info; "gtheme:nord" is no file and
         * DOS answered it with the requester "Please insert volume gtheme:" */
        char ip[IX_PATH];
        CHECK_INT(ix_icon_path(&x, &it[0], ip, sizeof ip), 0);
        CHECK_STR(ip, "");
        CHECK_INT(ix_launch_path(&x, &it[0], ip, sizeof ip), 1);     /* launching is unchanged */
        CHECK_STR(ip, "gtheme:nord");
    }
    CHECK_INT(x.rows[it[1].ix].kind, IX_KIND_GTHEME);
    c = ix_level_t(&km, &x, "style", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 2);                                 /* Amarchy theme (the GUI themes) and the Terminal group */
    CHECK_STR(it[0].label, "Amarchy theme");
    CHECK_INT(it[0].row, MN_ROW_GROUP);
    ix_free(&x);
    CHECK_INT(lf_plan(KA_GTHEME), LF_GTHEME);
    CHECK_INT(keymap_action_scope(KA_GTHEME), KS_COMMODITY);
}
/* Packages group (plan 2026-10-27-amarchy-packages-group.md PG.2, PG.3). The fake loader stands for pk_dos_load_rows:
 * it counts its runs and hands a fixed `upkg list --tsv` text to the index. */
static const char pkg_tsv[] =
    "tiny\t1.0\t1\t0\t\tutils\tUP-Term:uhc/ExtBin/Tiny\tuhc\tA tiny tool\n"
    "Gamma\t2.1\t1\t1\t12345\tgames\tUP-Term:bin/gamma-go\tupkg\tGamma game\n"
    "perl\t5.8\t1\t0\t900000\tdevel\t\tupkg\tScripting language\n"
    "notyet\t1.0\t0\t0\t10\tGames\t\tuhc\tNot installed\n";
static int pkg_fake_loads;
static int pkg_pub_n;
static char pkg_pub_name[2][24];
static long pkg_pub_val[2];
static void pkg_pub(const char *name, long n)
{
    if (pkg_pub_n < 2) {
        strncpy(pkg_pub_name[pkg_pub_n], name, 23);
        pkg_pub_val[pkg_pub_n++] = n;
    }
}
static void pkg_fake_load(ix_index *x)
{
    pk_rows l;
    memset(&l, 0, sizeof l);
    CHECK(pk_parse_all(pkg_tsv, (long)strlen(pkg_tsv), &l) > 0);
    ix_add_pkg_rows(x, &l);
    pkg_fake_loads++;
}
/* what the menus do on every node change (gui.c load_due, menu.c): load when due */
static void pkg_enter(ix_index *x, const char *node)
{
    if (ix_pkgs_due(x, node))
        pkg_fake_load(x);
}
static int pkg_level(const ix_index *x, const char *node, mn_item *it)
{
    return ix_level_t(&km, x, node, "", it, ix_level_cap(x), 0);
}
static int pkg_has(const mn_item *it, int c, const char *label)
{
    int i;
    for (i = 0; i < c; i++)
        if (!strcmp(it[i].label, label))
            return 1;
    return 0;
}
static void packages_group_lists_its_rows_and_drops_the_old_node(void)
{
    static char file[32768], icons[16384];
    mn_item it[MN_MAX_ITEMS];
    long n = slurp("dist/keymap", file, sizeof(file)), ni = slurp("dist/menu-icons", icons, sizeof(icons));
    static const char *const nodes[] = { "packages", "packages.browse", "packages.search", "packages.installed",
                                         "packages.upgrades", "packages.upgrades.all", "packages.refresh" };
    int c, i;
    CHECK(n > 0 && ni > 0 && keymap_parse(&km, file, n));
    c = ix_level_t(&km, 0, "packages", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 5);                                /* Browse, Search, Installed, Upgrades, Refresh */
    CHECK_STR(it[0].label, "Browse");
    CHECK_STR(it[1].label, "Search");
    CHECK_STR(it[2].label, "Installed");
    CHECK_STR(it[3].label, "Upgrades");
    CHECK_STR(it[4].label, "Refresh");
    CHECK(MN_IS_GROUP(&it[3]));
    c = ix_level_t(&km, 0, "packages.upgrades", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 1);
    CHECK_STR(it[0].label, "Upgrade all");
    CHECK_INT(km.menu[it[0].row].kind, KA_JOB);
    c = ix_level_t(&km, 0, "install", "", it, MN_MAX_ITEMS, 0);
    CHECK(!pkg_has(it, c, "Packages"));             /* install.packages is gone (Installed lives in the group) */
    for (i = 0; i < km.nmenu; i++)
        CHECK(strcmp(km.menu[i].id, "install.packages"));
    for (i = 0; i < (int)(sizeof nodes / sizeof *nodes); i++) {      /* every node has an icon line */
        const char *p = icons;
        int hit = 0, len = (int)strlen(nodes[i]);
        while ((p = strstr(p, nodes[i])) != 0) {
            if ((p == icons || p[-1] == '\n') && (p[len] == ' ' || p[len] == '='))
                hit = 1;
            p += len;
        }
        CHECK(hit);
    }
}
static void a_picked_package_offers_only_its_fitting_actions(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS + 64], a[8];
    ix_index x;
    char line[96];
    long n = slurp("dist/keymap", file, sizeof(file));
    int c, ca;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    pkg_fake_loads = 0;
    pkg_enter(&x, "packages.browse");
    c = pkg_level(&x, "packages.browse", it);       /* the sections, one group each, sorted without case */
    CHECK_INT(c, 3);
    CHECK_STR(it[0].label, "devel");
    CHECK_STR(it[1].label, "games");
    CHECK_STR(it[2].label, "utils");
    c = pkg_level(&x, "packages.browse.games", it); /* "games" and "Games" are one section */
    CHECK_INT(c, 2);
    CHECK_INT(it[0].row, MN_ROW_GROUP);
    CHECK(strstr(it[0].label, "[upkg] Gamma  2.1  Upgrade to 2.1") != 0);
    ca = pkg_level(&x, "packages.browse.games.notyet", a);     /* not installed: Install only */
    CHECK_INT(ca, 1);
    CHECK_INT(a[0].row, MN_ROW_PKGACT);
    CHECK_STR(a[0].label, "Install");
    CHECK_INT(a[0].seg[0], PK_ACT_INSTALL);
    CHECK(pk_action_line(&x.pk.r[a[0].ix], a[0].seg[0], line, sizeof line));
    CHECK_STR(line, "upkg install uhc:notyet");
    ca = pkg_level(&x, "packages.installed.Gamma", a); /* installed, upgradable, has a launch: Remove, Launch, Upgrade */
    CHECK_INT(ca, 3);
    CHECK_STR(a[0].label, "Remove");
    CHECK_STR(a[1].label, "Launch");
    CHECK_STR(a[2].label, "Upgrade");
    ca = pkg_level(&x, "packages.installed.perl", a);          /* installed, empty launch, not upgradable: Remove only */
    CHECK_INT(ca, 1);
    CHECK_STR(a[0].label, "Remove");
    ca = pkg_level(&x, "packages.installed.tiny", a);          /* launch set, not upgradable: Remove, Launch */
    CHECK_INT(ca, 2);
    CHECK_STR(a[1].label, "Launch");
    ix_free(&x);
}
/* The warm menu server resets the index at every open (ix_free, then the loaders): the Packages cache must survive
 * that, so upkg runs once per boot, again after a package job (stale) and again when upkg's status file changed. */
static void the_package_cache_survives_the_per_open_index_reset(void)
{
    ix_index x;
    int open;
    memset(&x, 0, sizeof x);
    pkg_fake_loads = 0;
    for (open = 0; open < 3; open++) {              /* three opens, the per-open reset between them */
        ix_free(&x);
        ix_pkg_check_stamp(&x, 100);
        pkg_enter(&x, "packages");
    }
    CHECK_INT(pkg_fake_loads, 1);
    CHECK_INT(x.pk_loads, 1);
    CHECK(x.pk.n > 0);
    ix_free(&x);
    ix_pkg_mark_stale(&x);                          /* a package job ended in the last open */
    pkg_enter(&x, "packages");
    CHECK_INT(pkg_fake_loads, 2);
    ix_free(&x);
    ix_pkg_check_stamp(&x, 100);                    /* status file unchanged: no load */
    pkg_enter(&x, "packages");
    CHECK_INT(pkg_fake_loads, 2);
    ix_free(&x);
    ix_pkg_check_stamp(&x, 250);                    /* upkg ran from a shell: status file changed */
    pkg_enter(&x, "packages");
    CHECK_INT(pkg_fake_loads, 3);
    CHECK_INT(x.pk_loads, 3);
    ix_pkg_check_stamp(&x, 250);
    pkg_enter(&x, "packages");
    CHECK_INT(pkg_fake_loads, 3);
    ix_pkg_free(&x);
}
static void the_package_list_loads_once_and_reloads_when_stale(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS + 64];
    ix_index x;
    long n = slurp("dist/keymap", file, sizeof(file));
    int c;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    pkg_fake_loads = 0;
    CHECK(!ix_pkgs_due(&x, "install"));             /* upkg runs only when the menu enters a packages node */
    CHECK(!ix_pkgs_due(&x, "packagesx"));
    CHECK(ix_pkgs_due(&x, "packages"));
    pkg_enter(&x, "packages");                      /* first entry loads */
    CHECK_INT(pkg_fake_loads, 1);
    pkg_enter(&x, "packages.browse");               /* later entries, any node of the group: no load */
    pkg_enter(&x, "packages");
    pkg_enter(&x, "packages.installed");
    CHECK_INT(pkg_fake_loads, 1);
    CHECK_INT(x.pk_loads, 1);
    pkg_pub_n = 0;
    ix_pkgs_publish(&x, pkg_pub);                   /* the sentinels carry the fake load's rows and load count */
    CHECK_INT(pkg_pub_n, 2);
    CHECK_STR(pkg_pub_name[0], "AMARCHY_PKGS_ROWS");
    CHECK_INT(pkg_pub_val[0], x.pk.n);
    CHECK(x.pk.n > 0);
    CHECK_STR(pkg_pub_name[1], "AMARCHY_PKGS_LOADS");
    CHECK_INT(pkg_pub_val[1], 1);
    c = pkg_level(&x, "packages.installed", it);    /* Installed and Upgrades are filters of that one load */
    CHECK_INT(c, 3);
    CHECK(pkg_has(it, c, "[upkg] perl  5.8  Installed"));
    c = pkg_level(&x, "packages.upgrades", it);
    CHECK_INT(c, 2);                                /* Upgrade all, then the one upgradable row */
    CHECK_STR(it[0].label, "Upgrade all");
    CHECK(strstr(it[1].label, "Gamma") != 0);
    pkg_enter(&x, "packages.upgrades");
    CHECK_INT(pkg_fake_loads, 1);
    ix_pkg_mark_stale(&x);                          /* a package job ended */
    CHECK(ix_pkgs_due(&x, "packages.installed"));
    CHECK(!ix_pkgs_due(&x, "install"));
    pkg_enter(&x, "packages.installed");
    pkg_enter(&x, "packages.browse");
    CHECK_INT(pkg_fake_loads, 2);                   /* reloaded once, not once per node */
    CHECK_INT(x.pk_loads, 2);
    pkg_pub_n = 0;
    ix_pkgs_publish(&x, pkg_pub);
    CHECK_INT(pkg_pub_val[1], 2);                   /* a reload moves the sentinel */
    ix_add_gthemes(&x, 0, 0, 0);                    /* a row adder does not drop the cache */
    CHECK(!ix_pkgs_due(&x, "packages"));
    ix_pkg_query(&x, "TINY");                       /* Search is a third filter of the same load */
    c = pkg_level(&x, "packages.search", it);
    CHECK_INT(c, 1);
    CHECK(strstr(it[0].label, "tiny") != 0);
    CHECK_INT(pkg_fake_loads, 2);
    ix_free(&x);
    CHECK(!ix_pkgs_due(&x, "packages"));            /* the per-open reset keeps the cache */
    ix_pkg_free(&x);
    CHECK(ix_pkgs_due(&x, "packages"));             /* the process end drops it: a freed cache loads again */
}
static void a_failed_upkg_gives_one_inert_row(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS + 8];
    ix_index x;
    pk_rows none;
    long n = slurp("dist/keymap", file, sizeof(file));
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    memset(&none, 0, sizeof none);
    ix_add_pkg_rows(&x, &none);                     /* the loader's answer when upkg is missing */
    CHECK(!ix_pkgs_due(&x, "packages"));            /* loaded: upkg is not rerun on every open */
    CHECK_INT(pkg_level(&x, "packages.browse", it), 1);
    CHECK_INT(it[0].row, MN_ROW_PKGACT);
    CHECK_INT(it[0].ix, -1);
    CHECK(strstr(it[0].label, "upkg") != 0);
    ix_free(&x);
}
/* PG.4, PG.5: Search asks through lf_ui.ask_value; Remove asks first; package jobs mark the list stale */
static const char *pkg_ask_text;
static int pkg_ask(void *ctx, const char *label, const char *usage, char *out, int n)
{
    (void)ctx; (void)usage;
    CHECK_STR(label, "Search packages");
    if (!pkg_ask_text)
        return 0;                                   /* Escape */
    strncpy(out, pkg_ask_text, (size_t)n - 1);
    out[n - 1] = 0;
    return 1;
}
static void package_search_asks_its_text_and_lists_the_matches(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS + 64], g;
    ix_index x;
    lf_ui ui;
    long n = slurp("dist/keymap", file, sizeof(file));
    int c, i;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    memset(&ui, 0, sizeof ui);
    pkg_fake_loads = 0;
    pkg_enter(&x, "packages");
    c = pkg_level(&x, "packages", it);
    CHECK_INT(c, 5);
    memset(&g, 0, sizeof g);
    for (i = 0; i < c; i++)
        if (lf_pkg_search_row("packages", &it[i]))
            g = it[i];
    CHECK_STR(g.seg, "search");                     /* exactly the Search group row, only at the Packages node */
    CHECK(!lf_pkg_search_row("packages.browse", &g));
    CHECK(!lf_pkg_search_row("packages", &it[c - 1]) || !strcmp(it[c - 1].seg, "search"));
    CHECK(!lf_pkg_search(&x, 0));                   /* no ui, no ask_value: nothing */
    ui.ask_value = pkg_ask;
    pkg_ask_text = 0;                               /* Escape: no query, the Search node stays empty */
    CHECK(!lf_pkg_search(&x, &ui));
    CHECK_INT(pkg_level(&x, "packages.search", it), 1);       /* the one inert note row, no package */
    CHECK_INT(it[0].ix, -1);
    pkg_ask_text = "";                              /* an empty line is the same */
    CHECK(!lf_pkg_search(&x, &ui));
    CHECK_INT(pkg_level(&x, "packages.search", it), 1);
    CHECK_INT(it[0].ix, -1);
    pkg_ask_text = "TINY";
    CHECK(lf_pkg_search(&x, &ui));
    c = pkg_level(&x, "packages.search", it);
    CHECK_INT(c, 1);
    CHECK(strstr(it[0].label, "tiny") != 0);
    CHECK_INT(pkg_fake_loads, 1);                   /* it filters the one loaded list */
    ix_free(&x);
}
static void remove_asks_first_and_a_package_job_marks_the_list_stale(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS + 64], a[8];
    char line[128];
    ix_index x;
    long n = slurp("dist/keymap", file, sizeof(file));
    int c, ca, i;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    memset(&x, 0, sizeof x);
    pkg_enter(&x, "packages");
    c = pkg_level(&x, "packages.upgrades", it);     /* Upgrade all is the first row, a job: line */
    CHECK_STR(it[0].label, "Upgrade all");
    CHECK_INT(km.menu[it[0].row].kind, KA_JOB);
    CHECK_STR(km.menu[it[0].row].arg, "upkg upgrade");
    ca = pkg_level(&x, "packages.installed.Gamma", a);
    CHECK_STR(a[0].label, "Remove");
    CHECK_INT(a[0].row, MN_ROW_GROUP);              /* Remove opens a confirm page, it does not run */
    CHECK_STR(a[0].seg, PK_CONFIRM_SEG);
    CHECK_INT(a[1].row, MN_ROW_PKGACT);
    for (i = 0; i < ca; i++)
        if (a[i].row == MN_ROW_PKGACT && a[i].seg[0] != PK_ACT_LAUNCH) {
            CHECK(pk_action_line(&x.pk.r[a[i].ix], a[i].seg[0], line, sizeof line));
            CHECK(!strncmp(line, "upkg ", 5));       /* Install, Upgrade: always a job line, never a window */
        }
    ca = pkg_level(&x, "packages.installed.Gamma.remove", a);
    CHECK_INT(ca, 2);
    CHECK(strstr(a[0].label, "No") != 0 && strstr(a[1].label, "Yes") != 0);   /* owner decision 2026-10-27: No first */
    CHECK_INT(a[1].row, MN_ROW_PKGACT);
    CHECK_INT(a[1].seg[0], PK_ACT_REMOVE);
    CHECK(pk_action_line(&x.pk.r[a[1].ix], PK_ACT_REMOVE, line, sizeof line));
    CHECK_STR(line, "upkg remove Gamma");
    CHECK_INT(a[0].seg[0], PK_ACT_NO);              /* No: leaf.c returns before any job */
    CHECK_INT(lf_pkg_back_levels(&a[0]), 2);        /* No: confirm page and action page, back at the package row */
    CHECK_INT(lf_pkg_back_levels(&a[1]), 2);
    CHECK_INT(lf_pkg_back_levels(0), 0);
    ca = pkg_level(&x, "packages.browse.games.notyet", a);
    CHECK_INT(lf_pkg_back_levels(&a[0]), 1);        /* Install: back to the list */
    CHECK(!ix_pkgs_due(&x, "packages.installed"));
    lf_job_done(&x, "echo hi", 0);                  /* not a package job: the list stays */
    CHECK(!ix_pkgs_due(&x, "packages.installed"));
    lf_job_done(&x, "upkg install uhc:notyet", 0);
    CHECK(ix_pkgs_due(&x, "packages.installed"));
    pkg_enter(&x, "packages.installed");
    CHECK(!ix_pkgs_due(&x, "packages.installed"));
    lf_job_done(&x, "upkg update", 5);              /* a failed or cancelled job may still have changed packages */
    CHECK(ix_pkgs_due(&x, "packages"));
    pkg_enter(&x, "packages");
    lf_job_done(&x, "upkg upgrade", 12);
    CHECK(ix_pkgs_due(&x, "packages"));
    lf_job_done(0, "upkg upgrade", 0);
    (void)c;
    ix_free(&x);
}
static void terminal_rows_show_only_over_a_terminal(void)
{
    static char file[32768];
    mn_item it[MN_MAX_ITEMS];
    long n = slurp("dist/keymap", file, sizeof(file));
    int c;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    c = ix_level_t(&km, 0, "", "", it, MN_MAX_ITEMS, 0);
    CHECK(has_label(it, c, "Apps"));
    CHECK(has_label(it, c, "Style"));               /* the Theme group lists the GUI themes, over a terminal or not */
    CHECK(has_label(it, c, "Window"));              /* the win: rows run in the commodity, over a terminal or not */
    c = ix_level_t(&km, 0, "style", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 2);                                 /* Amarchy theme and the Terminal group, shown with no terminal open */
    CHECK(has_label(it, c, "Amarchy theme") && has_label(it, c, "Terminal"));
    c = ix_level_t(&km, 0, "style.terminal", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 3);                                 /* Theme, Font, Background: picking one opens a terminal first */
    CHECK(has_label(it, c, "Theme") && has_label(it, c, "Font") && has_label(it, c, "Background"));
    c = ix_level_t(&km, 0, "window", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 32);                                /* the 32 win:/rexx:/run: rows (window.tile runs AmTile, plus the 8 window.layout* rows); window.tab and window.preferences stay hidden */
    c = ix_level_t(&km, 0, "apps", "", it, MN_MAX_ITEMS, 0);
    CHECK(has_label(it, c, "UP-Term") && has_label(it, c, "Editor"));
    CHECK(!has_label(it, c, "Claude") && !has_label(it, c, "pi")); /* menuid rows: the handler runs them, no index entry */
    CHECK_INT(c, mn_children(&km, "apps", it, MN_MAX_ITEMS) - 2);
    c = ix_level_t(&km, 0, "", "", it, MN_MAX_ITEMS, 1);
    CHECK(has_label(it, c, "Apps") && has_label(it, c, "Style") && has_label(it, c, "Window"));
    c = ix_level_t(&km, 0, "style", "", it, MN_MAX_ITEMS, 1);
    CHECK(has_label(it, c, "Amarchy theme") && has_label(it, c, "Terminal"));
    c = ix_level_t(&km, 0, "style.terminal", "", it, MN_MAX_ITEMS, 1);
    CHECK(has_label(it, c, "Theme") && has_label(it, c, "Font") && has_label(it, c, "Background"));
    c = ix_level_t(&km, 0, "apps", "", it, MN_MAX_ITEMS, 1);
    CHECK(has_label(it, c, "Claude"));
    CHECK(!mn_row_visible(KA_TAB, 0) && !mn_row_visible(KA_MENUID, 0) && !mn_row_visible(KA_SLASH, 0));
    CHECK(mn_row_visible(KA_RUN, 0) && mn_row_visible(KA_OPEN, 0) && mn_row_visible(KA_DETACH, 0));
    CHECK(mn_row_visible(KA_SLASH, 1));
}

static int fixed_width(const char *s, void *ctx);

/* The window is sized from the rows it draws: a tab:/menuid:/slash: row (and the longest label or the
 * most entries among them) must not widen or heighten a menu shown without a terminal. */
static void menu_size_follows_the_filtered_rows(void)
{
    int cols, rows, cols1, rows1, px = 16, w0, w1;
    mg_font topaz = { 8, 8 };
    mg_screen pal = { 640, 256, 22, 44, 0, 0, 0, 0 };
    CHECK(parse("[menu]\napps.a = Short | run:a\napps.b = Other | run:b\n"
                "apps.long = A very much longer slash label that is wider than the minimum window | slash:/theme\n"
                "apps.t1 = Tab one | tab:new\napps.t2 = Tab two | tab:next\ntop = Top | run:x\n"));
    mn_osd_size_t(&km, "apps", &cols, &rows, 0);
    mn_osd_size_t(&km, "apps", &cols1, &rows1, 1);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 2);        /* Short, Other */
    CHECK_INT(rows1, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 5);
    CHECK_INT(cols, MN_OSD_MIN_COLS);
    CHECK(cols1 > cols);
    mn_osd_size(&km, "apps", &cols, &rows);        /* the unfiltered call is the term = 1 one */
    CHECK(cols == cols1 && rows == rows1);
    w0 = mg_width_t(&km, "apps", &topaz, 0, 8, fixed_width, &px, &pal, 0);
    w1 = mg_width_t(&km, "apps", &topaz, 0, 8, fixed_width, &px, &pal, 1);
    CHECK(w0 < w1);
    CHECK_INT(w1, mg_width(&km, "apps", &topaz, 0, 8, fixed_width, &px, &pal));
}

/* Phase D: the menu tree comes from the [menu] ids; Esc goes up one level and stops at the root */
static void menu_levels_follow_the_ids_and_escape_goes_up(void)
{
    mn_item it[MN_MAX_ITEMS];
    char node[KM_ID], up[KM_ID];
    int n;
    CHECK(parse("[menu]\napps.upkg = Install add-ons | run:upkg-browse\napps.tools.tmux = Tmux | run:tmux\n"
                "apps.tools.nvim = Editor | run:nvim\nstyle.theme = Theme | slash:/theme\ntop = Top | run:x\n"));
    n = mn_children(&km, "", it, MN_MAX_ITEMS);
    CHECK_INT(n, 3);                               /* apps (group), style (group), top (leaf) */
    CHECK_INT(it[0].row, -1);
    CHECK_STR(it[0].label, "Apps");
    CHECK_INT(it[2].row, 4);
    mn_child_node("", &it[0], node, sizeof node);
    CHECK_STR(node, "apps");
    n = mn_children(&km, node, it, MN_MAX_ITEMS);
    CHECK_INT(n, 2);                               /* upkg (leaf), tools (group, once for two rows) */
    CHECK_STR(it[0].label, "Install add-ons");
    CHECK_INT(it[1].row, -1);
    mn_child_node(node, &it[1], node, sizeof node);
    CHECK_STR(node, "apps.tools");
    CHECK_INT(mn_children(&km, node, it, MN_MAX_ITEMS), 2);
    CHECK_INT(mn_parent(node, up, sizeof up), 1);
    CHECK_STR(up, "apps");
    CHECK_INT(mn_parent("apps", up, sizeof up), 1);
    CHECK_STR(up, "");
    CHECK_INT(mn_parent("", up, sizeof up), 0);    /* Esc at the root leaves the menu */
    /* MENU-BACKSPACE: one rule for both menus */
    CHECK_INT(mn_back(1, "ab", "apps"), MN_BACK_DROP_CHAR);
    CHECK_INT(mn_back(1, "ab", ""), MN_BACK_DROP_CHAR);
    CHECK_INT(mn_back(1, "", "apps.tools"), MN_BACK_UP);
    CHECK_INT(mn_back(1, "", ""), MN_BACK_CLOSE);
    CHECK_INT(mn_back(0, "ab", "apps"), MN_BACK_CLEAR_FILTER);
    CHECK_INT(mn_back(0, "", "apps"), MN_BACK_UP);
    CHECK_INT(mn_back(0, "", ""), MN_BACK_CLOSE);
}

/* AM-OSD O5: the commodity sizes the menu window from the keymap it holds, walking every level
 * reachable from the node (the same mn_children walk amarchy-menu uses). The expected numbers are
 * counted by hand from the shipped file below, not from mn_children. */
static void osd_size_counts_the_reachable_levels(void)
{
    int cols, rows;
    /* the padding, measured on the rig (cells are 8 px wide, 16 px tall): 2 columns are 16 px, so 1 blank row
     * (16 px) above the filter line and below the last entry; head = 1 + filter + gap, 3 entries = 3 + 3 + 1 */
    CHECK(parse("[menu]\na.x = X | run:x\na.y = Y | run:y\na.z = Z | run:z\n"));
    mn_osd_size(&km, "a", &cols, &rows);
    CHECK_INT(rows, 7);
    CHECK(parse("[menu]\napps.upkg = Install add-ons | run:upkg-browse\napps.tools.tmux = Tmux | run:tmux\n"
                "apps.tools.nvim = Editor | run:nvim\napps.tools.zzz = A much longer label here | run:z\n"
                "style.theme = Theme | slash:/theme\ntop = Top | run:x\n"));
    mn_osd_size(&km, "apps", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 3);        /* apps has 2 entries, apps.tools has 3 */
    CHECK_INT(cols, 24 + 2 + 2 * MN_OSD_PAD);      /* "A much longer label here" + 2 spaces */
    mn_osd_size(&km, "", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 3);        /* root has 3 (apps, style, top) */
    CHECK_INT(cols, 24 + 2 + 2 * MN_OSD_PAD);      /* reaches the long label two levels down */
    mn_osd_size(&km, "style", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 1);
    CHECK_INT(cols, MN_OSD_MIN_COLS);              /* "Theme" + pad is shorter than the minimum */
    /* the shipped dist/keymap: the sizes the commodity opens Super+Space and Super+Shift+Space with
     * (9 entries in the root, 4 in apps; the tallest level is Window, and the widest label is the Dopus group's
     * "Configure lister display format" (31 columns + 2 + 2 pads)) */
    {
        static char file[32768];
        long n = slurp("dist/keymap", file, sizeof(file));
        CHECK(n > 0 && keymap_parse(&km, file, n));
        mn_osd_size(&km, "", &cols, &rows);
        CHECK_INT(cols, 31 + 2 + 2 * MN_OSD_PAD);       /* "Configure lister display format" in Dopus */
        CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + MN_OSD_MAX_ENTRIES); /* Workbench has 42 rows: the window caps them, the list scrolls (owner 2026-10-26: taller than needed) */
        mn_osd_size(&km, "apps", &cols, &rows);
        CHECK_INT(cols, MN_OSD_MIN_COLS);
        CHECK_INT(rows, MN_OSD_HEAD_LINES + MN_OSD_BOTTOM_LINES + 5);
    }
}

/* The white lines of 2026-10-25: on the A1200's 8-plane AGA Workbench a 30-line row blit took about 150 beam
 * lines of a 313-line frame. The copy is one blit per plane, so while it runs every line of the row mixes new
 * and old planes: the beam must be off the row when the copy starts and must not reach the row's top before it
 * is done. Numbers from that machine: the row on beam lines 151-180. The beam is back on the row's top d lines
 * after the start. */
static void blit_starts_where_the_beam_cannot_cross_it(void)
{
    /* 51 lines above the row: the beam meets the copy a third of the way */
    CHECK_INT(mg_blit_safe(100, 151, 30, 313, 150), 0);
    /* 120 lines above: by the mean speed the beam meets the copy as it ends, a tear when the copy slows down
     * on the display's lines (the A1200 still showed white lines with such starts) */
    CHECK_INT(mg_blit_safe(31, 151, 30, 313, 150), 0);
    /* 150 lines above: done as the beam arrives */
    CHECK_INT(mg_blit_safe(1, 151, 30, 313, 150), 1);
    CHECK_INT(mg_blit_safe(2, 151, 30, 313, 150), 0);
    /* the line before the top, and the beam in the row: the lines below it show planes half copied (the white
     * lines the owner saw at every step after 3734d9a2 planned such starts as safe) */
    CHECK_INT(mg_blit_safe(150, 151, 30, 313, 150), 0);
    CHECK_INT(mg_blit_safe(151, 151, 30, 313, 150), 0);
    CHECK_INT(mg_blit_safe(170, 151, 30, 313, 150), 0);
    CHECK_INT(mg_blit_safe(180, 151, 30, 313, 150), 0);
    /* just past the row, the frame wrapping: 282 lines until the top again */
    CHECK_INT(mg_blit_safe(181, 151, 30, 313, 150), 1);
    CHECK_INT(mg_blit_safe(182, 151, 30, 313, 150), 1);
    CHECK_INT(mg_blit_safe(300, 151, 30, 313, 150), 1);
    /* a row near the top, the beam late in the frame: 30 lines to go, it wraps onto the copy */
    CHECK_INT(mg_blit_safe(290, 7, 30, 313, 150), 0);
    /* a copy faster than the beam is no exception: its planes mix like the slow one's while it runs */
    CHECK_INT(mg_blit_safe(120, 151, 30, 313, 20), 1);
    CHECK_INT(mg_blit_safe(140, 151, 30, 313, 20), 0);
    CHECK_INT(mg_blit_safe(160, 151, 30, 313, 20), 0);
    /* not measured: planned as half a frame, so 11 lines above the row waits */
    CHECK_INT(mg_blit_safe(140, 151, 30, 313, 0), 0);
    CHECK_INT(mg_blit_safe(300, 151, 30, 313, 0), 1);
    /* no clear start in a frame (the copy longer than the beam's time off the row): at once */
    CHECK_INT(mg_blit_safe(140, 151, 30, 313, 284), 1);
    CHECK_INT(mg_blit_safe(140, 151, 30, 313, 400), 1);
}

/* The A1200 again: the full window at the menu's open (176 lines) took 308 beam lines of 313, so no start kept
 * the beam off it and every open showed a torn line (the grab of 2026-10-25 19:37). A box taller than a row is
 * copied a row at a time; a row copies whole. */
static void a_tall_copy_goes_a_row_at_a_time(void)
{
    int s0 = -1, s1 = -1;
    CHECK_INT(mg_blit_strip(0, 175, 30, 0, &s0, &s1), 1);
    CHECK_INT(s0, 0);
    CHECK_INT(s1, 29);
    CHECK_INT(mg_blit_strip(0, 175, 30, 5, &s0, &s1), 1);
    CHECK_INT(s0, 150);
    CHECK_INT(s1, 175);
    CHECK_INT(mg_blit_strip(0, 175, 30, 6, &s0, &s1), 0);
    CHECK_INT(mg_blit_strip(40, 69, 30, 0, &s0, &s1), 1);
    CHECK_INT(s0, 40);
    CHECK_INT(s1, 69);
    CHECK_INT(mg_blit_strip(40, 69, 30, 1, &s0, &s1), 0);
}

static void a_slow_blit_keeps_the_estimate_up_so_the_bar_does_not_tear(void)
{
    long est = 0;
    int i;
    /* the first copy sets it, a slower one raises it at once */
    est = mg_blit_rate(est, 300000);
    CHECK_INT((int)est, 300000);
    est = mg_blit_rate(est, 400000);
    CHECK_INT((int)est, 400000);
    /* a copy in the vertical blank is faster: the next start is still planned for the slow one */
    est = mg_blit_rate(est, 240000);
    CHECK_INT((int)est, 390000);
    /* the slow 30-line row (150 lines) after a fast one (100): planned with the fast rate, a start 110 lines
     * above the row passes, and the beam crosses the slow copy; with the kept rate it waits */
    CHECK_INT(mg_blit_safe(41, 151, 30, 313, 100), 1);
    CHECK_INT(mg_blit_safe(41, 151, 30, 313, (int)(mg_blit_rate(150, 100))), 0);
    /* faster copies bring it down step by step */
    est = 160000;
    for (i = 0; i < 64; i++)
        est = mg_blit_rate(est, 100000);
    CHECK_INT(est < 105000 && est >= 100000, 1);
}

/* AM-GUI G1, aspect plan 2026-10-23-amarchy-menu-aspect.md: the drawn menu's rectangles. The text
 * width is a fixed number of pixels per character (ctx), as topaz's is; the expected numbers are worked
 * out by hand from the rule in menu_geom.h, not from menu_geom.c: a row is R = ysize * 15 / 4 (topaz 8:
 * 30, TopazPro 16: 60); Omarchy's px become V(px) = (px * R + 25) / 50 down and, across,
 * mg_cols(V(px)) = (V * ry + rx / 2) / rx (on 640x256 hires rx 22, ry 44: twice V). */
static int fixed_width(const char *s, void *ctx)
{
    return (int)strlen(s) * *(int *)ctx;
}

static void drawn_menu_geometry_follows_the_font_and_screen(void)
{
    static char file[32768];
    mg_font topaz = { 8, 8 }, pro = { 16, 8 };
    mg_screen pal = { 640, 256, 22, 44, 0, 0, 0, 0 }, narrow = { 320, 256, 22, 44, 0, 0, 0, 0 }, sq = { 640, 512, 22, 22, 0, 0, 0, 0 };
    int px8 = 8, px16 = 16, w;
    char head[KM_ID];
    mn_item items[MN_MAX_ITEMS];
    mg_geom g;
    long n = slurp("dist/keymap", file, sizeof(file));

    /* topaz 8 on 640x256 hires: R 30, row 16 + 2 * V(5) 3 = 22; V(2) 2, V(18) 11, V(34) 20, V(6) 4, V(3) 2, V(12) 7, V(10) 6,
     * V(300) 180; across twice those: 4, 22, 14, 12, 360 */
    mg_metrics_for(&g.m, &topaz, &pal);
    CHECK_INT(g.m.bx, 4);
    CHECK_INT(g.m.by, 2);
    CHECK_INT(g.m.hpad, 22);
    CHECK_INT(g.m.vpad, 11);
    CHECK_INT(g.m.head_h, 20);
    CHECK_INT(g.m.gap, 4);
    CHECK_INT(g.m.row_h, 22);
    CHECK_INT(g.m.row_gap, 2);
    CHECK_INT(g.m.pitch, 24);
    CHECK_INT(g.m.row_pad, 14);
    CHECK_INT(g.m.icon_gap, 12);
    CHECK_INT(g.m.chev_gap, 14);
    CHECK_INT(g.m.min_w, 360);
    CHECK_INT(g.m.margin_x, 12);
    CHECK_INT(g.m.margin_y, 6);

    /* TopazPro 16 on 640x512 square pixels: R 60, row 32 + 2 * V(5) 6 = 44; V(2) 2, V(18) 22, V(34) 41, V(6) 7, V(3) 4, V(12) 14,
     * V(10) 12, V(300) 360; across the same */
    mg_metrics_for(&g.m, &pro, &sq);
    CHECK_INT(g.m.bx, 2);
    CHECK_INT(g.m.by, 2);
    CHECK_INT(g.m.hpad, 22);
    CHECK_INT(g.m.vpad, 22);
    CHECK_INT(g.m.head_h, 41);
    CHECK_INT(g.m.gap, 7);
    CHECK_INT(g.m.row_h, 44);
    CHECK_INT(g.m.row_gap, 4);
    CHECK_INT(g.m.pitch, 48);
    CHECK_INT(g.m.row_pad, 14);
    CHECK_INT(g.m.icon_gap, 12);
    CHECK_INT(g.m.min_w, 360);
    CHECK_INT(g.m.margin_x, 12);
    CHECK_INT(g.m.margin_y, 12);

    mg_header("", head, sizeof head);
    CHECK_STR(head, "Amarchy");
    mg_header("apps", head, sizeof head);
    CHECK_STR(head, "Apps");
    mg_header("apps.tools", head, sizeof head);
    CHECK_STR(head, "Tools");

    /* the shipped keymap at topaz 8 on 640x256: root 3 groups, apps 6 rows; the widest label of all
     * levels is "Install add-ons" (15 x 8 px): 2 x 4 + 2 x 22 + 2 x 14 + 120 = 200, under min_w 360 */
    CHECK(n > 0 && keymap_parse(&km, file, n));
    CHECK_INT(mn_children(&km, "", items, MN_MAX_ITEMS), 13);   /* with the Trigger, Settings, Dopus, Workbench, Screens, Packages and Agents groups (all shown) */
    CHECK_INT(mn_children(&km, "apps", items, MN_MAX_ITEMS), 5);
    w = mg_width(&km, "", &topaz, 0, 8, fixed_width, &px8, &pal);
    CHECK_INT(w, 360);
    /* 3 rows: chrome 2 x 2 + 2 x 11 + 20 + 4 = 50, rows 3 x 24 - 2 = 70 */
    mg_layout(&g, &topaz, w, 3, &pal);
    CHECK_INT(g.h, 120);
    CHECK_INT(g.x, 140);
    CHECK_INT(g.y, 68);
    CHECK_INT(g.visible, 3);
    CHECK_INT(g.peek, 0);
    CHECK_INT(mg_more(&g, 0), 0);
    CHECK_INT(g.list_x, 26);
    CHECK_INT(g.list_y, 37);
    CHECK_INT(g.list_w, 308);
    /* room = min(256 - 12 - 50, 70 % of 256 = 179) = 179: 7 rows of 24 fit (7 x 24 - 2 = 166), no peek */
    mg_layout(&g, &topaz, w, 7, &pal);
    CHECK_INT(g.h, 216);
    CHECK_INT(g.y, 20);
    CHECK_INT(g.visible, 7);
    CHECK_INT(g.peek, 0);
    /* an eighth entry: the peek (55 % of 22 = 12) of the next leaves room for 6 rows */
    mg_layout(&g, &topaz, w, 8, &pal);
    CHECK_INT(g.visible, 6);
    CHECK_INT(g.peek, 12);

    /* hits: row 0 from y 37 for 22 px, then a 2 px gap, row 1 from 61 */
    mg_layout(&g, &topaz, w, 3, &pal);
    CHECK_INT(mg_hit(&g, 0, 30, 37), 0);
    CHECK_INT(mg_hit(&g, 0, 30, 59), -1);
    CHECK_INT(mg_row_y(&g, 1), 61);
    CHECK_INT(mg_hit(&g, 0, 30, 61), 1);
    CHECK_INT(mg_hit(&g, 0, 25, 37), -1);
    CHECK_INT(mg_hit(&g, 0, 26 + 308, 37), -1);

    /* 11 entries: 6 rows and the peek; the list is 6 x 24 + 12 = 156, the window 50 + 156 */
    mg_layout(&g, &topaz, w, 11, &pal);
    CHECK_INT(g.visible, 6);
    CHECK_INT(g.peek, 12);
    CHECK_INT(g.list_h, 156);
    CHECK_INT(g.h, 206);
    CHECK_INT(mg_more(&g, 0), 1);
    CHECK_INT(mg_more(&g, 5), 0);                  /* at the bottom the cue goes */
    CHECK_INT(mg_scroll(&g, 10, 0), 5);
    CHECK_INT(mg_scroll(&g, 1, 3), 1);
    CHECK_INT(mg_scroll(&g, 2, 1), 1);
    CHECK_INT(mg_hit(&g, 0, 30, 185), 6);          /* the peek row (from 37 + 144) selects the next entry */
    CHECK_INT(mg_hit(&g, 5, 30, 185), -1);         /* no peek drawn at the bottom */

    /* TopazPro 16 on 640x512: chrome 4 + 44 + 41 + 7 = 96; 3 rows 3 x 48 - 4 = 140; 5 rows fit in
     * min(512 - 24 - 96, 358) = 358 */
    w = mg_width(&km, "", &pro, 0, 8, fixed_width, &px8, &sq);
    CHECK_INT(w, 360);
    mg_layout(&g, &pro, w, 3, &sq);
    CHECK_INT(g.h, 236);
    CHECK_INT(g.x, 140);
    CHECK_INT(g.y, 138);
    mg_layout(&g, &pro, w, 5, &sq);
    CHECK_INT(g.h, 332);
    CHECK_INT(g.y, 90);
    CHECK_INT(g.peek, 0);

    /* width: the widest row two levels down sets it ("A much longer label here", 24 x 16 px = 384, plus
     * 2 x 4 + 2 x 22 + 2 x 14 = 80); the screen caps it at its width less 2 margins */
    CHECK(parse("[menu]\napps.upkg = Install add-ons | run:upkg-browse\napps.tools.zzz = A much longer label here | run:z\n"));
    CHECK_INT(mg_width(&km, "apps", &topaz, 0, 8, fixed_width, &px16, &pal), 464);
    CHECK_INT(mg_width(&km, "apps", &topaz, 16, 8, fixed_width, &px16, &pal), 464 + 16 + 12);
    /* any icon width adds itself plus the icon gap (12 here) to every row */
    CHECK_INT(mg_width(&km, "apps", &topaz, 9, 8, fixed_width, &px16, &pal), 464 + 9 + 12);
    CHECK_INT(mg_width(&km, "apps", &topaz, 32, 8, fixed_width, &px16, &pal), 464 + 32 + 12);
    CHECK_INT(mg_width(&km, "apps", &topaz, 0, 8, fixed_width, &px16, &narrow), 320 - 2 * 12);
    CHECK_INT(mg_width(&km, "apps", &topaz, 0, 8, fixed_width, &px8, &pal), 360);   /* 272, min_w */
}

/* The owner's symptom (G8, A1200 640x256 hires): the icons were squashed to half width and the window
 * narrow. A length looks the same across and down: on tall pixels (resolution 22 x 44) every across
 * length is twice its down length, on square ones they are equal. */
static void drawn_menu_icons_look_square_on_tall_pixels(void)
{
    mg_font topaz = { 8, 8 }, pro = { 16, 8 };
    mg_screen pal = { 640, 256, 22, 44, 0, 0, 0, 0 }, sq = { 640, 512, 22, 22, 0, 0, 0, 0 }, unknown = { 640, 256, 0, 0, 0, 0, 0, 0 };
    mg_metrics m;
    mg_metrics_for(&m, &topaz, &pal);
    CHECK_INT(mi_size(&m), 16);                    /* 30 * 8 / 15 */
    CHECK_INT(mi_width(&m, &pal), 32);             /* twice as many columns as lines */
    CHECK_INT(m.hpad, 2 * m.vpad);
    CHECK_INT(m.bx, 2 * m.by);
    CHECK_INT(m.row_pad, 2 * 7);
    mg_metrics_for(&m, &pro, &sq);
    CHECK_INT(mi_size(&m), 32);                    /* 60 * 8 / 15 */
    CHECK_INT(mi_width(&m, &sq), 32);
    CHECK_INT(mg_cols(&pal, 16), 32);
    CHECK_INT(mg_cols(&sq, 16), 16);
    CHECK_INT(mg_cols(&unknown, 16), 16);          /* aspect unknown: square */
}

/* The owner's symptom: the drawn menu's icons looked blocky, doubled pixels. The page held 16x16 glyphs
 * and the 32 px icon of TopazPro 16 was them scaled UP 2x. The source is now the glyph drawn at 32 px:
 * a 32x32 render is the source's coverage 1:1, and the source is not made of 2x2 blocks. */
static void icons_are_not_upscaled_from_16px(void)
{
    static vt_u8 idx[MI_SRC * MI_SRC], cov[MI_SRC * MI_SRC];
    long plen = 0;
    int i, j, x, y, finer = 0;
    CHECK_INT(MI_SRC, 32);
    CHECK(ce_page_check_px(mi_page(&plen), plen, 0xE0, MI_PIXELS));
    for (i = 0; i < mi_count(); i++) {
        const vt_u8 *g = mi_find(mi_id(i));
        int blocks = 1;
        CHECK(g != 0 && mi_source(g, idx) && mi_render(g, 32, 32, cov));
        if (!g)
            continue;
        for (j = 0; j < MI_SRC * MI_SRC; j++)
            if (cov[j] != (idx[j] == CE_BG ? 0 : g[2 + 4 * idx[j] + 3]))
                break;
        CHECK_INT(j, MI_SRC * MI_SRC);                 /* 1:1, no filter at the source's own size */
        for (y = 0; y < MI_SRC; y++)
            for (x = 0; x < MI_SRC; x += 2)
                if (idx[y * MI_SRC + x] != idx[y * MI_SRC + x + 1] ||
                    idx[(y & ~1) * MI_SRC + x] != idx[y * MI_SRC + x])
                    blocks = 0;
        finer += !blocks;
    }
    CHECK_INT(finer, mi_count());                       /* every icon has detail finer than 2x2 */
}

/* G3: every node the shipped [menu] names (each row and each group above it) has an icon image in
 * the compiled page, and the page is a valid emoji page; a missing one fails here */
static void every_shipped_menu_node_has_an_icon_image(void)
{
    static char file[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    long plen = 0;
    const vt_u8 *page = mi_page(&plen);
    mg_metrics m;
    static vt_u8 cov[MI_MAX_W * MI_MAX];
    int i, j, k, s, sum, sum2;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    CHECK(ce_page_check_px(page, plen, 0xE0, MI_PIXELS));
    CHECK(km.nmenu > 0);
    for (i = 0; i < km.nmenu; i++) {
        char node[64];
        const char *id = km.menu[i].id;
        int len = (int)strlen(id);
        for (j = 0; j <= len; j++) {
            if (id[j] != '.' && id[j] != 0)
                continue;
            for (k = 0; k < j; k++)
                node[k] = id[k];
            node[j] = 0;
            if (!mi_find(node))
                printf("  no icon for menu node '%s'\n", node);
            CHECK(mi_find(node) != 0);
        }
    }
    /* each table entry is drawn: its glyph covers some pixels at the icon size */
    memset(&m, 0, sizeof(m));
    m.unit = 14;
    s = mi_size(&m);
    CHECK_INT(s, 8);                               /* 14 * 8 / 15 = 7, 8 at least */
    m.unit = 30;
    CHECK_INT(mi_size(&m), 16);
    m.unit = 60;
    CHECK_INT(mi_size(&m), 32);
    m.unit = 400;
    CHECK_INT(mi_size(&m), MI_MAX);
    for (i = 0; i < mi_count(); i++) {
        const vt_u8 *g = mi_find(mi_id(i));
        CHECK(g != 0);
        if (!g)
            continue;
        CHECK(mi_render(g, 16, 16, cov));
        for (sum = 0, j = 0; j < 16 * 16; j++)
            sum += cov[j];
        /* twice as wide: twice the pixels at the same mean coverage, so about twice the sum */
        CHECK(mi_render(g, 32, 16, cov));
        for (sum2 = 0, j = 0; j < 32 * 16; j++)
            sum2 += cov[j];
        CHECK(sum2 * 10 >= sum * 18 && sum2 * 10 <= sum * 22);
        CHECK(!mi_render(g, MI_MAX_W + 1, 16, cov));
        CHECK(!mi_render(g, 16, MI_MAX + 1, cov));
        if (sum == 0)
            printf("  blank icon '%s'\n", mi_id(i));
        CHECK(sum > 0);
    }
}

/* Phase D: the Super+K viewer prints one line per [bind] row of the shipped file; the expected
 * count is read from the file text (second source), the printed lines are counted from the output */
static void the_key_viewer_lists_every_bind_row(void)
{
    static char file[32768], text[32768];
    char *p;
    long n = slurp("dist/keymap", file, sizeof(file));
    int expect = 0, lines = 0, in_bind = 0, printed;
    char *line;
    CHECK(n > 0);
    for (line = strtok(file, "\n"); line; line = strtok(0, "\n")) {
        while (*line == ' ')
            line++;
        if (*line == '[')
            in_bind = !strncmp(line, "[bind]", 6);
        else if (in_bind && !strncmp(line, "super", 5))
            expect += strstr(line, "1..9") && strstr(line, "=") && strstr(line, "1..9") < strstr(line, "=") ? 9 : 1;
    }
    CHECK(expect > 30);
    n = slurp("dist/keymap", file, sizeof(file));
    CHECK(keymap_parse(&km, file, n));
    CHECK_INT(km.nbind, expect);
    printed = mn_viewer_text(&km, text, sizeof text);
    for (p = text; *p; p++)
        if (*p == '\n')
            lines++;
    CHECK_INT(printed, expect);
    CHECK_INT(lines, expect);
    CHECK(strstr(text, "Super+Space") != 0);
    CHECK(strstr(text, "Super+Shift+Space") != 0);
    CHECK(strstr(text, "Super+K  ") != 0);
    CHECK(strstr(text, "Key bindings") != 0);
    CHECK(strstr(text, "(menu)") != 0);
}

/* Phase D: the viewer follows the keymap: a rebinding moves the printed key, none removes the line */
static void the_key_viewer_follows_a_rebinding(void)
{
    static char text[2048];
    CHECK(parse("[bind]\nsuper+k = viewer ; Key bindings | menu\nsuper+j = slash:/font ; Font | style\n"));
    {
        const char *u = "[bind]\nsuper+k = none\nsuper+shift+j = viewer ; Key bindings | menu\n";
        CHECK(keymap_overlay(&km, u, (long)strlen(u)));
    }
    CHECK_INT(mn_viewer_text(&km, text, sizeof text), 2);
    CHECK(strstr(text, "Super+K ") == 0);
    CHECK(strstr(text, "Super+Shift+J") != 0);
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

/* AMB1 (owner, 2026-10-23): Apps > Editor with no UP-Term running opened nvim inside the menu's
 * own OSD window, because a run: leaf went through system() on the menu's console. A run: leaf
 * is planned as its own UP-Term window; only terminal actions go to a terminal's request port. */
static int job_calls;
static int fake_job(void *ctx, const char *line, const char *title)
{
    (void)ctx; (void)line; (void)title;
    job_calls++;
    return 0;
}

/* RJ.K: job:<line> parses with its line, plans as LF_JOB, and falls back to the run: path while the menu style
 * has no run_job; run: rows stay LF_WINDOW; the shipped upgrade and update rows are job: */
static void a_job_row_parses_and_falls_back_to_run_without_a_job_view(void)
{
    lf_ui ui;
    const km_menu *m;
    memset(&ui, 0, sizeof ui);
    CHECK(parse("[bind]\nsuper+j = job:upkg upgrade ; Upgrade | apps\n[menu]\napps.up = Upgrade | job:upkg upgrade\napps.r = Run | run:upkg browse\n"));
    m = &km.menu[0];
    CHECK(m->kind == KA_JOB && !strcmp(m->arg, "upkg upgrade"));
    CHECK(km.bind[0].kind == KA_JOB && !strcmp(km.bind[0].arg, "upkg upgrade"));
    CHECK_INT(keymap_action_scope(KA_JOB), KS_BOTH);
    CHECK_INT((int)lf_plan(KA_JOB), (int)LF_JOB);
    CHECK_INT((int)lf_plan_t(KA_JOB, 0), (int)LF_JOB);
    CHECK_INT(km.menu[1].kind, KA_RUN);
    CHECK_INT((int)lf_plan(KA_RUN), (int)LF_WINDOW);
    CHECK_INT(lf_job_view(&ui), 0);          /* both styles today: the run: path */
    CHECK_INT(lf_job_view(0), 0);
    ui.run_job = fake_job;
    CHECK_INT(lf_job_view(&ui), 1);
    CHECK_INT(job_calls, 0);
}

static void a_run_leaf_gets_its_own_window_not_the_menus_console(void)
{
    CHECK_INT((int)lf_plan(KA_RUN), (int)LF_WINDOW);
    CHECK_INT((int)lf_plan(KA_REXX), (int)LF_REXX);
    CHECK_INT((int)lf_plan(KA_OPEN), (int)LF_OPEN);
    CHECK_INT((int)lf_plan(KA_SLASH), (int)LF_REQUEST);
    CHECK_INT((int)lf_plan(KA_MENUID), (int)LF_REQUEST);
    CHECK_INT((int)lf_plan_t(KA_SLASH, 0), (int)LF_OPEN_THEN_REQUEST);   /* no terminal: open one, then send the slash command */
    CHECK_INT((int)lf_plan_t(KA_SLASH, 1), (int)LF_REQUEST);
    CHECK_INT((int)lf_plan_t(KA_RUN, 0), (int)LF_WINDOW);
    /* a row that needs a value (slash:/bg) with no terminal: ask FIRST, while the menu has the focus; the new
     * terminal takes it and the menu's INACTIVEWINDOW closes the prompt before it is answered (rig 7, 27eda4b7) */
    CHECK(upreq_needs_value(KA_SLASH, "/bg") != 0);
    CHECK(upreq_needs_value(KA_SLASH, "/theme") == 0);
    CHECK(upreq_needs_value(KA_SLASH, "/font") == 0);
    CHECK(upreq_needs_value(KA_SLASH, "/bg 102030") == 0);
    CHECK(upreq_needs_value(KA_TAB, "new") == 0);
    CHECK_INT((int)lf_plan_ta(KA_SLASH, 0, upreq_needs_value(KA_SLASH, "/bg") != 0), (int)LF_ASK_THEN_OPEN_REQUEST);
    CHECK_INT((int)lf_plan_ta(KA_SLASH, 0, upreq_needs_value(KA_SLASH, "/theme") != 0), (int)LF_OPEN_THEN_REQUEST);
    CHECK_INT((int)lf_plan_ta(KA_SLASH, 1, 1), (int)LF_REQUEST);
    CHECK_INT((int)lf_plan(KA_TAB), (int)LF_REQUEST);
    CHECK(!strncmp(LF_UTERM_SPEC, "XCON:", 5));
}

/* AMB3 (owner, 2026-10-23): leaves opened from a DOpus screen landed on Workbench, because the window
 * spec had no SCREEN option. The spec builder appends the front screen's public name, and nothing when
 * there is none (the default public screen). */
static void a_leaf_window_spec_names_the_front_public_screen(void)
{
    char out[128];
    CHECK_INT(lf_screen_spec(out, sizeof out, LF_UTERM_SPEC, "DOpus.1"), 1);
    CHECK_STR(out, "XCON:0/20/640/300/UP-Term/CLOSE/SCREEN DOpus.1");
    CHECK_INT(lf_screen_spec(out, sizeof out, LF_UTERM_SPEC, ""), 1);
    CHECK_STR(out, LF_UTERM_SPEC);
    CHECK_INT(lf_screen_spec(out, sizeof out, LF_UTERM_SPEC, 0), 1);
    CHECK_STR(out, LF_UTERM_SPEC);
    CHECK_INT(lf_screen_spec(out, 20, LF_UTERM_SPEC, "A"), 0);        /* does not fit */
    CHECK_INT(lf_screen_spec(out, sizeof out, LF_UTERM_SPEC, "A/B"), 0); /* '/' would end the option */
    CHECK_STR(out, "");
}


/* DOPUS D2 (plan 2026-10-25-amarchy-dopus-integration.md): the three Directory Opus hotkeys parse from the
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

static int node_has(const char *node, const char *label, int cond)
{
    mn_item it[MN_MAX_ITEMS];
    int i, n = mn_children_t(&km, node, it, MN_MAX_ITEMS, cond);
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].label, label))
            return 1;
    return 0;
}

/* DOPUS D3: the Dopus group and its rows are drawn only when DOPUS.1 exists (MN_COND_DOPUS, probed once at
 * open by upactive_dopus_present); every other group is unaffected, a terminal is not needed for them. */
/* D5: the dopus-*.rexx scripts end RC 5 without DOPUS.1; that and only that is "Directory Opus is not running". */
static void a_dopus_script_without_a_port_has_a_notice(void)
{
    const char *n = lf_rexx_notice("UP-Term:Rexx/dopus-newlister.rexx", 5);
    CHECK(n && strcmp(n, "Directory Opus is not running") == 0);
    CHECK(lf_rexx_notice("dopus-front.rexx", 5) != 0);
    CHECK(lf_rexx_notice("UP-Term:Rexx/dopus-front.rexx", 0) == 0);
    CHECK(lf_rexx_notice("UP-Term:Rexx/dopus-front.rexx", 10) == 0);
    CHECK(lf_rexx_notice("UP-Term:Rexx/other.rexx", 5) == 0);
    CHECK(lf_rexx_notice(0, 5) == 0);
}

/* D5 hotkey: act_rexx runs one RX line that calls the script and shows the notice window only on RC 5. */
static void a_hotkey_dopus_script_without_a_port_shows_the_notice_window(void)
{
    char c[480];
    const char *spec = "XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS 36x3";
    const char *want =
        "RX \"r = 'UP-Term:Rexx/dopus-front.rexx'(); if r = 5 then do; if open(w, '"
        "XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS 36x3', 'W') then do; "
        "call writech(w, 'Directory Opus is not running'); address command 'Wait 3 SECS'; "
        "call close(w); end; end\"";
    CHECK(lf_rexx_notice_line(c, sizeof c, "UP-Term:Rexx/dopus-front.rexx", spec) == 1);
    CHECK(strcmp(c, want) == 0);
    CHECK(lf_rexx_notice_line(c, sizeof c, "UP-Term:Rexx/other.rexx", spec) == 0 && c[0] == 0);
    CHECK(lf_rexx_notice_line(c, 8, "dopus-front.rexx", spec) == 0 && c[0] == 0);
    CHECK(lf_rexx_notice_line(c, sizeof c, "RAM:dopus-a'b.rexx", spec) == 0);
    CHECK(lf_rexx_notice_line(c, sizeof c, "RAM:dopus-a\"b.rexx", spec) == 0);
    CHECK(lf_rexx_notice_line(c, sizeof c, "dopus-front.rexx", "XCON:a'b") == 0);
}

static void the_dopus_group_needs_a_port(void)
{
    static char file[32768];
    long n = slurp("dist/keymap", file, sizeof(file));
    CHECK(n > 0 && keymap_parse(&km, file, n));
    CHECK_INT(mn_cond(1, 0, 0), MN_COND_TERM);
    CHECK_INT(mn_cond(0, 1, 0), MN_COND_DOPUS);
    CHECK_INT(mn_row_needs(KA_REXX, "dopus.front"), MN_COND_DOPUS);
    CHECK_INT(mn_row_needs(KA_OPEN, "dopus.shell"), MN_COND_DOPUS);
    CHECK_INT(mn_row_needs(KA_REXX, "apps.claude"), 0);
    CHECK_INT(mn_row_needs(KA_SLASH, "style.terminal.font"), 0);   /* the Terminal group opens a terminal when none is open */
    CHECK_INT(mn_row_needs(KA_SLASH, "style.other"), MN_COND_TERM);
    CHECK(!node_has("", "Dopus", mn_cond(1, 0, 1)));            /* no port: the group is gone, terminal or not */
    CHECK(!node_has("", "Dopus", mn_cond(0, 0, 1)));
    CHECK(node_has("", "Dopus", mn_cond(1, 1, 1)));              /* port present: the group shows */
    CHECK(node_has("", "Dopus", mn_cond(0, 1, 1)));
    CHECK(node_has("", "Dopus", MN_COND_ALL));
    CHECK(node_has("", "Dopus", mn_cond(1, 1, 1)) && node_has("", "Apps", mn_cond(1, 0, 1)));
    CHECK(node_has("dopus", "Open lister", mn_cond(0, 1, 1)));
    CHECK(!node_has("dopus", "Open lister", mn_cond(1, 0, 1)));
    {
        int c1, r1, c2, r2;                                   /* the OSD size ignores the hidden group */
        mn_osd_size_t(&km, "", &c1, &r1, mn_cond(1, 0, 1));
        mn_osd_size_t(&km, "", &c2, &r2, mn_cond(1, 1, 1));
        CHECK(r2 >= r1 && c2 >= c1);
    }
}

/* K9 port-present hiding: the Workbench group is drawn only when the WORKBENCH port exists; Screens always. */
static void the_workbench_group_needs_its_port(void)
{
    CHECK_INT(mn_cond(1, 0, 0), MN_COND_TERM);
    CHECK_INT(mn_cond(0, 0, 1), MN_COND_WB);
    CHECK_INT(mn_row_needs(KA_REXX, "workbench.backdrop"), MN_COND_WB);
    CHECK_INT(mn_row_needs(KA_WIN, "screens.next"), 0);
    CHECK(!node_has("", "Workbench", mn_cond(1, 1, 0)));
    CHECK(!node_has("", "Workbench", mn_cond(0, 0, 0)));
    CHECK(node_has("", "Workbench", mn_cond(0, 0, 1)));
    CHECK(node_has("", "Workbench", MN_COND_ALL));
    CHECK(node_has("", "Screens", mn_cond(0, 0, 0)));
    CHECK(node_has("workbench", "Redraw All", mn_cond(0, 0, 1)));
    CHECK(node_has("screens", "Next screen", mn_cond(0, 0, 0)));
    CHECK(!node_has("screens", "UP-Term own screen", mn_cond(0, 0, 0)));   /* slash: needs a terminal */
    CHECK(node_has("screens", "UP-Term own screen", mn_cond(1, 0, 0)));
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
    CHECK_INT(dopus, 11);                       /* ten rows plus the dopus.listers group (amarchy/index_core.h) */
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

/* Apps > Editor (run:nvim) started nothing: nvim is on vsh's $PATH (UP-Term:nvim/bin), not on the AmigaDOS
 * path SystemTags searched. The leaf runs through the UP-Term shell, as a tab would. */
static void a_run_row_starts_its_command_through_the_upterm_shell(void)
{
    char out[128];
    CHECK_INT(lf_shell_command(out, sizeof out, "UP-Term:bin/vsh", "nvim"), 1);
    CHECK_STR(out, "\"UP-Term:bin/vsh\" -c \"nvim\"");
    CHECK_INT(lf_shell_command(out, sizeof out, "UP-Term:bin/vsh", "upkg upgrade"), 1);
    CHECK_STR(out, "\"UP-Term:bin/vsh\" -c \"upkg upgrade\"");
    CHECK_INT(lf_shell_command(out, sizeof out, 0, "tmux"), 1);      /* no vsh: the AmigaDOS command */
    CHECK_STR(out, "tmux");
    CHECK_INT(lf_shell_command(out, sizeof out, "UP-Term:bin/vsh", "a\"b"), 0); /* a quote ends the argument */
    CHECK_STR(out, "");
    CHECK_INT(lf_shell_command(out, 20, "UP-Term:bin/vsh", "nvim"), 0);          /* does not fit */
    CHECK_INT(lf_shell_command(out, sizeof out, "UP-Term:bin/vsh", ""), 0);
}

/* A detached start (detach: rows, Install > Packages rows) gives the program the stack its $STACK cookie asks
 * for (RULES.md rule 6): the launcher loads the line's first word. vsh asks 262144; it was started on 32768. */
static void a_detached_line_names_the_program_whose_stack_cookie_counts(void)
{
    char o[64], small[6];
    CHECK(lf_command_program("\"UP-Term:bin/vsh\" -c \"gamma-go\"", o, (int)sizeof o));
    CHECK_STR(o, "UP-Term:bin/vsh");
    CHECK(lf_command_program("  UP-Term:bin/nvim -R x", o, (int)sizeof o));
    CHECK_STR(o, "UP-Term:bin/nvim");
    CHECK(lf_command_program("\"Work:My Game/go\" -w", o, (int)sizeof o));
    CHECK_STR(o, "Work:My Game/go");
    CHECK(lf_command_program("tmux", o, (int)sizeof o));
    CHECK_STR(o, "tmux");
    CHECK(!lf_command_program("\"Work:open", o, (int)sizeof o));
    CHECK(!lf_command_program("   ", o, (int)sizeof o));
    CHECK(!lf_command_program("UP-Term:x", small, (int)sizeof small));
}

/* Owner's A1200, 2026-10-26: no DOpus hotkey or menu row did anything. RX "<arg>" runs the
 * quoted text as an inline program, so a script path was never opened; a script goes to RX bare,
 * an inline string stays quoted. */
static void a_rexx_script_path_runs_as_a_script_and_a_string_inline(void)
{
    char out[128];
    CHECK_INT(lf_rexx_command(out, sizeof out, "UP-Term:Rexx/dopus-newlister.rexx"), 1);
    CHECK_STR(out, "RX UP-Term:Rexx/dopus-newlister.rexx");
    CHECK_INT(lf_rexx_command(out, sizeof out, "REXX:tidy.rx"), 1);
    CHECK_STR(out, "RX REXX:tidy.rx");
    CHECK_INT(lf_rexx_command(out, sizeof out, "Tidy.REXX"), 1);       /* the suffix in any case */
    CHECK_STR(out, "RX Tidy.REXX");
    CHECK_INT(lf_rexx_command(out, sizeof out, "address WORKBENCH 'WINDOW ACTIVE CLOSE'"), 1);
    CHECK_STR(out, "RX \"address WORKBENCH 'WINDOW ACTIVE CLOSE'\"");
    CHECK_INT(lf_rexx_command(out, sizeof out, "say \"hi\""), 1);      /* a quote would end the string */
    CHECK_STR(out, "RX \"say 'hi'\"");
    CHECK_INT(lf_rexx_command(out, sizeof out, ""), 0);
    CHECK_STR(out, "");
    CHECK_INT(lf_rexx_command(out, sizeof out, "say 1\nsay 2"), 0);    /* a newline ends the command line */
    CHECK_INT(lf_rexx_command(out, 10, "UP-Term:Rexx/dopus-front.rexx"), 0); /* does not fit */
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

/* The Amarchy commodity holds only its own rows, at exact size: the heap bytes of dist/keymap
 * after keymap_compact(cx_row_installed) stay under a ceiling (the rig measured 17944 B for the
 * parsed table, 8288 B on the host), carry no
 * [menu] rows, no slack beyond the block headers, and every kept row is found unchanged. */
#define AMARCHY_KEYMAP_HEAP_MAX 3400  /* 46 rows (8 + the 18 window hotkeys + the three Directory Opus hotkeys + the three Screens hotkeys + the 15 AmTile layout hotkeys); 3365 B measured; the full parsed table is larger */
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
    CHECK(held <= AMARCHY_KEYMAP_HEAP_MAX);
    keymap_free(&km);
    CHECK_INT((int)(vt_count_live - base), 0);
}

static void menu_theme_roles_map_the_palette(void);
static void dopus_theme_commands_use_the_theme_directly(void);
static void minimal_mode_reaches_the_program(void);
static void the_menu_takes_its_ground_and_text_from_the_upconf_fg_and_bg(void);

static void the_app_index_core_behaves(void);

static void app_icons_are_cached_per_path_bounded_and_fitted(void);
static void the_icon_cache_file_round_trips_and_rejects_bad_buffers(void);
static void the_scan_record_names_the_last_path_and_the_where_text_tells_running_stuck_died_and_done(void)
{
    ix_prog p = {0};
    char buf[1024];
    long seq;
    ix_prog_begin(&p);
    CHECK_INT(p.stage, IX_ST_VOLUMES);
    ix_prog_note(&p, IX_ST_WALK, "Work:Games/Turrican");
    ix_prog_note(&p, IX_ST_ICON, "Work:Games/Turrican/Turrican");
    ix_prog_note(&p, IX_ST_ICON, "Work:Games/Turrican/Readme");
    CHECK_INT(p.dirs, 1);
    CHECK_INT(p.icons, 2);
    CHECK_INT(p.seq, 4);
    CHECK_STR(p.path, "Work:Games/Turrican/Readme");
    CHECK(ix_prog_text(&p, 1, 1, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "running") && strstr(buf, "stage: icon") && strstr(buf, "volume: Work") &&
          strstr(buf, "path: Work:Games/Turrican/Readme") && strstr(buf, "drawers: 1, icons: 2"));
    CHECK(ix_prog_text(&p, 1, 0, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "stuck, or stopped at a requester") && strstr(buf, "path: Work:Games/Turrican/Readme"));
    CHECK(ix_prog_text(&p, 0, 0, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "it died") && strstr(buf, "path: Work:Games/Turrican/Readme"));
    seq = p.seq;
    ix_prog_done(&p, 7, 2, 3);
    CHECK(p.seq > seq);
    CHECK(ix_prog_text(&p, 0, 0, buf, sizeof buf) > 0);
    CHECK(strstr(buf, ": done") && strstr(buf, "7 apps, 2 games on 3 volumes") && !strstr(buf, "died"));
    CHECK_INT(ix_prog_text(&p, 0, 0, buf, 20), -1);        /* does not fit */
    ix_prog_note(&p, IX_ST_WALK, "Work:Games");           /* Ctrl-C: stage stopped, path kept, not "died" */
    ix_prog_stopped(&p);
    CHECK_INT(p.stage, IX_ST_STOPPED);
    CHECK(ix_prog_text(&p, 0, 0, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "stopped by Ctrl-C") && strstr(buf, "stage: stopped") && strstr(buf, "path: Work:Games") && !strstr(buf, "died"));
    {
        char longp[IX_PATH + 40];
        memset(longp, 'a', sizeof longp);
        longp[sizeof longp - 1] = 0;
        ix_prog_note(&p, IX_ST_WALK, longp);
        CHECK_INT(strlen(p.path), IX_PATH - 1);
    }
    memset(&p, 0, sizeof p);
    p.version = IX_PROG_VERSION;
    CHECK(ix_prog_text(&p, 0, 0, buf, sizeof buf) > 0 && strstr(buf, "no scan has run"));
}

static void the_scan_record_has_a_layout_version_and_an_older_one_is_not_read(void)
{
    ix_prog p = {0};
    char buf[1024];
    ix_prog_begin(&p);
    CHECK_INT(p.version, IX_PROG_VERSION);
    ix_prog_note(&p, IX_ST_WALK, "Work:Games");
    p.version = IX_PROG_VERSION - 1;   /* a record another build left in memory */
    CHECK(ix_prog_text(&p, 1, 1, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "another amarchy-index build, reboot to clear") && !strstr(buf, "Work:Games") &&
          !strstr(buf, "stage:"));
    CHECK_INT(ix_prog_text(&p, 1, 1, buf, 10), -1);
    CHECK(ix_prog_other_text(buf, sizeof buf) > 0 && strstr(buf, "another amarchy-index build"));
    /* the scanner never reuses the older semaphore: its name differs from the older build's */
    CHECK(strcmp(IX_PROG_NAME, ix_prog_old_names[0]) != 0 && strcmp(IX_PROG_NAME, ix_prog_old_names[1]) != 0);
    CHECK_STR(ix_prog_old_names[0], "Amarchy.index.2");
    CHECK_STR(ix_prog_old_names[1], "Amarchy.index");
    CHECK(ix_prog_old_names[2] == 0);
}

static void a_failed_cache_write_is_its_own_stage_and_not_reported_as_died(void)
{
    ix_prog p = {0};
    char buf[1024];
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WRITE, "ENVARC:Amarchy/apps");
    ix_prog_write_failed(&p);
    CHECK_INT(p.stage, IX_ST_WRITE_FAILED);
    CHECK(ix_prog_text(&p, 0, 0, buf, sizeof buf) > 0);
    CHECK(strstr(buf, "cache write failed") && strstr(buf, "stage: write failed") &&
          strstr(buf, "path: ENVARC:Amarchy/apps") && !strstr(buf, "died"));
}

static void a_scan_leaf_parses_to_ka_scan_and_a_detach_leaf_stays_ka_detach(void)
{
    CHECK(parse("[keymap]\nversion = 1\n[menu]\napps.rescan = Rescan apps | scan:amarchy-index\napps.bg = Bg | detach:foo\n"));
    CHECK_INT(km.nmenu, 2);
    CHECK_INT(km.menu[0].kind, KA_SCAN);
    CHECK_STR(km.menu[0].arg, "amarchy-index");
    CHECK_INT(km.menu[1].kind, KA_DETACH);
}

static void ixw_poll(ix_watch *w, const ix_prog *p, int have, int alive, int want, const char *text)
{
    char row[96];
    int st = ix_watch_row(w, have, p, alive, row, sizeof row);
    CHECK_INT(st, want);
    CHECK_STR(row, text);
}

/* RESCAN-ONE-SCANNER: Rescan chosen again while a scan ran started a second amarchy-index (three ran at
 * once on the A1200, all writing the apps file). The menu now shows the live scan and starts nothing. */
static void a_second_rescan_while_a_scan_runs_shows_that_scan_and_starts_nothing(void)
{
    ix_prog p = {0};
    ix_watcher x;
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    CHECK(ix_scan_live(1, &p, 1));
    CHECK_INT(ix_watcher_adopt_live(&x, 1, &p, 1), 1);
    CHECK_INT(x.state, IXW_RUNNING);
    CHECK_INT(x.on, 1);
    CHECK_STR(x.row, "Scanning... Work:Apps");
}

static void a_rescan_after_a_finished_stopped_or_dead_scan_starts_a_new_one(void)
{
    ix_prog p = {0};
    ix_watcher x;
    CHECK(!ix_scan_live(0, 0, 0));                         /* no record since boot */
    CHECK_INT(ix_watcher_adopt_live(&x, 0, &p, 0), 0);
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_ICON, "Work:Apps/Foo.info");
    CHECK(!ix_scan_live(1, &p, 0));                        /* the scanner task is gone */
    ix_prog_done(&p, 3, 4, 1);
    CHECK(!ix_scan_live(1, &p, 1));                        /* finished */
    ix_prog_stopped(&p);
    CHECK(!ix_scan_live(1, &p, 1));                        /* stopped by Ctrl-C */
    ix_prog_begin(&p);
    p.version = IX_PROG_VERSION + 1;                       /* another layout: its fields are not read */
    CHECK(!ix_scan_live(1, &p, 1));
}

static void a_menu_that_started_a_scan_shows_scanning_starting_before_the_scanner_begins(void)
{
    ix_prog p = {0};
    ix_watch w = { 4, 0 };
    ixw_poll(&w, 0, 0, 0, IXW_STARTING, "Scanning... starting");              /* no record yet */
    p.version = IX_PROG_VERSION;
    p.runs = 4;
    p.stage = IX_ST_DONE;                                                      /* the older scan's record */
    ixw_poll(&w, &p, 1, 0, IXW_STARTING, "Scanning... starting");
    ix_prog_begin(&p);                                                         /* runs 5: the new scan began */
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    ixw_poll(&w, &p, 1, 1, IXW_RUNNING, "Scanning... Work:Apps");
}

static void a_running_scan_shows_its_path_cut_from_the_left(void)
{
    ix_prog p = {0};
    ix_watch w = { -1, 0 };
    char row[24];
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WALK, "Work:Games/Turrican/Data");
    CHECK_INT(ix_watch_row(&w, 1, &p, 1, row, sizeof row), IXW_RUNNING);
    CHECK_STR(row, "Scanning... ...can/Data");        /* 23 characters: the drawer name stays */
}

static void a_scan_whose_task_is_gone_shows_it_stopped_working_at_its_path(void)
{
    ix_prog p = {0};
    ix_watch w = { 0, 0 };
    ix_prog_begin(&p);                                 /* runs 1 > runs0 0 */
    ix_prog_note(&p, IX_ST_ICON, "Work:Apps/Foo");
    ixw_poll(&w, &p, 1, 0, IXW_FAILED, "Scan stopped working at Work:Apps/Foo");
}

static void a_finished_scan_shows_the_new_counts(void)
{
    ix_prog p = {0};
    ix_watch w = { 0, 0 };
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    ix_prog_done(&p, 12, 3, 2);
    ixw_poll(&w, &p, 1, 0, IXW_DONE, "Scan done: 12 apps, 3 games");
}

static void an_old_finished_record_is_not_shown_in_a_menu_that_did_not_start_it(void)
{
    ix_prog p = {0};
    ix_watch w = { -1, 0 };
    ix_prog_begin(&p);
    ix_prog_done(&p, 12, 3, 2);
    ixw_poll(&w, &p, 1, 0, IXW_NONE, "");
    ixw_poll(&w, 0, 0, 0, IXW_NONE, "");
    p.stage = IX_ST_STOPPED;
    ixw_poll(&w, &p, 1, 0, IXW_NONE, "");
    p.version = IX_PROG_VERSION - 1;                   /* another layout is never read */
    ixw_poll(&w, &p, 1, 1, IXW_NONE, "");
    /* a running scan is adopted, so its end is shown in the same menu */
    p.version = IX_PROG_VERSION;
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_ICON, "Work:A");
    ixw_poll(&w, &p, 1, 1, IXW_RUNNING, "Scanning... Work:A");
    ix_prog_done(&p, 1, 0, 1);
    ixw_poll(&w, &p, 1, 0, IXW_DONE, "Scan done: 1 apps, 0 games");
}

static void a_scan_that_never_begins_is_reported_after_10_polls(void)
{
    ix_watch w = { 7, 0 };
    char row[96];
    int i;
    for (i = 0; i < IXW_START_POLLS; i++)
        CHECK_INT(ix_watch_row(&w, 0, 0, 0, row, sizeof row), IXW_STARTING);
    CHECK_INT(ix_watch_row(&w, 0, 0, 0, row, sizeof row), IXW_FAILED);
    CHECK_STR(row, "Scan did not start");
}

static void stopped_and_write_failed_each_name_the_failure(void)
{
    ix_prog p = {0};
    ix_watch w = { 0, 0 };
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    ix_prog_stopped(&p);
    ixw_poll(&w, &p, 1, 0, IXW_FAILED, "Scan stopped at Work:Apps");
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_WRITE, "ENVARC:Amarchy/apps");
    ix_prog_write_failed(&p);
    ixw_poll(&w, &p, 1, 0, IXW_FAILED, "Scan could not save the list");
}

static void a_watching_menu_rebuilds_only_when_the_status_row_changes_and_stops_at_done(void)
{
    ix_prog p = {0};
    ix_watcher x;
    ix_prog_begin(&p);                                   /* runs 1 */
    ix_watcher_begin(&x, 1, 0, 0, 0);                    /* started by this menu, no record before */
    CHECK_INT(x.on, 1);
    CHECK_STR(x.row, "Scanning... starting");
    CHECK_INT(ix_watcher_step(&x, 0, 0, 0), 0);          /* no record yet: same row, no rebuild */
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    CHECK_INT(ix_watcher_step(&x, 1, &p, 1), 1);
    CHECK_STR(x.row, "Scanning... Work:Apps");
    CHECK_INT(ix_watcher_step(&x, 1, &p, 1), 0);         /* unchanged: the menu does not redraw */
    ix_prog_done(&p, 3, 1, 1);
    CHECK_INT(ix_watcher_step(&x, 1, &p, 0), 1);
    CHECK_INT(x.state, IXW_DONE);
    CHECK_INT(x.on, 0);                                  /* ticks stop, the row stays */
    CHECK_STR(x.row, "Scan done: 3 apps, 1 games");
}

static void a_menu_opened_during_a_scan_it_did_not_start_adopts_it_and_a_quiet_record_shows_no_row(void)
{
    ix_prog p = {0};
    ix_watcher x;
    ix_prog_begin(&p);
    ix_prog_note(&p, IX_ST_ICON, "Work:Apps/Foo");
    ix_watcher_begin(&x, 0, 1, &p, 1);
    CHECK_INT(x.on, 1);
    CHECK_STR(x.row, "Scanning... Work:Apps/Foo");
    ix_prog_done(&p, 1, 0, 1);
    ix_watcher_begin(&x, 0, 1, &p, 0);                   /* an old result is not news */
    CHECK_INT(x.on, 0);
    CHECK_STR(x.row, "");
    ix_watcher_begin(&x, 0, 0, 0, 0);
    CHECK_INT(x.on, 0);
}

/* AMARCHY_ROWS: amarchy-menu (text) and amarchy-gui (drawn) both call mn_rows_join, so one format: the shown rows'
 * labels in display order joined by '|', whole labels only. */
static void both_menu_styles_publish_the_shown_rows_through_one_join(void)
{
    mn_item it[3];
    int idx[3] = { 2, 0, 1 };
    char out[16], big[64];
    memset(it, 0, sizeof it);
    strcpy(it[0].label, "Alpha");
    strcpy(it[1].label, "Rescan apps");
    strcpy(it[2].label, "Zed");
    mn_rows_join(it, idx, 3, big, sizeof big);
    CHECK_STR(big, "Zed|Alpha|Rescan apps");
    mn_rows_join(it, idx, 3, out, sizeof out);   /* 16 bytes: the third label no longer fits whole */
    CHECK_STR(out, "Zed|Alpha");
    mn_rows_join(it, idx, 0, big, sizeof big);
    CHECK_STR(big, "");
}

static void the_cursor_stays_on_the_same_entry_when_the_status_row_shifts_the_level(void)
{
    mn_item it[3];
    int idx[3] = { 2, 0, 1 }, c = 3;
    memset(it, 0, sizeof it);
    strcpy(it[0].label, "Alpha");
    strcpy(it[1].label, "Rescan apps");
    strcpy(it[2].label, "Zed");
    CHECK_INT(mn_pick(it, idx, c, "Rescan apps"), 2);
    CHECK_INT(mn_pick(it, idx, c, "Zed"), 0);
    CHECK_INT(mn_pick(it, idx, c, "Gone"), -1);
    CHECK_INT(mn_pick(it, idx, c, 0), -1);
}

static void a_run_counter_survives_ix_prog_begin(void)
{
    ix_prog p = {0};
    ix_prog_begin(&p);
    CHECK_INT(p.runs, 1);
    ix_prog_note(&p, IX_ST_WALK, "Work:Apps");
    ix_prog_begin(&p);
    CHECK_INT(p.runs, 2);
    CHECK_INT(p.dirs, 0);
}

static void a_volume_without_a_handler_task_matches_no_device(void)
{
    int a, b;
    CHECK_INT(ix_same_handler_task(&a, &a), 1);
    CHECK_INT(ix_same_handler_task(&a, &b), 0);
    CHECK_INT(ix_same_handler_task(0, 0), 0);
    CHECK_INT(ix_same_handler_task(0, &b), 0);
}


/* OMARCHY-AI F2: the agents panel the bar's agents segment opens (node BAR_AGENT_NODE of the shipped keymap). The
 * agent rows go through /agent (the ai-agent key), the launch row through the action the bar's right click and
 * super+l share, and a prompt row parses to /ask with its task, the A4 path (Agent "<task>"). */
static void the_agents_panel_rows_reach_the_agent_paths(void)
{
    mn_item it[MN_MAX_ITEMS];
    int i, n, asks = 0, agents = 0, launch = 0, limits = 0;
    n = mn_children(&km, BAR_AGENT_NODE, it, MN_MAX_ITEMS);
    CHECK(n >= 5);
    for (i = 0; i < n; i++) {
        const km_menu *r = &km.menu[it[i].row];
        slash_cmd cmd;
        char err[80];
        if (!strcmp(it[i].seg, "limits"))
            limits++;
        if (r->kind == KA_MENUID && !strcmp(r->arg, BAR_LAUNCH_MENUID))
            launch++;
        if (r->kind != KA_SLASH)
            continue;
        CHECK(slash_parse(r->arg, (int)strlen(r->arg), &cmd, err, sizeof err) == SLASH_OK);
        if (cmd.id == SLASH_ASK) {
            CHECK(cmd.arg[0] != 0);
            asks++;
        } else if (!strcmp(cmd.arg, "claude") || !strcmp(cmd.arg, "pi")) {
            agents++;
        }
    }
    CHECK_INT(agents, 2);
    CHECK_INT(launch, 1);
    CHECK(asks >= 3);
    CHECK_INT(limits, 0);       /* part E is not built: no meter row, no invented number */
}

void suite_keymap(void)
{
    style_theme_lists_the_gui_theme_rows();
    packages_group_lists_its_rows_and_drops_the_old_node();
    a_picked_package_offers_only_its_fitting_actions();
    the_package_list_loads_once_and_reloads_when_stale();
    the_package_cache_survives_the_per_open_index_reset();
    a_failed_upkg_gives_one_inert_row();
    package_search_asks_its_text_and_lists_the_matches();
    remove_asks_first_and_a_package_job_marks_the_list_stale();
    a_detached_line_names_the_program_whose_stack_cookie_counts();
    the_scan_record_has_a_layout_version_and_an_older_one_is_not_read();
    a_failed_cache_write_is_its_own_stage_and_not_reported_as_died();
    a_scan_leaf_parses_to_ka_scan_and_a_detach_leaf_stays_ka_detach();
    a_run_counter_survives_ix_prog_begin();
    a_menu_that_started_a_scan_shows_scanning_starting_before_the_scanner_begins();
    a_second_rescan_while_a_scan_runs_shows_that_scan_and_starts_nothing();
    a_rescan_after_a_finished_stopped_or_dead_scan_starts_a_new_one();
    a_running_scan_shows_its_path_cut_from_the_left();
    a_scan_whose_task_is_gone_shows_it_stopped_working_at_its_path();
    a_finished_scan_shows_the_new_counts();
    an_old_finished_record_is_not_shown_in_a_menu_that_did_not_start_it();
    a_scan_that_never_begins_is_reported_after_10_polls();
    stopped_and_write_failed_each_name_the_failure();
    a_watching_menu_rebuilds_only_when_the_status_row_changes_and_stops_at_done();
    a_menu_opened_during_a_scan_it_did_not_start_adopts_it_and_a_quiet_record_shows_no_row();
    the_cursor_stays_on_the_same_entry_when_the_status_row_shifts_the_level();
    both_menu_styles_publish_the_shown_rows_through_one_join();
    a_volume_without_a_handler_task_matches_no_device();
    menu_theme_roles_map_the_palette();
    dopus_theme_commands_use_the_theme_directly();
    minimal_mode_reaches_the_program();
    the_menu_takes_its_ground_and_text_from_the_upconf_fg_and_bg();
    the_commodity_keymap_of_dist_keymap_is_exact_and_small();
    a_reload_swaps_whole_keeps_the_old_map_on_a_bad_edit_and_leaks_nothing();
    comments_crlf_and_fields();
    digits_expand_with_percent_n();
    junk_file_keeps_the_old_keymap();
    bad_rows_are_skipped_and_counted();
    the_safety_refusals();
    a_newer_major_is_rejected_whole();
    a_later_row_replaces_and_none_unbinds();
    the_user_file_overrides_the_shipped_one();
    the_tables_have_caps();
    key_names_are_raw_codes();
    symbol_names_follow_the_printed_key();
    the_shipped_keymap_loads_whole();
    actions_resolve_to_menu_ids();
    win_actions_parse();
    win_scope_is_commodity();
    win_defers_in_the_handler();
    the_shipped_file_resolves_and_derives_menu_keys();
    the_shipped_menu_has_learn_system_and_install_groups();
    the_window_group_rows_and_hotkeys();
    settings_page_group_lists_the_table_rows();
    right_enters_only_rows_with_children();
    shipped_settings_has_the_six_page_groups();
    the_agents_panel_rows_reach_the_agent_paths();
    the_screens_group_rows_and_hotkeys();
    the_workbench_group_rows();
    cx_descriptions_and_qualifier_mask();
    cx_installs_only_the_commodity_rows();
    ua_count_released_before_own_window_closes();
    typing_th_leaves_theme_first();
    menu_levels_follow_the_ids_and_escape_goes_up();
    terminal_rows_show_only_over_a_terminal();
    menu_size_follows_the_filtered_rows();
    osd_size_counts_the_reachable_levels();
    drawn_menu_geometry_follows_the_font_and_screen();
    blit_starts_where_the_beam_cannot_cross_it();
    a_slow_blit_keeps_the_estimate_up_so_the_bar_does_not_tear();
    a_tall_copy_goes_a_row_at_a_time();
    drawn_menu_icons_look_square_on_tall_pixels();
    icons_are_not_upscaled_from_16px();
    every_shipped_menu_node_has_an_icon_image();
    the_key_viewer_lists_every_bind_row();
    the_key_viewer_follows_a_rebinding();
    a_request_resolves_like_its_bind_row();
    a_request_needs_no_bind_row();
    an_unknown_request_is_an_error_code();
    the_menu_builds_the_request_of_a_leaf();
    a_job_row_parses_and_falls_back_to_run_without_a_job_view();
    a_run_leaf_gets_its_own_window_not_the_menus_console();
    a_leaf_window_spec_names_the_front_public_screen();
    dopus_binds_parse();
    the_dopus_group_needs_a_port();
    the_workbench_group_needs_its_port();
    a_dopus_script_without_a_port_has_a_notice();
    a_hotkey_dopus_script_without_a_port_shows_the_notice_window();
    the_dopus_rows_are_shipped();
    a_run_row_starts_its_command_through_the_upterm_shell();
    a_rexx_script_path_runs_as_a_script_and_a_string_inline();
    the_quick_menu_finds_its_terminal_after_an_earlier_menu_window_closed();
    the_target_is_the_front_entry_for_a_menu_outside_the_record();
    a_refused_slash_request_answers_why_and_a_missing_value_is_asked_for();
    the_app_index_core_behaves();
    app_icons_are_cached_per_path_bounded_and_fitted();
    the_icon_cache_file_round_trips_and_rejects_bad_buffers();
}
/* ---- menu_theme: the drawn menu's colour roles ---- */
#include "../amarchy/menu_theme.h"

/* role -> palette index is the table's: the shipped default palette is the renderer's xterm one, and the
 * table's defaults are those values (hand-written below, then held to the engine) */
static void menu_theme_roles_map_the_palette(void)
{
    static const uc_u32 want_default[MT_ROLES] = { 0x000000UL, 0xE5E5E5UL, 0x7F7F7FUL, 0x00FFFFUL, 0x0000EEUL, 0x00FF00UL, 0xFF0000UL };
    uc_u32 pal[16], rgb[MT_ROLES];
    int i;

    /* nothing remapped (upconf has no palette key): the xterm colours of the role's index */
    upconf_palette_parse(0, pal);
    menu_theme_roles(pal, rgb);
    for (i = 0; i < MT_ROLES; i++) {
        CHECK_INT((long)rgb[i], (long)want_default[i]);
        CHECK_INT((long)rgb[i], (long)vt_palette_rgb(0, menu_theme_index(i)));
    }
    CHECK_INT(menu_theme_index(MT_BG), 0);
    CHECK_INT(menu_theme_index(MT_TEXT), 7);
    CHECK_INT(menu_theme_index(MT_DIM), 8);
    CHECK_INT(menu_theme_index(MT_ACCENT), 14);
    CHECK_INT(menu_theme_index(MT_SELBOX), 4);
    CHECK_INT(menu_theme_index(MT_OK), 10);
    CHECK_INT(menu_theme_index(MT_FAIL), 9);
    /* outline and selected row box are one colour: the border role is the selection box role, not the accent */
    CHECK_INT(MT_BORDER_ROLE, MT_SELBOX);

    /* a Dracula-like palette remaps 0, 7, 8, 14 and 4; index 1 (no role) is ignored */
    CHECK_INT(upconf_palette_parse("0,282A36,1,FF5555,7,F8F8F2,8,6272A4,14,8BE9FD,4,44475A", pal), 6);
    menu_theme_roles(pal, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x282A36L);
    CHECK_INT((long)rgb[MT_TEXT], 0xF8F8F2L);
    CHECK_INT((long)rgb[MT_DIM], 0x6272A4L);
    CHECK_INT((long)rgb[MT_ACCENT], 0x8BE9FDL);
    CHECK_INT((long)rgb[MT_SELBOX], 0x44475AL);

    /* a palette that remaps only the accent leaves the other roles at their defaults */
    CHECK_INT(upconf_palette_parse("14,FFAA00", pal), 1);
    menu_theme_roles(pal, rgb);
    CHECK_INT((long)rgb[MT_ACCENT], 0xFFAA00L);
    CHECK_INT((long)rgb[MT_BG], 0x000000L);
    CHECK_INT((long)rgb[MT_SELBOX], 0x0000EEL);
}

/* Minimal menu mode, one reachability test: the upconf text selects it, the command builder sends
 * it to the drawn program, the geometry drops border and gaps, and the theme has two colours. */
static void minimal_mode_reaches_the_program(void)
{
    static const char conf[] = "menu-style = minimal\n";
    mg_font topaz = { 8, 8 };
    mg_screen drawn = { 640, 256, 22, 44, 0, 0, 0, 0 }, mini = { 640, 256, 22, 44, 1, 0, 0, 0 };
    mg_metrics md, mm;
    uc_u32 rgb[MT_ROLES];
    char out[96];
    int style, text = -1, px = 8, wd, wm, i;

    style = upconf_menu_style(conf, (long)strlen(conf));
    CHECK_INT(style, UC_MENU_MINIMAL);
    CHECK(cx_menu_cmd(out, sizeof(out), "UP-Term:bin", KA_MENU, "apps", style, &text));
    CHECK_STR(out, "\"UP-Term:bin/amarchy-gui\" menu apps");
    CHECK_INT(text, 0);

    mg_metrics_for(&md, &topaz, &drawn);
    mg_metrics_for(&mm, &topaz, &mini);
    CHECK(md.bx > 0 && md.by > 0 && md.icon_gap > 0 && md.chev_gap > 0);
    CHECK_INT(mm.bx, 0);
    CHECK_INT(mm.by, 0);
    CHECK_INT(mm.icon_gap, 0);
    CHECK_INT(mm.chev_gap, 0);
    CHECK_INT(mm.row_h, md.row_h);

    /* a label (31 letters, 16 pixels each) wider than the minimum window: the width follows the content */
    CHECK(parse("[menu]\napps.a = Short | run:a\n"
                "apps.b = A label of thirty one letters. | run:b\n"));
    px = 16;
    topaz.cw = 16;
    wd = mg_width_t(&km, "apps", &topaz, 16, 8, fixed_width, &px, &drawn, 1);
    wm = mg_width_t(&km, "apps", &topaz, 0, 16, fixed_width, &px, &mini, 1);
    CHECK(wm > md.min_w && wm < wd);

    /* two colours: the background roles share one value, the text roles the other */
    for (i = 0; i < MT_ROLES; i++)
        rgb[i] = 0x111111UL + (uc_u32)i;
    rgb[MT_BG] = 0x102030UL;
    rgb[MT_TEXT] = 0xA0B0C0UL;
    CHECK_INT(menu_theme_minimal(rgb), 1);
    CHECK_INT((long)rgb[MT_ACCENT], 0x102030L);
    CHECK_INT((long)rgb[MT_DIM], 0xA0B0C0L);
    CHECK_INT((long)rgb[MT_SELBOX], 0xA0B0C0L);
    rgb[MT_BG] = rgb[MT_TEXT] = 0x555555UL;
    CHECK_INT(menu_theme_minimal(rgb), 0);       /* equal colours: the caller uses screen pens */
}

/* the terminal draws its ground and text from the upconf bg and fg; the drawn menu follows them */
static void the_menu_takes_its_ground_and_text_from_the_upconf_fg_and_bg(void)
{
    uc_u32 pal[16], rgb[MT_ROLES];
    const uc_u32 fg = 0x01112233UL, bg = 0x01445566UL;

    /* both set, no palette: ground and text are theirs, the other roles stay on the palette's index */
    upconf_palette_parse(0, pal);
    menu_theme_roles_fb(pal, fg, bg, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x445566L);
    CHECK_INT((long)rgb[MT_TEXT], 0x112233L);
    CHECK_INT((long)rgb[MT_DIM], (long)vt_palette_rgb(0, 8));
    CHECK_INT((long)rgb[MT_ACCENT], (long)vt_palette_rgb(0, 14));
    CHECK_INT((long)rgb[MT_SELBOX], (long)vt_palette_rgb(0, 4));

    /* they win over a palette that remaps 0 and 7; the palette's accent still counts */
    upconf_palette_parse("0,282A36,7,F8F8F2,14,8BE9FD", pal);
    menu_theme_roles_fb(pal, fg, bg, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x445566L);
    CHECK_INT((long)rgb[MT_TEXT], 0x112233L);
    CHECK_INT((long)rgb[MT_ACCENT], 0x8BE9FDL);

    /* only one set: the other keeps the palette */
    menu_theme_roles_fb(pal, 0, bg, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x445566L);
    CHECK_INT((long)rgb[MT_TEXT], 0xF8F8F2L);
    menu_theme_roles_fb(pal, fg, 0, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x282A36L);
    CHECK_INT((long)rgb[MT_TEXT], 0x112233L);

    /* neither set: the old behaviour, palette 0 and 7, else the xterm colours */
    menu_theme_roles_fb(pal, 0, 0, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x282A36L);
    CHECK_INT((long)rgb[MT_TEXT], 0xF8F8F2L);
    upconf_palette_parse(0, pal);
    menu_theme_roles_fb(pal, 0, 0, rgb);
    CHECK_INT((long)rgb[MT_BG], 0x000000L);
    CHECK_INT((long)rgb[MT_TEXT], 0xE5E5E5L);
}

/* DOpus ARexx syntax: dopus5 v5.104 source/Program/rexx_dopus.c:587 (palette) and :616 (pens) */
static void dopus_theme_commands_use_the_theme_directly(void)
{
    gtheme th;
    char buf[256], tight[8];
    unsigned n;
    const char *want = "dopus set palette 0x1a1b26 0xa9b1d6 0x565f89 0x7aa2f7\n"
                       "dopus set pens files 1 0 dirs 3 0 selfiles 0 3 seldirs 0 3\n";

    /* D14: the Directory Opus listers take the theme's own bg/fg/dim/accent, not the menu roles */
    gt_builtin(&th);
    n = dopus_theme_commands(&th, buf, sizeof buf);
    CHECK_INT((long)n, (long)strlen(want));
    CHECK_STR(buf, want);

    /* a colour above 24 bits (0x01RRGGBB leaked in), a missing theme, too small a buffer: 0 and an empty string */
    th.dim = 0x01000000UL;
    strcpy(buf, "x");
    CHECK_INT((long)dopus_theme_commands(&th, buf, sizeof buf), 0L);
    CHECK_STR(buf, "");
    CHECK_INT((long)dopus_theme_commands(0, buf, sizeof buf), 0L);
    gt_builtin(&th);
    CHECK_INT((long)dopus_theme_commands(&th, tight, sizeof tight), 0L);
    CHECK_INT((long)dopus_theme_commands(&th, buf, n), 0L);
    CHECK_INT((long)dopus_theme_commands(&th, buf, n + 1), (long)n);
}

/* ---- index_core: the app index's cache rows (plan 2026-10-23-amarchy-app-index.md, X1) ---- */

static void an_index_row_has_kind_label_and_path(void)
{
    ix_row r;
    char buf[400];
    CHECK(ix_parse_row("app\tDirectory Opus\tWork:Apps/DOpus/DirectoryOpus", &r));
    CHECK_INT(r.kind, IX_KIND_APP);
    CHECK_STR(r.label, "Directory Opus");
    CHECK_STR(r.path, "Work:Apps/DOpus/DirectoryOpus");
    CHECK(ix_parse_row("game\tTurrican II\tGames:T/Turrican2.info\r\n", &r));
    CHECK_INT(r.kind, IX_KIND_GAME);
    CHECK_STR(r.path, "Games:T/Turrican2.info");
    CHECK_INT(ix_format_row(&r, buf, sizeof buf), (long)strlen("game\tTurrican II\tGames:T/Turrican2.info\n"));
    CHECK_STR(buf, "game\tTurrican II\tGames:T/Turrican2.info\n");
    CHECK_INT(ix_format_row(&r, buf, 10), -1);
}

static void a_bad_index_row_is_rejected(void)
{
    ix_row r;
    char longlabel[200], line[400];
    CHECK(!ix_parse_row("", &r));
    CHECK(!ix_parse_row("app\tOnly two fields", &r));
    CHECK(!ix_parse_row("tool\tLabel\tWork:x", &r));
    CHECK(!ix_parse_row("App\tLabel\tWork:x", &r));
    CHECK(!ix_parse_row("app\t\tWork:x", &r));
    CHECK(!ix_parse_row("app\tLabel\t", &r));
    CHECK(!ix_parse_row("app\tLabel\tWork:x\textra", &r));
    CHECK(!ix_parse_row("app Label Work:x", &r));
    memset(longlabel, 'a', sizeof longlabel - 1);
    longlabel[sizeof longlabel - 1] = 0;
    strcpy(line, "app\t");
    strcat(line, longlabel);
    strcat(line, "\tWork:x");
    CHECK(!ix_parse_row(line, &r));
}

static void an_index_label_is_the_icon_file_name_without_info(void)
{
    char l[IX_LABEL];
    ix_label_from_name("Multiview.info", l, sizeof l);
    CHECK_STR(l, "Multiview");
    ix_label_from_name("Foo Bar.INFO", l, sizeof l);
    CHECK_STR(l, "Foo Bar");
    ix_label_from_name("Tool", l, sizeof l);
    CHECK_STR(l, "Tool");
    ix_label_from_name(".info", l, sizeof l);
    CHECK_STR(l, ".info");
}

static void network_volumes_are_skipped_unless_listed_in_amarchy_index_volumes(void)
{
    /* handler names measured on the A1200: SMB0 L:smb2-handler, FAT0 l:fat95, DH0 none (a startup message) */
    CHECK(ix_net_handler("L:smb2-handler"));
    CHECK(ix_net_handler("l:SMBFS"));
    CHECK(ix_net_handler("nfs-handler"));
    CHECK(!ix_net_handler("l:fat95"));
    CHECK(!ix_net_handler(""));
    CHECK(!ix_net_handler(0));
    CHECK(!ix_volume_wanted("spotUP", 1, ""));
    CHECK(!ix_volume_wanted("spotUP", 1, "Music, UpRough"));
    CHECK(ix_volume_wanted("Music", 1, "spotUP, music ,UpRough:"));
    CHECK(ix_volume_wanted("UpRough", 1, "spotUP, music ,UpRough:"));
    CHECK(!ix_volume_wanted("Mus", 1, "Music"));
    CHECK(ix_volume_wanted("System", 0, ""));        /* a local volume is always scanned */
}

static void the_skip_list_skips_by_whole_path_prefix(void)
{
    CHECK(ix_skip_match("Work:Games", "Work:Games/Turrican/Turrican"));
    CHECK(ix_skip_match("Work:Games", "Work:Games"));
    CHECK(!ix_skip_match("Work:Games", "Work:Games2/x"));
    CHECK(!ix_skip_match("Work:Games", "Work:Gam"));
    CHECK(ix_skip_match("Old:, Work:Games ,Tmp:x", "Old:Anything/y"));
    CHECK(ix_skip_match("Old:, Work:Games ,Tmp:x", "tmp:X/z"));
    CHECK(!ix_skip_match("Old:, Work:Games ,Tmp:x", "Work:Tools/y"));
    CHECK(!ix_skip_match("", "Work:Tools/y"));
    CHECK(!ix_skip_match(",, ,", "Work:Tools/y"));
}

static void equal_index_labels_get_their_volume_as_suffix_and_rows_sort_by_label(void)
{
    ix_row r[4];
    memset(r, 0, sizeof r);
    r[0].kind = IX_KIND_APP; strcpy(r[0].label, "Zip");    strcpy(r[0].path, "Work:Z/Zip");
    r[1].kind = IX_KIND_APP; strcpy(r[1].label, "editor"); strcpy(r[1].path, "Sys:E/Editor");
    r[2].kind = IX_KIND_APP; strcpy(r[2].label, "Editor"); strcpy(r[2].path, "Work:E/Editor");
    r[3].kind = IX_KIND_APP; strcpy(r[3].label, "Clock");  strcpy(r[3].path, "Sys:C/Clock");
    ix_sort_dedupe(r, 4);
    CHECK_STR(r[0].label, "Clock");
    CHECK_STR(r[1].label, "editor");
    CHECK_STR(r[2].label, "Editor");
    CHECK_STR(r[3].label, "Zip");
    CHECK_STR(r[0].suffix, "");
    CHECK_STR(r[1].suffix, "Sys");
    CHECK_STR(r[2].suffix, "Work");
    CHECK_STR(r[3].suffix, "");
}

static void a_menu_row_wins_over_an_index_row_with_the_same_label(void)
{
    ix_row r[3];
    memset(r, 0, sizeof r);
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\napps.upkg = Add-ons | run:upkg\n"));
    strcpy(r[0].label, "Add-ons"); strcpy(r[0].path, "Work:A/Addons");
    strcpy(r[1].label, "Clock");   strcpy(r[1].path, "Sys:C/Clock");
    strcpy(r[2].label, "tmux");    strcpy(r[2].path, "Work:T/tmux");
    CHECK_INT(ix_drop_shadowed(&km, "apps", r, 3), 1);
    CHECK_STR(r[0].label, "Clock");
    /* another node's rows do not shadow */
    memset(r, 0, sizeof r);
    strcpy(r[0].label, "Tmux"); strcpy(r[0].path, "Work:T/tmux");
    CHECK_INT(ix_drop_shadowed(&km, "window", r, 1), 1);
}

static const char *const index_cache =
    "app\tClock\tSys:Tools/Clock\n"
    "app\tTmux\tWork:T/tmux\n"
    "game\tTurrican 2\tWork:Games/Turrican2\n"
    "bad row\n"
    "app\tEditor\tSys:E/Editor\n"
    "app\tEditor\tWork:E/Editor\n";

static int level_has(const mn_item *it, int n, const char *label)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].label, label))
            return 1;
    return 0;
}

/* the item arrays of the small index tests: the [menu] rows plus a few index rows */
#define TEST_LEVEL_MAX (MN_MAX_ITEMS + 64)

static void every_cache_row_reaches_the_apps_level_past_256(void)
{
    /* a big install (the rig has 258 apps plus games): rows late in the file were once dropped */
    static ix_index x;
    static char text[400 * 40];
    mn_item *it;
    int i, n, len = 0;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    for (i = 0; i < 399; i++)
        len += snprintf(text + len, sizeof text - len, "app\tApp%03d\tWork:App%03d\n", i, i);
    snprintf(text + len, sizeof text - len, "app\tZed Last\tWork:Last\n");
    ix_load_text(&km, &x, text);
    CHECK_INT(x.n, 400);
    CHECK(ix_level_cap(&x) >= MN_MAX_ITEMS + 400);
    it = malloc(ix_level_cap(&x) * sizeof *it);
    n = ix_level(&km, &x, "apps", "", it, ix_level_cap(&x));
    CHECK_INT(n, 401);                                    /* Tmux plus every index row */
    CHECK(level_has(it, n, "Zed Last"));
    free(it);
    ix_free(&x);
    CHECK_INT(x.n, 0);
}

/* GROUP-DRAWER-ICON: every item of the levels amarchy-gui draws resolves to an id with a glyph; the kind
 * groups keep their own, a scanned drawer takes "drawer", an index row the glyph of its level */
static void menu_item_id_cases(void)
{
    static ix_index x;
    static char file[32768];
    mn_item it[TEST_LEVEL_MAX];
    char id[KM_ID * 2 + 2];
    long fn = slurp("dist/keymap", file, sizeof(file));
    const char *const nodes[] = { "", "games", "apps" };
    int ni, i, n, ran = 0, want = 0, got_utils = 0, got_idx_games = 0, got_idx_apps = 0;
    CHECK(fn > 0 && keymap_parse(&km, file, fn));
    ix_load_text(&km, &x,
                 "app\tClock\tWork:Tools/Clock\n"
                 "app\tEdit\tWork:Dev/Edit\n"
                 "app\tRootApp\tWork:RootApp\n"
                 "game\tTurrican 2\tWork:Games/Turrican2\n"
                 "game\tState of the Art\tWork:Demos/Soa\n"
                 "app\tSong\tWork:Music/Song\n");
    for (ni = 0; ni < 3; ni++) {
        n = ix_level(&km, &x, nodes[ni], "", it, TEST_LEVEL_MAX);
        want += n;
        for (i = 0; i < n; i++) {
            mi_item_id(nodes[ni], &it[i], id, sizeof id);
            ran++;
            if (!mi_find(id))
                printf("  no icon for '%s' (node '%s', row '%s')\n", id, nodes[ni], it[i].label);
            CHECK(mi_find(id) != 0);
            if (MN_IS_INDEX(&it[i])) {
                if (ni == 1) { CHECK_STR(id, "games"); got_idx_games++; }
                if (ni == 2) { CHECK_STR(id, "apps"); got_idx_apps++; }
            } else if (MN_IS_GROUP(&it[i]) && ni == 0) {
                if (!strcmp(it[i].label, "Games")) CHECK_STR(id, "games");
                if (!strcmp(it[i].label, "Demos")) CHECK_STR(id, "demos");
                if (!strcmp(it[i].label, "Music")) CHECK_STR(id, "music");
                if (!strcmp(it[i].label, "Style")) CHECK_STR(id, "style");
            } else if (MN_IS_GROUP(&it[i]) && ni == 2) {
                CHECK_STR(id, "drawer");               /* apps.tools, apps.dev */
                got_utils++;
            }
        }
    }
    CHECK_INT(ran, want);                              /* the loop ran over every item mn level returned */
    CHECK(want > 6);
    CHECK_INT(got_utils, 2);
    CHECK_INT(got_idx_games, 1);
    CHECK_INT(got_idx_apps, 1);                        /* the volume-root app */
    /* an index row at an unknown node, and a static leaf */
    memset(&it[0], 0, sizeof it[0]);
    it[0].row = MN_ROW_INDEX;
    mi_item_id("foo", &it[0], id, sizeof id);
    CHECK_STR(id, "apps");
    n = mn_children(&km, "style", it, TEST_LEVEL_MAX);
    for (i = 0; i < n; i++)
        if (!MN_IS_GROUP(&it[i]) && !strcmp(it[i].label, "Amarchy theme")) {
            mi_item_id("style", &it[i], id, sizeof id);
            CHECK_STR(id, "style.theme");
            ran = -1;
        }
    CHECK_INT(ran, -1);
    ix_free(&x);
}

static void the_apps_level_lists_index_apps_after_merging(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n, base, i, idx = -1;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\napps.upkg = Add-ons | run:upkg\n"));
    ix_load_text(&km, &x, index_cache);
    base = mn_children(&km, "apps", it, TEST_LEVEL_MAX);
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    CHECK_INT(base, 2);
    CHECK_INT(n, 4);                                   /* 2 menu rows + the drawers Tools and E */
    CHECK_STR(it[0].label, "Tmux");                    /* the keymap rows come first */
    CHECK(level_has(it, n, "Tools") && level_has(it, n, "E"));
    n = ix_level(&km, &x, "apps.Tools", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 1);
    CHECK_STR(it[0].label, "Clock");
    base = 0;
    CHECK(MN_IS_INDEX(&it[0]));
    idx = it[0].ix;
    CHECK(idx >= 0 && !strcmp(x.rows[idx].path, "Sys:Tools/Clock"));
    n = ix_level(&km, &x, "apps.E", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 2);
    CHECK(level_has(it, n, "Editor (Sys)"));           /* a shared label carries its volume */
    CHECK(level_has(it, n, "Editor (Work)"));
    CHECK(!level_has(it, n, "Turrican 2"));
    for (i = base; i < n; i++)
        CHECK(MN_IS_INDEX(&it[i]));                    /* the action kind X5 launches */
}

static void an_index_row_launches_its_tool_path_and_a_rescan_row_ships_in_the_apps_menu(void)
{
    static ix_index x;
    static char buf[KM_MAX_BIND * 100];
    mn_item it[TEST_LEVEL_MAX];
    char path[IX_PATH];
    int n, i, clock = -1, rescan = -1;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, index_cache);
    n = ix_level(&km, &x, "apps.Tools", "", it, TEST_LEVEL_MAX);
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].label, "Clock"))
            clock = i;
    CHECK(clock >= 0);
    CHECK_INT(ix_launch_path(&x, &it[clock], path, sizeof path), 1);
    CHECK_STR(path, "Sys:Tools/Clock");                /* the tool, no .info */
    CHECK_INT(ix_icon_path(&x, &it[clock], path, sizeof path), 1);   /* an app row is a file: its icon path */
    CHECK_STR(path, "Sys:Tools/Clock");
    {   /* a clip row is no file either (AM-GTHEME-ICON-REQ) */
        const char *cl[1] = { "copied" };
        mn_item ci[4];
        CHECK_INT(ix_add_clips(&x, cl, 1), 1);
        CHECK_INT(ix_level(&km, &x, IX_CLIPS_NODE, "", ci, 4), 1);
        CHECK_INT(ix_icon_path(&x, &ci[0], path, sizeof path), 0);
        CHECK_INT(ix_launch_path(&x, &ci[0], path, sizeof path), 1);
    }
    {
        mn_item top[TEST_LEVEL_MAX];
        ix_level(&km, &x, "apps", "", top, TEST_LEVEL_MAX);
        CHECK_INT(ix_launch_path(&x, &top[0], path, sizeof path), 0);   /* a menu row is not launched this way */
    }
    CHECK_INT(ix_launch_path(&x, &it[clock], path, 8), 0);         /* a path that does not fit */
    /* the cache is read again when ix_load_text runs again: a rescan shows on the next menu open */
    ix_load_text(&km, &x, "app\tZed\tWork:Zed\n");
    CHECK_INT(x.n, 1);
    /* the shipped menu has the Rescan apps row, and it starts the scanner */
    n = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nmenu; i++)
        if (!strcmp(km.menu[i].id, "settings.amarchy.rescan"))
            rescan = i;
    CHECK(rescan >= 0);
    CHECK_STR(km.menu[rescan].label, "Rescan apps");
    CHECK_STR(km.menu[rescan].arg, "amarchy-index");
    CHECK_INT(km.menu[rescan].kind, KA_SCAN);            /* in the background, and the menu watches it */
    CHECK_INT((int)lf_plan(KA_DETACH), (int)LF_DETACH);
    CHECK_INT((int)lf_plan(KA_SCAN), (int)LF_SCAN);
    CHECK_INT(keymap_action_scope(KA_DETACH), KS_BOTH);
    CHECK_INT(keymap_action_scope(KA_SCAN), KS_BOTH);
}

static void a_launch_request_for_the_resident_amarchy_carries_the_path_and_refuses_a_bad_one(void)
{
    lf_launch b;
    char longp[LF_LAUNCH_PATH + 8];
    CHECK_INT(lf_launch_fill(&b, "Sys:Tools/Clock"), 1);
    CHECK_STR(b.path, "Sys:Tools/Clock");
    CHECK_STR(b.err, "");
    CHECK_INT(lf_launch_fill(&b, ""), 0);
    memset(longp, 'x', sizeof longp - 1);
    longp[sizeof longp - 1] = 0;
    CHECK_INT(lf_launch_fill(&b, longp), 0);
    longp[LF_LAUNCH_PATH - 1] = 0;                     /* the longest that fits */
    CHECK_INT(lf_launch_fill(&b, longp), 1);
}

static void a_menu_row_hides_an_index_app_with_the_same_label(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n, i, tmux = 0;
    CHECK(parse("[menu]\napps.tmux = tmux | run:tmux\n"));
    ix_load_text(&km, &x, index_cache);
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    for (i = 0; i < n; i++)
        if (!strcasecmp(it[i].label, "tmux"))
            tmux++;
    CHECK_INT(tmux, 1);
    CHECK(!MN_IS_INDEX(&it[0]));
}

static const char owner_index[] =
    "app\tOctaMED\tRushhours:Music/OctaMED/OctaMED\n"
    "app\tProTracker\tRushhours:Music/ProTracker\n"
    "game\tAlien Breed disk\tRushhours:Music/AB/AlienBreed\n"
    "game\tTurrican 2\tRushhours:Games/Turrican2/Turrican2\n"
    "app\tMultiView\tRushhours:Games/Tools/MView\n"
    "app\tPalette\tSystem:Prefs/Palette\n"
    "app\tScreenMode\tSystem:Prefs/ScreenMode\n"
    "app\tClock\tSys:Tools/Clock\n"
    "game\tState of the Art\tRushhours:Scene/SotA/SotA\n"
    "game\tDesert Dream\tRushhours:Scene/DD/DD\n";

static void music_disks_leave_games_and_get_a_flat_music_level(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n, i;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, owner_index);
    n = ix_level(&km, &x, "", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Apps") && level_has(it, n, "Games") && level_has(it, n, "Demos") &&
          level_has(it, n, "Music") && level_has(it, n, "Settings"));
    n = ix_level(&km, &x, "music", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Alien Breed disk") && level_has(it, n, "OctaMED") && level_has(it, n, "ProTracker"));
    for (i = 0; i < n; i++)
        CHECK(!MN_IS_GROUP(&it[i]));                   /* one drawer: no lone "Music" group */
    n = ix_level(&km, &x, "games", "", it, TEST_LEVEL_MAX);
    CHECK(!level_has(it, n, "Alien Breed disk"));
    CHECK(level_has(it, n, "Turrican 2") || level_has(it, n, "Games"));
    n = ix_level(&km, &x, "demos", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "State of the Art") && level_has(it, n, "Desert Dream"));
}

static void prefs_tools_get_a_flat_settings_level_and_leave_apps(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, owner_index);
    n = ix_level(&km, &x, "settings", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Palette") && level_has(it, n, "ScreenMode"));
    CHECK(!MN_IS_GROUP(&it[0]));
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    CHECK(!level_has(it, n, "Prefs") && !level_has(it, n, "Palette") && !level_has(it, n, "Music"));
    n = ix_level(&km, &x, "apps.Tools", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Clock"));
}

static void an_index_without_prefs_music_or_scene_rows_keeps_apps_and_games_only(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, "app\tClock\tSys:Tools/Clock\ngame\tTurrican 2\tWork:Games/T2\n");
    n = ix_level(&km, &x, "", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Apps") && level_has(it, n, "Games"));
    CHECK(!level_has(it, n, "Demos") && !level_has(it, n, "Music") && !level_has(it, n, "Settings"));
}

static void extra_music_drawers_from_the_upconf_move_games_to_music(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_set_drawers("Chiptunes, Trackers", "Showcases");
    ix_load_text(&km, &x, "game\tX\tWork:chiptunes/X\ngame\tY\tWork:Showcases/Y\n");
    ix_set_drawers(0, 0);
    n = ix_level(&km, &x, "music", "", it, TEST_LEVEL_MAX);
    CHECK(n > 0 && level_has(it, n, "X"));
    n = ix_level(&km, &x, "demos", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Y"));
}

static void system_drawers_vanish_from_the_apps_level(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, "app\tDir\tSystem:C/Dir\napp\tFormat\tSystem:Storage/Format\n"
                 "app\tClock\tSystem:Utilities/Clock\napp\tCommodities\tSystem:WBStartup/Exchange\n"
                 "app\tMUI\tWork:MUI/Setup\napp\tCED\tWork:Dev/CED\napp\tPalette\tSystem:Prefs/Palette\n"
                 "game\tLemmings\tGames:Puzzle/Lemmings\n");
    CHECK_INT(x.n, 3);                                  /* CED, Palette, Lemmings; the five system rows are dropped */
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    CHECK(!level_has(it, n, "C") && !level_has(it, n, "Storage") && !level_has(it, n, "Utilities"));
    CHECK(!level_has(it, n, "WBStartup") && !level_has(it, n, "MUI"));
    CHECK(!level_has(it, n, "Dir") && !level_has(it, n, "Clock"));
    CHECK(level_has(it, n, "CED"));         /* the one remaining category is shown flat */
    n = ix_level(&km, &x, "apps", "Clock", it, TEST_LEVEL_MAX);
    CHECK(!level_has(it, n, "Clock"));
    n = ix_level(&km, &x, "settings", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Palette"));
    n = ix_level(&km, &x, "games", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Lemmings"));        /* the one game category is shown flat */
}

static void the_shipped_settings_amarchy_group_holds_rescan_preferences_font_and_edit(void)
{
    static char buf[32768];
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n, g, i, found = 0;
    long len = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(len > 0 && keymap_parse(&km, buf, len));
    check_row("settings.amarchy.rescan", KA_SCAN, "amarchy-index");
    /* run: opened an UP-Term window beside the Prefs window (A1200 2026-10-26) */
    check_row("settings.amarchy.preferences", KA_DETACH, "C:UP-Term-Prefs AMARCHY");
    check_row("settings.amarchy.font", KA_DETACH, "C:UP-Term-Prefs AMARCHY");
    check_row("settings.amarchy.edit", KA_RUN, "nvim ENV:up-term/keymap");
    CHECK(shipped_row("apps.rescan") == 0);
    ix_load_text(&km, &x, "app\tClock\tSys:Tools/Clock\napp\tPalette\tSystem:Prefs/Palette\n");
    n = ix_level(&km, &x, "", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Settings"));
    CHECK(root_has_group("Settings", 1));
    n = ix_level(&km, &x, "settings", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Amarchy") && level_has(it, n, "Palette"));
    for (g = 0; g < n; g++)
        if (!strcmp(it[g].label, "Amarchy"))
            found = 1;
    CHECK(found);
    n = ix_level(&km, &x, "settings.amarchy", "", it, TEST_LEVEL_MAX);
    for (g = 0, i = 0; i < sd_nsettings; i++)
        g += sd_settings[i].page == PG_PAGE_AMARCHY;
    CHECK_INT(n, g + 6); /* the page's table rows plus the six static rows (rescan, preferences, font, edit, two AI agent picks) */
    CHECK(level_has(it, n, "Rescan apps") && level_has(it, n, "Amarchy preferences") &&
          level_has(it, n, "Menu font") && level_has(it, n, "Edit menu") &&
          level_has(it, n, "AI Agent: Claude") && level_has(it, n, "AI Agent: pi"));
    for (i = 0; i < n; i++)
        CHECK(!MN_IS_GROUP(&it[i]));
}

static void hidden_drawers_from_the_upconf_vanish_too(void)
{
    static ix_index x;
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_set_hidden_drawers("Tools");
    ix_load_text(&km, &x, "app\tFoo\tSys:Tools/Foo\napp\tCED\tWork:Dev/CED\n");
    n = x.n;
    ix_set_hidden_drawers(0);
    CHECK_INT(n, 1);
    ix_load_text(&km, &x, "app\tFoo\tSys:Tools/Foo\napp\tCED\tWork:Dev/CED\n");
    CHECK_INT(x.n, 2);
}

static void the_games_level_lists_index_games_and_the_root_offers_the_group(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, index_cache);
    n = ix_level(&km, &x, "games.Games", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 1);
    CHECK_STR(it[0].label, "Turrican 2");
    CHECK(MN_IS_INDEX(&it[0]));
    n = ix_level(&km, &x, "", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Games"));
    CHECK(!level_has(it, n, "Turrican 2"));            /* without a filter the root stays short */
}

static int level_group(const mn_item *it, int n, const char *label)
{
    int i;
    for (i = 0; i < n; i++)
        if (MN_IS_GROUP(&it[i]) && !strcmp(it[i].label, label))
            return i;
    return -1;
}

static void the_apps_and_games_levels_group_by_the_first_drawer_under_the_volume(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.claude = Claude | run:claude\n"));
    ix_load_text(&km, &x,
                 "app\tClock\tSys:Office/Clock\n"
                 "app\tCED\tWork:Dev/Editors/CED\n"
                 "app\tShell\tSys:Tools/Shell\n"
                 "app\tShell\tWork:Tools/Shell\n"
                 "app\tFlat\tWork:Flat\n"
                 "game\tTurrican 2\tWork:Games/Turrican2\n"
                 "game\tBoulder\tSys:Fun/Boulder\n"
                 "game\tRootGame\tWork:RootGame\n");
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 5);                                   /* Claude, Dev, Office, Tools, Flat */
    CHECK(!MN_IS_GROUP(&it[0]));                       /* the [menu] row stays at the Apps level */
    CHECK_STR(it[0].label, "Claude");
    CHECK_INT(level_group(it, n, "Dev"), 1);       /* categories alphabetical, merged by drawer name */
    CHECK_INT(level_group(it, n, "Office"), 2);
    CHECK_INT(level_group(it, n, "Tools"), 3);
    CHECK_STR(it[4].label, "Flat");                    /* the volume-root app is flat, below the categories */
    CHECK(MN_IS_INDEX(&it[4]));
    CHECK(!level_has(it, n, "Clock"));                 /* categorised apps are not at the Apps level */
    n = ix_level(&km, &x, "apps.Tools", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 2);                                   /* Sys:Tools and Work:Tools merged, suffix kept */
    CHECK_STR(it[0].label, "Shell (Sys)");
    CHECK_STR(it[1].label, "Shell (Work)");
    n = ix_level(&km, &x, "apps.dev", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 1);                                   /* a single-app category is kept; case-insensitive node */
    CHECK_STR(it[0].label, "CED");
    n = ix_level(&km, &x, "games", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 3);
    CHECK_INT(level_group(it, n, "Fun"), 0);
    CHECK_INT(level_group(it, n, "Games"), 1);
    CHECK_STR(it[2].label, "RootGame");
    n = ix_level(&km, &x, "games.Games", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 1);
    CHECK_STR(it[0].label, "Turrican 2");
    n = ix_level(&km, &x, "apps", "cl", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Clock") && level_has(it, n, "Flat"));   /* a filter at Apps searches every category */
    CHECK(level_has(it, n, "Office"));                 /* the group stays, so its name can match too */
    n = ix_level(&km, &x, "", "shell", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Shell (Sys)"));            /* the root filter still finds apps in categories */
}

static void a_missing_or_empty_cache_adds_no_index_rows(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX], base[TEST_LEVEL_MAX];
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, "");
    CHECK_INT(ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX), mn_children(&km, "apps", base, TEST_LEVEL_MAX));
    CHECK_INT(ix_level(&km, &x, "games", "", it, TEST_LEVEL_MAX), 0);
    CHECK_INT(ix_level(&km, &x, "", "tur", it, TEST_LEVEL_MAX), mn_children(&km, "", base, TEST_LEVEL_MAX));
    ix_load_file(&km, &x, "/nonexistent/amarchy/apps");
    CHECK_INT(x.n, 1);                                  /* no cache rows; only the clipboard history placeholder */
    CHECK_INT(x.rows[0].kind, IX_KIND_CLIP);
    ix_load_text(&km, &x, 0);
    CHECK_INT(x.n, 0);
}

static void root_search_finds_an_index_game_and_an_index_app(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int idx[TEST_LEVEL_MAX], n, c;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, index_cache);
    n = ix_level(&km, &x, "", "turr", it, TEST_LEVEL_MAX);
    c = mn_filter(it, n, "turr", idx, TEST_LEVEL_MAX);
    CHECK_INT(c, 1);
    CHECK_STR(it[idx[0]].label, "Turrican 2");
    CHECK(MN_IS_INDEX(&it[idx[0]]));
    n = ix_level(&km, &x, "", "clo", it, TEST_LEVEL_MAX);
    c = mn_filter(it, n, "clo", idx, TEST_LEVEL_MAX);
    CHECK_INT(c, 1);
    CHECK_STR(it[idx[0]].label, "Clock");
    n = ix_level(&km, &x, "", "zzz", it, TEST_LEVEL_MAX);
    CHECK_INT(mn_filter(it, n, "zzz", idx, TEST_LEVEL_MAX), 0);
}

static void the_app_index_core_behaves(void)
{
    an_index_row_has_kind_label_and_path();
    every_cache_row_reaches_the_apps_level_past_256();
    a_bad_index_row_is_rejected();
    an_index_label_is_the_icon_file_name_without_info();
    the_skip_list_skips_by_whole_path_prefix();
    network_volumes_are_skipped_unless_listed_in_amarchy_index_volumes();
    equal_index_labels_get_their_volume_as_suffix_and_rows_sort_by_label();
    a_menu_row_wins_over_an_index_row_with_the_same_label();
    the_apps_level_lists_index_apps_after_merging();
    menu_item_id_cases();
    the_scan_record_names_the_last_path_and_the_where_text_tells_running_stuck_died_and_done();
    an_index_row_launches_its_tool_path_and_a_rescan_row_ships_in_the_apps_menu();
    a_launch_request_for_the_resident_amarchy_carries_the_path_and_refuses_a_bad_one();
    a_menu_row_hides_an_index_app_with_the_same_label();
    the_games_level_lists_index_games_and_the_root_offers_the_group();
    music_disks_leave_games_and_get_a_flat_music_level();
    prefs_tools_get_a_flat_settings_level_and_leave_apps();
    an_index_without_prefs_music_or_scene_rows_keeps_apps_and_games_only();
    extra_music_drawers_from_the_upconf_move_games_to_music();
    system_drawers_vanish_from_the_apps_level();
    the_shipped_settings_amarchy_group_holds_rescan_preferences_font_and_edit();
    hidden_drawers_from_the_upconf_vanish_too();
    the_apps_and_games_levels_group_by_the_first_drawer_under_the_volume();
    a_missing_or_empty_cache_adds_no_index_rows();
    root_search_finds_an_index_game_and_an_index_app();
}

/* The icon cache file and the dynamic slot count (the level's row count sets the slots). */
static void the_icon_cache_file_round_trips_and_rejects_bad_buffers(void)
{
    static ic_cache c;
    static unsigned char buf[2048];
    unsigned char pens[6] = {1, 2, 3, 4, 5, 6}, mask[6] = {1, 0, 1, 1, 0, 1};
    ic_dec d[4];
    long n, h;
    char p[16];
    int i, a;
    h = ic_file_head(buf, sizeof buf, 32, 16, 4);
    CHECK(h > 0);
    n = h;
    n += ic_file_entry(buf + n, sizeof buf - n, "Work:A", 3, 2, 1, 2, pens, mask);
    n += ic_file_entry(buf + n, sizeof buf - n, "Work:B", 3, 2, 0, 0, mask, pens);
    CHECK_INT(ic_file_decode(buf, n, 32, 16, 4, d, 4), 2);
    CHECK_STR(d[0].path, "Work:A");
    CHECK_INT(d[0].dw, 3); CHECK_INT(d[0].dh, 2); CHECK_INT(d[0].dx, 1); CHECK_INT(d[0].dy, 2);
    CHECK_INT(d[0].pens[5], 6); CHECK_INT(d[0].mask[1], 0);
    CHECK_STR(d[1].path, "Work:B");
    CHECK_INT(d[1].pens[0], 1); CHECK_INT(d[1].mask[0], 1); CHECK_INT(d[1].mask[1], 2);
    /* a header that does not match the run's box or depth: the whole file is ignored */
    CHECK_INT(ic_file_decode(buf, n, 33, 16, 4, d, 4), 0);
    CHECK_INT(ic_file_decode(buf, n, 32, 17, 4, d, 4), 0);
    CHECK_INT(ic_file_decode(buf, n, 32, 16, 5, d, 4), 0);
    buf[4] = 2;
    CHECK_INT(ic_file_decode(buf, n, 32, 16, 4, d, 4), 0);
    buf[4] = 1;
    /* truncated anywhere inside the entries: no entries at all */
    CHECK_INT(ic_file_decode(buf, n - 1, 32, 16, 4, d, 4), 0);
    CHECK_INT(ic_file_decode(buf, h + 3, 32, 16, 4, d, 4), 0);
    CHECK_INT(ic_file_decode(buf, 5, 32, 16, 4, d, 4), 0);
    buf[0] = 'X';
    CHECK_INT(ic_file_decode(buf, n, 32, 16, 4, d, 4), 0);
    buf[0] = 'A';
    /* an oversize image is refused by the encoder and the decoder alike */
    CHECK(ic_file_entry(buf + h, sizeof buf - h, "Work:C", 65, 1, 0, 0, pens, mask) < 0);
    /* slots follow the caller's count: slots + 1 claims evict the oldest */
    memset(&c, 0, sizeof c);
    CHECK_INT(ic_init(&c, 300), 1);
    for (i = 0; i < 300; i++) {
        strcpy(p, "Work:");
        p[5] = (char)('a' + i % 26);
        p[6] = (char)('a' + i / 26);
        p[7] = 0;
        a = ic_claim(&c, p);
        CHECK(a >= 0 && a < 300);
        ic_set(&c, a, IC_READY);
    }
    CHECK(ic_find(&c, "Work:aa") >= 0);
    CHECK(ic_find(&c, "Work:ba") >= 0);
    a = ic_claim(&c, "Work:new");
    CHECK(a >= 0 && a < 300);
    ic_set(&c, a, IC_READY);
    CHECK_INT(ic_find(&c, "Work:aa") >= 0, 1);
    CHECK_INT(ic_find(&c, "Work:ca"), -1);
    ic_free(&c);
}

/* AM-IDX-ICON I1: the icon cache's slots and the fit of an image into the icon column. */
static void app_icons_are_cached_per_path_bounded_and_fitted(void)
{
    static ic_cache c;
    char p[IX_PATH + 1];
    int i, a, b, first, dw, dh, dx, dy;
    memset(&c, 0, sizeof c);
    CHECK_INT(ic_init(&c, IC_SLOTS), 1);
    CHECK_INT(ic_find(&c, "Work:A"), -1);
    a = ic_claim(&c, "Work:A");
    CHECK(a >= 0);
    CHECK_INT(ic_find(&c, "Work:A"), -1);              /* claimed, not yet loaded: not a hit */
    ic_set(&c, a, IC_READY);
    CHECK_INT(ic_find(&c, "Work:A"), a);
    b = ic_claim(&c, "Work:B");
    ic_set(&c, b, IC_FAILED);
    CHECK(b != a);
    CHECK_INT(ic_find(&c, "Work:B"), b);               /* a failure is remembered: not tried again */
    CHECK_INT(c.state[b], IC_FAILED);
    /* the cache is bounded: filling it past IC_SLOTS evicts the path used longest ago */
    for (i = 0; i < IC_SLOTS - 2; i++) {
        strcpy(p, "Work:F");
        p[6] = (char)('a' + i);
        p[7] = 0;
        ic_set(&c, ic_claim(&c, p), IC_READY);
    }
    CHECK_INT(ic_find(&c, "Work:A"), a);               /* touched: now newer than Work:B */
    first = ic_claim(&c, "Work:New");
    CHECK_INT(first, b);                               /* Work:B was the oldest */
    ic_set(&c, first, IC_READY);
    CHECK_INT(ic_find(&c, "Work:B"), -1);
    CHECK_INT(ic_find(&c, "Work:A"), a);
    for (i = 0; i < 4 * IC_SLOTS; i++) {
        strcpy(p, "Work:G");
        p[6] = (char)('a' + i % 26);
        p[7] = (char)('a' + i / 26);
        p[8] = 0;
        CHECK(ic_claim(&c, p) < IC_SLOTS);
    }
    memset(p, 'x', sizeof p);
    p[IX_PATH] = 0;
    CHECK_INT(ic_claim(&c, p), -1);
    ic_free(&c);
    /* ic_fit: aspect kept, centred, up or down */
    CHECK_INT(ic_fit(46, 22, 32, 16, &dw, &dh, &dx, &dy), 1);
    CHECK_INT(dw, 32);
    CHECK_INT(dh, 15);                                 /* 22 * 32 / 46 */
    CHECK_INT(dx, 0);
    CHECK_INT(dy, 0);
    CHECK_INT(ic_fit(30, 30, 64, 32, &dw, &dh, &dx, &dy), 1);
    CHECK_INT(dh, 32);
    CHECK_INT(dw, 32);                                 /* square image in a wide box: centred sideways */
    CHECK_INT(dx, 16);
    CHECK_INT(ic_fit(100, 1, 16, 16, &dw, &dh, &dx, &dy), 1);
    CHECK_INT(dh, 1);                                  /* never 0 */
    CHECK_INT(ic_fit(0, 10, 16, 16, &dw, &dh, &dx, &dy), 0);
}
