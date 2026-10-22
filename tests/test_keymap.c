/* The keymap parser (config/keymap.c): syntax, expansion, refusals,
 * overrides, caps, and the shipped dist/keymap. No OS calls: a host suite. */
#include <stdio.h>
#include <string.h>
#include "../config/upconf.h"
#include "../config/keymap.h"
#include "../handler/keymap_act.h"
#include "../handler/menu_ids.h"
#include "../amarchy/cxdesc.h"
#include "../amarchy/menu_core.h"
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

static void cx_descriptions_and_qualifier_mask(void)
{
    char d[48], hex[8], buf[16384];
    const km_bind *b;
    long n;
    int i;
    CHECK(parse("[bind]\nsuper+k = viewer\nsuper+shift+space = menu:apps\n"
                "super+alt+t = run:C:Echo hi\n"));
    b = keymap_find(&km, KM_Q_SUPER, RAW_K);
    CHECK(b && cx_describe(b, d, sizeof(d)));
    CHECK_STR(d, "rawkey lcommand 0x27");
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_SHIFT, RAW_SPACE);
    CHECK(b && cx_describe(b, d, sizeof(d)));
    CHECK_STR(d, "rawkey lcommand shift 0x40");
    b = keymap_find(&km, KM_Q_SUPER | KM_Q_ALT, RAW_T);
    CHECK(b && cx_describe(b, d, sizeof(d)));
    CHECK_STR(d, "rawkey lcommand alt 0x14");
    CHECK(b && !cx_describe(b, d, 12));          /* too small: refused, never cut */
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
    /* every row of the shipped file describes, and the text names its raw code */
    n = slurp("dist/keymap", buf, sizeof(buf));
    CHECK(n > 0 && keymap_parse(&km, buf, n));
    for (i = 0; i < km.nbind; i++) {
        CHECK(cx_describe(&km.bind[i], d, sizeof(d)));
        hex[0] = '0'; hex[1] = 'x';
        hex[2] = "0123456789abcdef"[(km.bind[i].code >> 4) & 15];
        hex[3] = "0123456789abcdef"[km.bind[i].code & 15];
        hex[4] = 0;
        CHECK(strstr(d, hex) != 0);
        CHECK(strncmp(d, "rawkey lcommand ", 16) == 0);
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
    r.version = 2;
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

void suite_keymap(void)
{
    a_reload_swaps_whole_keeps_the_old_map_on_a_bad_edit_and_leaks_nothing();
    comments_crlf_and_fields();
    digits_expand_with_percent_n();
    bad_rows_are_skipped_and_counted();
    the_safety_refusals();
    a_newer_major_is_rejected_whole();
    a_later_row_replaces_and_none_unbinds();
    the_user_file_overrides_the_shipped_one();
    the_tables_have_caps();
    key_names_are_raw_codes();
    the_shipped_keymap_loads_whole();
    actions_resolve_to_menu_ids();
    the_shipped_file_resolves_and_derives_menu_keys();
    cx_descriptions_and_qualifier_mask();
    cx_installs_only_the_commodity_rows();
    ua_count_released_before_own_window_closes();
    typing_th_leaves_theme_first();
    menu_levels_follow_the_ids_and_escape_goes_up();
    the_key_viewer_lists_every_bind_row();
    the_key_viewer_follows_a_rebinding();
    a_request_resolves_like_its_bind_row();
    a_request_needs_no_bind_row();
    an_unknown_request_is_an_error_code();
    the_menu_builds_the_request_of_a_leaf();
}
