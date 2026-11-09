/* dlsave: the path rules and the create-each-level walk (handler/dlsave.c). The AmigaDOS binding is
 * a fake OS here that records every call. */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../handler/dlsave.h"

static char made[4][160];
static int nmade, fail_at = -1;
static char wrote_path[160];
static long wrote_len;
static int nwrote;

static int fk_mkdir(const char *p)
{
    if (nmade == fail_at)
        return 0;
    strcpy(made[nmade++], p);
    return 1;
}

static int fk_write(const char *p, const vt_u8 *d, long len)
{
    (void)d;
    strcpy(wrote_path, p);
    wrote_len = len;
    nwrote++;
    return 1;
}

static const dl_os fos = { fk_mkdir, fk_write };

static void paths(void)
{
    char o[160], small[20];
    int i;
    CHECK(dl_path(o, sizeof o, "", "A.TXT"));
    CHECK_STR(o, "ENVARC:UP-Term/Downloads/local/A.TXT");
    CHECK(dl_path(o, sizeof o, "bbs.example.org", "A.TXT"));
    CHECK_STR(o, "ENVARC:UP-Term/Downloads/bbs.example.org/A.TXT");
    CHECK(dl_path(o, sizeof o, 0, "A.TXT"));
    CHECK_STR(o, "ENVARC:UP-Term/Downloads/local/A.TXT");
    /* separators from the input never survive */
    CHECK(dl_path(o, sizeof o, "x/../y:z", "a/b:c\001.d"));
    CHECK_STR(o, "ENVARC:UP-Term/Downloads/x_.._y_z/a_b_c_.d");
    {
        int slashes = 0, colons = 0;
        for (i = 0; o[i]; i++) {
            slashes += o[i] == '/';
            colons += o[i] == ':';
        }
        CHECK_INT(slashes, 3);
        CHECK_INT(colons, 1);
    }
    /* overlong leaves are cut */
    {
        char longn[200];
        memset(longn, 'n', 199);
        longn[199] = 0;
        CHECK(dl_path(o, sizeof o, longn, longn));
        CHECK_INT(strlen(o), strlen(DL_ROOT) + 1 + DL_LEAF_MAX + 1 + DL_LEAF_MAX);
    }
    CHECK(!dl_path(small, sizeof small, "h", "A"));
    CHECK_STR(small, "");
    CHECK(!dl_path(o, sizeof o, "h", ""));
}

static void walk(void)
{
    static const vt_u8 data[5] = { 1, 2, 3, 4, 5 };
    nmade = nwrote = 0;
    fail_at = -1;
    CHECK(dl_save_with(&fos, "bbs", "N.BIN", data, 5));
    CHECK_INT(nmade, 3);
    CHECK_STR(made[0], "ENVARC:UP-Term");
    CHECK_STR(made[1], "ENVARC:UP-Term/Downloads");
    CHECK_STR(made[2], "ENVARC:UP-Term/Downloads/bbs");
    CHECK_STR(wrote_path, "ENVARC:UP-Term/Downloads/bbs/N.BIN");
    CHECK_INT(wrote_len, 5);
    /* a level that cannot be made: nothing is written */
    nmade = nwrote = 0;
    fail_at = 1;
    CHECK(!dl_save_with(&fos, "bbs", "N.BIN", data, 5));
    CHECK_INT(nwrote, 0);
    fail_at = -1;
}

static void hosts(void)
{
    char h[48];
    dl_host_of_cwd("file://bbs.example.org/home/x", h, sizeof h);
    CHECK_STR(h, "bbs.example.org");
    dl_host_of_cwd("file:///home/x", h, sizeof h);
    CHECK_STR(h, "");
    dl_host_of_cwd("", h, sizeof h);
    CHECK_STR(h, "");
    dl_host_of_cwd(0, h, sizeof h);
    CHECK_STR(h, "");
    dl_host_of_cwd("file://averyverylonghostnamethatdoesnotfit/x", h, 8);
    CHECK_STR(h, "averyve");
}

void suite_dlsave(void)
{
    paths();
    walk();
    hosts();
}
