/* The Amarchy Packages menu rows (amarchy/pkgs_core.c; plan 2026-10-27-amarchy-packages-submenu.md P1). The input is
 * `upkg --tsv` text in the field order of upterm-ports pkgs/opkg/doc/upkg-tsv.txt. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/pkgs_core.h"
#include "harness.h"

static void test_pkgs_parse(void)
{
    static const char tsv[] =
        "tiny\t1.0\t1\t0\t\tutils\tUP-Term:uhc/ExtBin/Tiny\tuhc\tA tiny tool\n"
        "Gamma\t2.1\t1\t1\t12345\tgames\tUP-Term:bin/gamma-go\tupkg\tGamma game\r\n"
        "perl\t5.8\t1\t0\t900000\tdevel\t\tupkg\tno launch: skipped\n"
        "notyet\t1.0\t0\t0\t10\tgames\tUP-Term:bin/notyet\tupkg\tnot installed: skipped\n"
        "short\t1.0\t1\n"
        "\n"
        "alpha\t0.1\t1\t0\t1\tutils\tUP-Term:bin/alpha";   /* last line without newline, no fields 8-9 */
    static pk_list l;
    CHECK_INT(pk_parse(tsv, (long)sizeof tsv - 1, &l), 3);
    CHECK_STR(l.name[0], "alpha");
    CHECK_STR(l.launch[0], "UP-Term:bin/alpha");
    CHECK_STR(l.name[1], "Gamma");
    CHECK_STR(l.launch[1], "UP-Term:bin/gamma-go");   /* CR dropped */
    CHECK_STR(l.name[2], "tiny");
    CHECK_STR(l.launch[2], "UP-Term:uhc/ExtBin/Tiny");
    CHECK_INT(pk_parse("", 0, &l), 0);
}

static void test_pkgs_path(void)
{
    char p[PK_PATH + 8], back[PK_PATH], small[6];
    CHECK(pk_row_path("UP-Term:bin/gamma-go", p, (int)sizeof p));
    CHECK_STR(p, "pkg:UP-Term:bin/gamma-go");
    CHECK(pk_path_launch(p, back, (int)sizeof back));
    CHECK_STR(back, "UP-Term:bin/gamma-go");
    CHECK(!pk_path_launch("gtheme:tokyo-night", back, (int)sizeof back));
    CHECK(!pk_path_launch("pkg:", back, (int)sizeof back));
    CHECK(!pk_row_path("", p, (int)sizeof p));
    CHECK(!pk_row_path("UP-Term:x", small, (int)sizeof small));
}

/* PG.1: every row, all nine fields, mixed upkg and uhc, the label, the actions and the job lines */
static const char all_tsv[] =
    "tiny\t1.0\t1\t0\t\tutils\tUP-Term:uhc/ExtBin/Tiny\tuhc\tA tiny tool\n"
    "Gamma\t2.1\t1\t1\t12345\tgames\tUP-Term:bin/gamma-go\tupkg\tGamma game\r\n"
    "perl\t5.8\t1\t0\t900000\tdevel\t\tupkg\tScripting language\n"
    "notyet\t1.0\t0\t0\t10\tGames\t\tuhc\tNot installed\n"
    "bad;name\t1\t0\t0\t1\tx\t\tupkg\tmetacharacter: refused\n"
    "short\t1.0\t1\n"
    "\n"
    "nosec\t0.1\t0\t0\t1\t\t\tupkg";   /* last line without newline, no description */

static void test_pkgs_all_rows(void)
{
    static pk_rows l;
    char sec[8][PK_SEC];
    int idx[16], n, i;
    CHECK_INT(pk_parse_all(all_tsv, (long)sizeof all_tsv - 1, &l), 5);
    CHECK_STR(l.r[0].name, "Gamma");              /* sorted without case */
    CHECK_STR(l.r[1].name, "nosec");
    CHECK_STR(l.r[2].name, "notyet");
    CHECK_STR(l.r[4].name, "tiny");
    CHECK_STR(l.r[0].launch, "UP-Term:bin/gamma-go");   /* CR dropped, fields kept */
    CHECK_STR(l.r[0].desc, "Gamma game");
    CHECK_STR(l.r[0].version, "2.1");
    CHECK(l.r[0].installed && l.r[0].upgradable);
    CHECK(!l.r[2].installed && !strcmp(l.r[2].src, "uhc"));
    CHECK_STR(l.r[3].launch, "");                 /* installed, empty launch: kept (the old parse dropped it) */
    CHECK(l.r[3].installed);
    CHECK_STR(l.r[1].desc, "");
    n = pk_sections(&l, sec, 8);                  /* Games and games are one section; an empty field is "other" */
    CHECK_INT(n, 4);
    CHECK_STR(sec[0], "devel");
    CHECK_STR(sec[1], "games");
    CHECK_STR(sec[2], "other");
    CHECK_STR(sec[3], "utils");
    CHECK_INT(pk_by_section(&l, "games", idx, 16), 2);
    CHECK_INT(pk_by_section(&l, "OTHER", idx, 16), 1);
    CHECK_STR(l.r[idx[0]].name, "nosec");
    CHECK_INT(pk_select(&l, 0, idx, 16), 3);      /* installed: Gamma, perl, tiny */
    CHECK_INT(pk_select(&l, 1, idx, 16), 1);      /* upgradable: Gamma */
    CHECK_STR(l.r[idx[0]].name, "Gamma");
    CHECK_INT(pk_parse_all("", 0, &l), 0);
    CHECK(l.r == 0 || l.n == 0);
    pk_rows_free(&l);
    for (i = 0; i < 1; i++)
        CHECK(l.r == 0);
}

static void test_pkgs_filter(void)
{
    static pk_rows l;
    int idx[16];
    CHECK_INT(pk_parse_all(all_tsv, (long)sizeof all_tsv - 1, &l), 5);
    CHECK_INT(pk_filter(&l, "", idx, 16), 5);     /* empty text lists all */
    CHECK_INT(pk_filter(&l, "GAM", idx, 16), 1);  /* name hit, any case */
    CHECK_STR(l.r[idx[0]].name, "Gamma");
    CHECK_INT(pk_filter(&l, "scripting", idx, 16), 1);   /* description hit */
    CHECK_STR(l.r[idx[0]].name, "perl");
    CHECK_INT(pk_filter(&l, "tool", idx, 16), 1);
    CHECK_INT(pk_filter(&l, "zzz", idx, 16), 0);
    CHECK_INT(pk_filter(&l, "", idx, 2), 2);      /* max respected */
    pk_rows_free(&l);
}

static void test_pkgs_state_actions(void)
{
    static pk_rows l;
    char s[64], line[96], acts[4];
    int n;
    CHECK_INT(pk_parse_all(all_tsv, (long)sizeof all_tsv - 1, &l), 5);
    pk_label(&l.r[0], s, sizeof s);               /* Gamma: installed, upgradable, upkg */
    CHECK_STR(s, "[upkg] Gamma  2.1  Upgrade to 2.1");
    pk_label(&l.r[4], s, sizeof s);               /* tiny: installed, uhc */
    CHECK_STR(s, "[UHC] tiny  1.0  Installed");
    pk_label(&l.r[1], s, sizeof s);               /* nosec: not installed, empty state trimmed */
    CHECK_STR(s, "[upkg] nosec  0.1");
    n = pk_actions(&l.r[2], acts);                /* notyet: not installed */
    CHECK_INT(n, 1);
    CHECK_INT(acts[0], PK_ACT_INSTALL);
    n = pk_actions(&l.r[0], acts);                /* Gamma: installed, upgradable, has a launch path */
    CHECK_INT(n, 3);
    CHECK_INT(acts[0], PK_ACT_REMOVE);
    CHECK_INT(acts[1], PK_ACT_LAUNCH);
    CHECK_INT(acts[2], PK_ACT_UPGRADE);
    n = pk_actions(&l.r[3], acts);                /* perl: installed, empty launch: no Launch, not upgradable */
    CHECK_INT(n, 1);
    CHECK_INT(acts[0], PK_ACT_REMOVE);
    CHECK(pk_action_line(&l.r[2], PK_ACT_INSTALL, line, sizeof line));
    CHECK_STR(line, "upkg install uhc:notyet");   /* uhc: prefix only for src uhc */
    CHECK(pk_action_line(&l.r[1], PK_ACT_INSTALL, line, sizeof line));
    CHECK_STR(line, "upkg install nosec");
    CHECK(pk_action_line(&l.r[0], PK_ACT_REMOVE, line, sizeof line));
    CHECK_STR(line, "upkg remove Gamma");
    CHECK(pk_action_line(&l.r[0], PK_ACT_UPGRADE, line, sizeof line));
    CHECK_STR(line, "upkg upgrade Gamma");
    CHECK(!pk_action_line(&l.r[0], PK_ACT_LAUNCH, line, sizeof line));   /* Launch is a pkg: path, not a job */
    CHECK(!pk_action_line(&l.r[0], 'X', line, sizeof line));
    CHECK(!pk_action_line(&l.r[0], PK_ACT_REMOVE, line, 8));             /* does not fit */
    pk_rows_free(&l);
}

static void test_pkgs_names_and_limit(void)
{
    static pk_rows l;
    pk_row r;
    char line[96], acts[4];
    char *big = malloc((size_t)(PK_ROWS_MAX + 50) * 40), *p;
    int i;
    CHECK(pk_name_ok("lib-png1.6+x_2"));
    CHECK(!pk_name_ok("a b") && !pk_name_ok("a;b") && !pk_name_ok("a$b") && !pk_name_ok("a`b") && !pk_name_ok("") &&
          !pk_name_ok("a|b") && !pk_name_ok("a\"b"));
    memset(&r, 0, sizeof r);
    strcpy(r.name, "x;rm -rf");
    CHECK_INT(pk_actions(&r, acts), 0);           /* a bad name is not offered ... */
    CHECK(!pk_action_line(&r, PK_ACT_INSTALL, line, sizeof line));   /* ... and never reaches a line */
    p = big;
    for (i = 0; i < PK_ROWS_MAX + 50; i++)
        p += snprintf(p, 40, "p%d\t1\t0\t0\t1\ts\t\tupkg\td\n", i);
    CHECK_INT(pk_parse_all(big, (long)(p - big), &l), PK_ROWS_MAX);
    CHECK_INT(l.over, 1);
    free(big);
    pk_rows_free(&l);
}

void suite_pkgs(void)
{
    test_pkgs_parse();
    test_pkgs_path();
    test_pkgs_all_rows();
    test_pkgs_filter();
    test_pkgs_state_actions();
    test_pkgs_names_and_limit();
}
