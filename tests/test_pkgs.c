/* The Amarchy Packages menu rows (amarchy/pkgs_core.c; plan 2026-10-27-amarchy-packages-submenu.md P1). The input is
 * `upkg --tsv` text in the field order of upterm-ports pkgs/opkg/doc/upkg-tsv.txt. */
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

void suite_pkgs(void)
{
    test_pkgs_parse();
    test_pkgs_path();
}
