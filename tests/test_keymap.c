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
#include "../amarchy/menu_core.h"
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
    static char file[16384], ids[16384];
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
    CHECK(km.nbind >= 30 && km.nbind <= 60);
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
    char buf[16384];
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
    CHECK(comm_is("l", MENU_CLAUDE, 0));
    CHECK(comm_is("-", MENU_CLEAR_SCREEN, 0));
}

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
        { KA_OTHERMENU,   UC_MENU_GRAPHICAL, "",     "\"UP-Term:bin/amarchy-menu\" menu root", 1 },
        { KA_OTHERVIEWER, UC_MENU_GRAPHICAL, "",     "\"UP-Term:bin/amarchy-menu\" viewer", 1 },
        { KA_MENU,        UC_MENU_TEXT,      "apps", "\"UP-Term:bin/amarchy-menu\" menu apps", 1 },
        { KA_VIEWER,      UC_MENU_TEXT,      "",     "\"UP-Term:bin/amarchy-menu\" viewer", 1 },
        { KA_OTHERMENU,   UC_MENU_TEXT,      "apps", "\"UP-Term:bin/amarchy-gui\" menu apps", 0 },
        { KA_OTHERVIEWER, UC_MENU_TEXT,      "",     "\"UP-Term:bin/amarchy-gui\" viewer", 0 },
    };
    char out[96];
    int i, text;
    for (i = 0; i < 8; i++) {
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
    CHECK_INT(STYLE("[profile vim]\nmenu-style = text\n"), UC_MENU_GRAPHICAL);            /* default profile only */
#undef STYLE
}

static void other_menu_actions(void)
{
    const km_bind *b;
    CHECK(parse("[bind]\nsuper+alt+space = othermenu:root\nsuper+alt+k = otherviewer\n"));
    CHECK_INT(km.skipped, 0);
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, RAW_SPACE);
    CHECK(b != 0 && b->kind == KA_OTHERMENU && !strcmp(b->arg, "root"));
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, RAW_K);
    CHECK(b != 0 && b->kind == KA_OTHERVIEWER);
    CHECK_INT(keymap_action_scope(KA_OTHERMENU), KS_BOTH);
    CHECK_INT(keymap_action_scope(KA_OTHERVIEWER), KS_BOTH);
    CHECK(!parse("[bind]\nsuper+alt+k = otherviewer:x\n") || km.skipped == 1);
}

static void cx_descriptions_and_qualifier_mask(void)
{
    char buf[16384];
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
    other_menu_actions();
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
    char buf[16384];
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

static void terminal_rows_show_only_over_a_terminal(void)
{
    static char file[16384];
    mn_item it[MN_MAX_ITEMS];
    long n = slurp("dist/keymap", file, sizeof(file));
    int c;
    CHECK(n > 0 && keymap_parse(&km, file, n));
    c = ix_level_t(&km, 0, "", "", it, MN_MAX_ITEMS, 0);
    CHECK(has_label(it, c, "Apps"));
    CHECK(!has_label(it, c, "Style"));
    CHECK(!has_label(it, c, "Window"));
    c = ix_level_t(&km, 0, "style", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 0);
    c = ix_level_t(&km, 0, "window", "", it, MN_MAX_ITEMS, 0);
    CHECK_INT(c, 0);
    c = ix_level_t(&km, 0, "apps", "", it, MN_MAX_ITEMS, 0);
    CHECK(has_label(it, c, "UP-Term") && has_label(it, c, "Editor"));
    CHECK(!has_label(it, c, "Claude"));
    CHECK_INT(c, mn_children(&km, "apps", it, MN_MAX_ITEMS) - 1);
    c = ix_level_t(&km, 0, "", "", it, MN_MAX_ITEMS, 1);
    CHECK(has_label(it, c, "Apps") && has_label(it, c, "Style") && has_label(it, c, "Window"));
    c = ix_level_t(&km, 0, "style", "", it, MN_MAX_ITEMS, 1);
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
    mg_screen pal = { 640, 256, 22, 44 };
    CHECK(parse("[menu]\napps.a = Short | run:a\napps.b = Other | run:b\n"
                "apps.long = A very much longer slash label that is wider than the minimum window | slash:/theme\n"
                "apps.t1 = Tab one | tab:new\napps.t2 = Tab two | tab:next\ntop = Top | run:x\n"));
    mn_osd_size_t(&km, "apps", &cols, &rows, 0);
    mn_osd_size_t(&km, "apps", &cols1, &rows1, 1);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + 2);        /* Short, Other */
    CHECK_INT(rows1, MN_OSD_HEAD_LINES + 5);
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
}

/* AM-OSD O5: the commodity sizes the menu window from the keymap it holds, walking every level
 * reachable from the node (the same mn_children walk amarchy-menu uses). The expected numbers are
 * counted by hand from the shipped file below, not from mn_children. */
static void osd_size_counts_the_reachable_levels(void)
{
    int cols, rows;
    CHECK(parse("[menu]\napps.upkg = Install add-ons | run:upkg-browse\napps.tools.tmux = Tmux | run:tmux\n"
                "apps.tools.nvim = Editor | run:nvim\napps.tools.zzz = A much longer label here | run:z\n"
                "style.theme = Theme | slash:/theme\ntop = Top | run:x\n"));
    mn_osd_size(&km, "apps", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + 3);        /* apps has 2 entries, apps.tools has 3 */
    CHECK_INT(cols, 24 + 2 + 2 * MN_OSD_PAD);      /* "A much longer label here" + 2 spaces */
    mn_osd_size(&km, "", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + 3);        /* root has 3 (apps, style, top) */
    CHECK_INT(cols, 24 + 2 + 2 * MN_OSD_PAD);      /* reaches the long label two levels down */
    mn_osd_size(&km, "style", &cols, &rows);
    CHECK_INT(rows, MN_OSD_HEAD_LINES + 1);
    CHECK_INT(cols, MN_OSD_MIN_COLS);              /* "Theme" + pad is shorter than the minimum */
    /* the shipped dist/keymap: the sizes the commodity opens Super+Space and Super+Shift+Space with
     * (worked out from the file's [menu] rows with a separate script: 6 entries at most in apps, labels short) */
    {
        static char file[16384];
        long n = slurp("dist/keymap", file, sizeof(file));
        CHECK(n > 0 && keymap_parse(&km, file, n));
        mn_osd_size(&km, "", &cols, &rows);
        CHECK_INT(cols, MN_OSD_MIN_COLS);
        CHECK_INT(rows, MN_OSD_HEAD_LINES + 6);
        mn_osd_size(&km, "apps", &cols, &rows);
        CHECK_INT(cols, MN_OSD_MIN_COLS);
        CHECK_INT(rows, MN_OSD_HEAD_LINES + 6);
    }
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
    static char file[16384];
    mg_font topaz = { 8, 8 }, pro = { 16, 8 };
    mg_screen pal = { 640, 256, 22, 44 }, narrow = { 320, 256, 22, 44 }, sq = { 640, 512, 22, 22 };
    int px8 = 8, px16 = 16, w;
    char head[KM_ID];
    mn_item items[MN_MAX_ITEMS];
    mg_geom g;
    long n = slurp("dist/keymap", file, sizeof(file));

    /* topaz 8 on 640x256 hires: R 30; V(2) 2, V(18) 11, V(34) 20, V(6) 4, V(3) 2, V(12) 7, V(10) 6,
     * V(300) 180; across twice those: 4, 22, 14, 12, 360 */
    mg_metrics_for(&g.m, &topaz, &pal);
    CHECK_INT(g.m.bx, 4);
    CHECK_INT(g.m.by, 2);
    CHECK_INT(g.m.hpad, 22);
    CHECK_INT(g.m.vpad, 11);
    CHECK_INT(g.m.head_h, 20);
    CHECK_INT(g.m.gap, 4);
    CHECK_INT(g.m.row_h, 30);
    CHECK_INT(g.m.row_gap, 2);
    CHECK_INT(g.m.pitch, 32);
    CHECK_INT(g.m.row_pad, 14);
    CHECK_INT(g.m.icon_gap, 12);
    CHECK_INT(g.m.chev_gap, 14);
    CHECK_INT(g.m.min_w, 360);
    CHECK_INT(g.m.margin_x, 12);
    CHECK_INT(g.m.margin_y, 6);

    /* TopazPro 16 on 640x512 square pixels: R 60; V(2) 2, V(18) 22, V(34) 41, V(6) 7, V(3) 4, V(12) 14,
     * V(10) 12, V(300) 360; across the same */
    mg_metrics_for(&g.m, &pro, &sq);
    CHECK_INT(g.m.bx, 2);
    CHECK_INT(g.m.by, 2);
    CHECK_INT(g.m.hpad, 22);
    CHECK_INT(g.m.vpad, 22);
    CHECK_INT(g.m.head_h, 41);
    CHECK_INT(g.m.gap, 7);
    CHECK_INT(g.m.row_h, 60);
    CHECK_INT(g.m.row_gap, 4);
    CHECK_INT(g.m.pitch, 64);
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
    CHECK_INT(mn_children(&km, "", items, MN_MAX_ITEMS), 3);
    CHECK_INT(mn_children(&km, "apps", items, MN_MAX_ITEMS), 6);
    w = mg_width(&km, "", &topaz, 0, 8, fixed_width, &px8, &pal);
    CHECK_INT(w, 360);
    /* 3 rows: chrome 2 x 2 + 2 x 11 + 20 + 4 = 50, rows 3 x 32 - 2 = 94 */
    mg_layout(&g, &topaz, w, 3, &pal);
    CHECK_INT(g.h, 144);
    CHECK_INT(g.x, 140);
    CHECK_INT(g.y, 56);
    CHECK_INT(g.visible, 3);
    CHECK_INT(g.peek, 0);
    CHECK_INT(mg_more(&g, 0), 0);
    CHECK_INT(g.list_x, 26);
    CHECK_INT(g.list_y, 37);
    CHECK_INT(g.list_w, 308);
    /* room = min(256 - 12 - 50, 70 % of 256 = 179) = 179: 5 rows of 32 fit, no peek */
    mg_layout(&g, &topaz, w, 5, &pal);
    CHECK_INT(g.h, 208);
    CHECK_INT(g.y, 24);
    CHECK_INT(g.visible, 5);
    CHECK_INT(g.peek, 0);
    /* a sixth entry: 5 rows and the peek (55 % of 30 = 16) of the next */
    mg_layout(&g, &topaz, w, 6, &pal);
    CHECK_INT(g.visible, 5);
    CHECK_INT(g.peek, 16);

    /* hits: row 0 from y 37 for 30 px, then a 2 px gap, row 1 from 69 */
    mg_layout(&g, &topaz, w, 3, &pal);
    CHECK_INT(mg_hit(&g, 0, 30, 37), 0);
    CHECK_INT(mg_hit(&g, 0, 30, 67), -1);
    CHECK_INT(mg_row_y(&g, 1), 69);
    CHECK_INT(mg_hit(&g, 0, 30, 69), 1);
    CHECK_INT(mg_hit(&g, 0, 25, 37), -1);
    CHECK_INT(mg_hit(&g, 0, 26 + 308, 37), -1);

    /* 11 entries: 5 rows and the peek; the list is 5 x 32 + 16 = 176, the window 50 + 176 */
    mg_layout(&g, &topaz, w, 11, &pal);
    CHECK_INT(g.visible, 5);
    CHECK_INT(g.peek, 16);
    CHECK_INT(g.list_h, 176);
    CHECK_INT(g.h, 226);
    CHECK_INT(mg_more(&g, 0), 1);
    CHECK_INT(mg_more(&g, 6), 0);                  /* at the bottom the cue goes */
    CHECK_INT(mg_scroll(&g, 10, 0), 6);
    CHECK_INT(mg_scroll(&g, 1, 3), 1);
    CHECK_INT(mg_scroll(&g, 2, 1), 1);
    CHECK_INT(mg_hit(&g, 0, 30, 200), 5);          /* the peek row (from 37 + 160) selects the next entry */
    CHECK_INT(mg_hit(&g, 6, 30, 200), -1);         /* no peek drawn at the bottom */

    /* TopazPro 16 on 640x512: chrome 4 + 44 + 41 + 7 = 96; 3 rows 3 x 64 - 4 = 188; 5 rows fit in
     * min(512 - 24 - 96, 358) = 358 */
    w = mg_width(&km, "", &pro, 0, 8, fixed_width, &px8, &sq);
    CHECK_INT(w, 360);
    mg_layout(&g, &pro, w, 3, &sq);
    CHECK_INT(g.h, 284);
    CHECK_INT(g.x, 140);
    CHECK_INT(g.y, 114);
    mg_layout(&g, &pro, w, 5, &sq);
    CHECK_INT(g.h, 412);
    CHECK_INT(g.y, 50);
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
    mg_screen pal = { 640, 256, 22, 44 }, sq = { 640, 512, 22, 22 }, unknown = { 640, 256, 0, 0 };
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
    static char file[16384];
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
    m.row_h = 14;
    s = mi_size(&m);
    CHECK_INT(s, 8);                               /* 14 * 8 / 15 = 7, 8 at least */
    m.row_h = 30;
    CHECK_INT(mi_size(&m), 16);
    m.row_h = 60;
    CHECK_INT(mi_size(&m), 32);
    m.row_h = 400;
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
    static char file[16384], text[16384];
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
static void a_run_leaf_gets_its_own_window_not_the_menus_console(void)
{
    CHECK_INT((int)lf_plan(KA_RUN), (int)LF_WINDOW);
    CHECK_INT((int)lf_plan(KA_REXX), (int)LF_REXX);
    CHECK_INT((int)lf_plan(KA_OPEN), (int)LF_OPEN);
    CHECK_INT((int)lf_plan(KA_SLASH), (int)LF_REQUEST);
    CHECK_INT((int)lf_plan(KA_MENUID), (int)LF_REQUEST);
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
#define AMARCHY_KEYMAP_HEAP_MAX 800   /* 8 rows, 460 B measured; the parsed table is 8288 B */
static void the_commodity_keymap_of_dist_keymap_is_exact_and_small(void)
{
    static char file[16384];
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

static void the_app_index_core_behaves(void);

static void app_icons_are_cached_per_path_bounded_and_fitted(void);

void suite_keymap(void)
{
    menu_theme_roles_map_the_palette();
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
    the_shipped_file_resolves_and_derives_menu_keys();
    cx_descriptions_and_qualifier_mask();
    cx_installs_only_the_commodity_rows();
    ua_count_released_before_own_window_closes();
    typing_th_leaves_theme_first();
    menu_levels_follow_the_ids_and_escape_goes_up();
    terminal_rows_show_only_over_a_terminal();
    menu_size_follows_the_filtered_rows();
    osd_size_counts_the_reachable_levels();
    drawn_menu_geometry_follows_the_font_and_screen();
    drawn_menu_icons_look_square_on_tall_pixels();
    icons_are_not_upscaled_from_16px();
    every_shipped_menu_node_has_an_icon_image();
    the_key_viewer_lists_every_bind_row();
    the_key_viewer_follows_a_rebinding();
    a_request_resolves_like_its_bind_row();
    a_request_needs_no_bind_row();
    an_unknown_request_is_an_error_code();
    the_menu_builds_the_request_of_a_leaf();
    a_run_leaf_gets_its_own_window_not_the_menus_console();
    a_leaf_window_spec_names_the_front_public_screen();
    a_run_row_starts_its_command_through_the_upterm_shell();
    the_quick_menu_finds_its_terminal_after_an_earlier_menu_window_closed();
    the_target_is_the_front_entry_for_a_menu_outside_the_record();
    a_refused_slash_request_answers_why_and_a_missing_value_is_asked_for();
    the_app_index_core_behaves();
    app_icons_are_cached_per_path_bounded_and_fitted();
}

/* ---- menu_theme: the drawn menu's colour roles ---- */
#include "../amarchy/menu_theme.h"

/* role -> palette index is the table's: the shipped default palette is the renderer's xterm one, and the
 * table's defaults are those values (hand-written below, then held to the engine) */
static void menu_theme_roles_map_the_palette(void)
{
    static const uc_u32 want_default[MT_ROLES] = { 0x000000UL, 0xE5E5E5UL, 0x7F7F7FUL, 0x00FFFFUL, 0x0000EEUL };
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
        len += snprintf(text + len, sizeof text - len, "app\tApp%03d\tWork:A/App%03d\n", i, i);
    snprintf(text + len, sizeof text - len, "app\tZed Last\tWork:Z/Last\n");
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
    CHECK_INT(n, 5);                                   /* 2 menu rows + Clock + 2 Editors */
    CHECK_STR(it[0].label, "Tmux");                    /* the keymap rows come first */
    CHECK(level_has(it, n, "Clock"));
    CHECK(level_has(it, n, "Editor (Sys)"));           /* a shared label carries its volume */
    CHECK(level_has(it, n, "Editor (Work)"));
    CHECK(!level_has(it, n, "Turrican 2"));
    for (i = base; i < n; i++) {
        CHECK(MN_IS_INDEX(&it[i]));                    /* the action kind X5 launches */
        if (!strcmp(it[i].label, "Clock"))
            idx = it[i].ix;
    }
    CHECK(idx >= 0 && !strcmp(x.rows[idx].path, "Sys:Tools/Clock"));
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
    n = ix_level(&km, &x, "apps", "", it, TEST_LEVEL_MAX);
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].label, "Clock"))
            clock = i;
    CHECK(clock >= 0);
    CHECK_INT(ix_launch_path(&x, &it[clock], path, sizeof path), 1);
    CHECK_STR(path, "Sys:Tools/Clock");                /* the tool, no .info */
    CHECK_INT(ix_launch_path(&x, &it[0], path, sizeof path), 0);   /* a menu row is not launched this way */
    CHECK_INT(ix_launch_path(&x, &it[clock], path, 8), 0);         /* a path that does not fit */
    /* the cache is read again when ix_load_text runs again: a rescan shows on the next menu open */
    ix_load_text(&km, &x, "app\tZed\tWork:Zed\n");
    CHECK_INT(x.n, 1);
    /* the shipped menu has the Rescan apps row, and it starts the scanner */
    n = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nmenu; i++)
        if (!strcmp(km.menu[i].id, "apps.rescan"))
            rescan = i;
    CHECK(rescan >= 0);
    CHECK_STR(km.menu[rescan].label, "Rescan apps");
    CHECK_STR(km.menu[rescan].arg, "amarchy-index");
    CHECK_INT(km.menu[rescan].kind, KA_DETACH);          /* in the background: no UP-Term window for a scan */
    CHECK_INT((int)lf_plan(KA_DETACH), (int)LF_DETACH);
    CHECK_INT(keymap_action_scope(KA_DETACH), KS_BOTH);
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

static void the_games_level_lists_index_games_and_the_root_offers_the_group(void)
{
    static ix_index x;
    mn_item it[TEST_LEVEL_MAX];
    int n;
    CHECK(parse("[menu]\napps.tmux = Tmux | run:tmux\n"));
    ix_load_text(&km, &x, index_cache);
    n = ix_level(&km, &x, "games", "", it, TEST_LEVEL_MAX);
    CHECK_INT(n, 1);
    CHECK_STR(it[0].label, "Turrican 2");
    CHECK(MN_IS_INDEX(&it[0]));
    n = ix_level(&km, &x, "", "", it, TEST_LEVEL_MAX);
    CHECK(level_has(it, n, "Games"));
    CHECK(!level_has(it, n, "Turrican 2"));            /* without a filter the root stays short */
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
    CHECK_INT(x.n, 0);
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
    equal_index_labels_get_their_volume_as_suffix_and_rows_sort_by_label();
    a_menu_row_wins_over_an_index_row_with_the_same_label();
    the_apps_level_lists_index_apps_after_merging();
    an_index_row_launches_its_tool_path_and_a_rescan_row_ships_in_the_apps_menu();
    a_launch_request_for_the_resident_amarchy_carries_the_path_and_refuses_a_bad_one();
    a_menu_row_hides_an_index_app_with_the_same_label();
    the_games_level_lists_index_games_and_the_root_offers_the_group();
    a_missing_or_empty_cache_adds_no_index_rows();
    root_search_finds_an_index_game_and_an_index_app();
}

/* AM-IDX-ICON I1: the icon cache's slots and the fit of an image into the icon column. */
static void app_icons_are_cached_per_path_bounded_and_fitted(void)
{
    static ic_cache c;
    char p[IX_PATH + 1];
    int i, a, b, first, dw, dh, dx, dy;
    memset(&c, 0, sizeof c);
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
