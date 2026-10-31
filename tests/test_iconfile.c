/* The .info rewrite of the Lucide icons (amarchy/iconfile_core.c; plan 2026-10-31-amarchy-lucide-icons.md L4, L13 host
 * part). Fixtures: tests/data/icons/ (a stock 3.2 drawer icon and tool icon), a built-here NewIcon + FORM ICON file, a
 * built-here lucide.dat, and every .info under build/rig/os-31 and os-322 when those trees exist (UPTERM_ICON_ROOT
 * names another root). */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../amarchy/iconfile_core.h"
#include "harness.h"

#define BUF 65536
static uint8_t g_a[BUF], g_b[BUF], g_c[BUF], g_d[BUF];

static long slurp(const char *path, uint8_t *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

static size_t w16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; return 2; }
static size_t w32(uint8_t *p, uint32_t v) { w16(p, v >> 16); w16(p + 2, v & 0xFFFF); return 4; }

/* a lucide.dat with the icons "alpha" and "folder", variants 32x32 and 32x16, depth 6, planes filled with a marker */
static size_t make_dat(uint8_t *d)
{
    size_t pos = 0, rec0, rec1, sz0 = 6 * 32 * 2 * 2, sz1 = 6 * 16 * 2 * 2, i;
    memcpy(d, "LUCI", 4);
    pos = 4;
    pos += w16(d + pos, 1); pos += w16(d + pos, 2); pos += w16(d + pos, 6); pos += w16(d + pos, 2); pos += w32(d + pos, 0);
    pos += w16(d + pos, 32); pos += w16(d + pos, 32); pos += w16(d + pos, 32); pos += w16(d + pos, 16);
    rec0 = pos + 2 * 52;
    rec1 = rec0 + 2 * (sz0 + sz1);
    memset(d + pos, 0, 2 * 52);
    memcpy(d + pos, "alpha", 5); w32(d + pos + 48, (uint32_t)rec0); pos += 52;
    memcpy(d + pos, "folder", 6); w32(d + pos + 48, (uint32_t)rec1); pos += 52;
    for (i = 0; i < 2 * (sz0 + sz1); i++) d[rec0 + i] = 0x11;
    for (i = 0; i < 2 * (sz0 + sz1); i++) d[rec1 + i] = (uint8_t)(0xA0 + (i < sz0 ? 0 : i < 2 * sz0 ? 1 : i < 2 * sz0 + sz1 ? 2 : 3));
    return rec1 + 2 * (sz0 + sz1);
}

/* a WBTOOL icon: 8x4 depth 2 image, optional select image, tooltypes, optional FORM ICON tail */
static size_t make_tool(uint8_t *p, int sel, const char **tt, int ntt, int form)
{
    size_t pos = 0, tb;
    int i;
    memset(p, 0, 78);
    w16(p, 0xE310); w16(p + 2, 1);
    w16(p + 12, 8); w16(p + 14, 4); w16(p + 16, sel ? 6 : 4);
    w32(p + 22, 0x00123456);
    w32(p + 26, sel ? 0x00654321 : 0);
    p[48] = 3;
    w32(p + 54, ntt ? 0x0001ABCD : 0);
    w32(p + 58, 77); w32(p + 62, 99);
    w32(p + 74, 4096);
    pos = 78;
    for (i = 0; i < (sel ? 2 : 1); i++) {
        w16(p + pos + 4, 8); w16(p + pos + 6, 4); w16(p + pos + 8, 2);
        w32(p + pos + 10, 0x00FF00FF); p[pos + 14] = 3;
        memset(p + pos + 20, 0x5A + i, 16);
        pos += 36;
    }
    if (ntt) {
        tb = pos;
        pos += 4;
        for (i = 0; i < ntt; i++) {
            size_t n = strlen(tt[i]) + 1;
            pos += w32(p + pos, (uint32_t)n);
            memcpy(p + pos, tt[i], n);
            pos += n;
        }
        w32(p + tb, (uint32_t)(ntt + 1) * 4);
    }
    if (form) {
        memcpy(p + pos, "FORM", 4); w32(p + pos + 4, 12); memcpy(p + pos + 8, "ICON", 4);
        memcpy(p + pos + 12, "FACE\0\0\0\6abcdef", 14);
        pos += 26;
    }
    return pos;
}

/* parse, then splice back the file's own images and fields: identical bytes */
static int identity(const uint8_t *buf, size_t len)
{
    li_file f;
    li_edit e;
    size_t n;
    if (li_parse(buf, len, &f) != 0) return 0;
    memset(&e, 0, sizeof e);
    e.nimg = f.nimg; e.img[0] = f.img[0]; e.img[1] = f.img[1];
    e.gad_w = f.gad_w; e.gad_h = f.gad_h; e.gad_flags = f.gad_flags;
    e.gad_render = f.gad_render; e.gad_select = f.gad_select;
    n = li_splice(&f, &e, g_c, BUF);
    return n == len && memcmp(g_c, buf, len) == 0;
}

/* the full cycle against the lucide.dat icon "alpha": apply, then restore into the written file */
static int cycle(const uint8_t *buf, size_t len, const li_dat *d, int variant)
{
    li_file src, wr, back;
    li_edit e;
    li_image nrm, sel;
    size_t n1, n2;
    if (li_parse(buf, len, &src) != 0) return 0;
    if (li_dat_find(d, "alpha", variant, &nrm, &sel) != 0) return 0;
    li_edit_apply(&e, &src, &nrm, &sel);
    n1 = li_splice(&src, &e, g_c, BUF);
    if (!n1 || li_parse(g_c, n1, &wr) != 0) return 0;
    if (wr.nimg != 2 || wr.img[0].width != nrm.width || wr.form_len != 0 || li_newicon_count(&wr, NULL) != 0) return 0;
    if (wr.gad_w != nrm.width || wr.gad_h != nrm.height || (wr.gad_flags & 3) != 2) return 0;
    if (memcmp(wr.img[0].data, nrm.data, nrm.dlen) != 0 || memcmp(wr.img[1].data, sel.data, sel.dlen) != 0) return 0;
    if (li_parse(buf, len, &back) != 0) return 0;
    li_edit_restore(&e, &wr, &back);
    n2 = li_splice(&wr, &e, g_d, BUF);
    return n2 == len && memcmp(g_d, buf, len) == 0;
}

static void test_fixtures(void)
{
    li_file f;
    long n;
    li_dat d;
    size_t dl;
    static uint8_t dat[8192];

    dl = make_dat(dat);
    CHECK_INT(li_dat_open(dat, dl, &d), 0);

    n = slurp("tests/data/icons/os322-drawer.info", g_a, BUF);
    CHECK(n > 0);
    CHECK_INT(li_parse(g_a, (size_t)n, &f), 0);
    CHECK(f.is_drawer);
    CHECK_INT(f.nimg, 2);
    CHECK_INT(f.img[0].width, 57);
    CHECK_INT(f.img[0].depth, 2);
    CHECK_INT(f.ntt, -1);
    CHECK_INT(f.form_len, 0);
    CHECK(identity(g_a, (size_t)n));
    CHECK(cycle(g_a, (size_t)n, &d, 0));
    CHECK(cycle(g_a, (size_t)n, &d, 1));

    n = slurp("tests/data/icons/os322-tool.info", g_a, BUF);
    CHECK(n > 0);
    CHECK_INT(li_parse(g_a, (size_t)n, &f), 0);
    CHECK(!f.is_drawer);
    CHECK(identity(g_a, (size_t)n));
    CHECK(cycle(g_a, (size_t)n, &d, 0));
}

static void test_newicon_form(void)
{
    static const char *tt[] = { "FILETYPE=text", "*** DON'T EDIT THE FOLLOWING LINES!! ***", "IM1=BAAAA", "IM1=BBBB", "IM2=CCCC", "DONOTWAIT" };
    static const char *only[] = { "*** DON'T EDIT THE FOLLOWING LINES!! ***", "IM1=BAAAA" };
    static uint8_t dat[8192];
    li_dat d;
    li_file src, wr;
    li_edit e;
    li_image nrm, sel;
    size_t len, n;
    int first, i;

    len = make_dat(dat);
    CHECK_INT(li_dat_open(dat, len, &d), 0);
    len = make_tool(g_a, 1, tt, 6, 1);
    CHECK_INT(li_parse(g_a, len, &src), 0);
    CHECK_INT(src.ntt, 6);
    CHECK_INT(li_newicon_count(&src, &first), 4);
    CHECK_INT(first, 1);
    CHECK(src.form_len > 0);
    CHECK(identity(g_a, len));

    /* APPLY strips the NewIcon lines and the FORM tail, keeps the other tooltypes and their order */
    CHECK_INT(li_dat_find(&d, "folder", 0, &nrm, &sel), 0);
    li_edit_apply(&e, &src, &nrm, &sel);
    n = li_splice(&src, &e, g_c, BUF);
    CHECK(n > 0);
    CHECK_INT(li_parse(g_c, n, &wr), 0);
    CHECK_INT(wr.ntt, 2);
    CHECK_STR((const char *)wr.buf + wr.tt[0].off + 4, "FILETYPE=text");
    CHECK_STR((const char *)wr.buf + wr.tt[1].off + 4, "DONOTWAIT");
    CHECK_INT(wr.form_len, 0);
    CHECK_INT(wr.nimg, 2);
    CHECK_INT(wr.tt_raw, 0x0001ABCD);
    CHECK_INT(wr.gad_w, 32);

    /* RESTORE returns the original bytes; a later edit of CurrentX survives */
    li_edit_restore(&e, &wr, &src);
    g_c[58 + 3] = 123;
    n = li_splice(&wr, &e, g_d, BUF);
    CHECK_INT(n, len);
    CHECK_INT(g_d[61], 123);
    g_d[61] = g_a[61];
    CHECK(memcmp(g_d, g_a, len) == 0);

    /* only NewIcon tooltypes: the array disappears (ToolTypes field 0), and comes back */
    len = make_tool(g_a, 0, only, 2, 0);
    CHECK_INT(li_parse(g_a, len, &src), 0);
    CHECK_INT(li_dat_find(&d, "alpha", 1, &nrm, &sel), 0);
    li_edit_apply(&e, &src, &nrm, NULL);
    n = li_splice(&src, &e, g_c, BUF);
    CHECK_INT(li_parse(g_c, n, &wr), 0);
    CHECK_INT(wr.ntt, -1);
    CHECK_INT(wr.tt_raw, 0);
    CHECK_INT(wr.nimg, 1);
    CHECK_INT(wr.gad_select, 0);
    CHECK_INT(wr.gad_h, 16);
    CHECK_INT(wr.gad_flags & 3, 0);
    li_edit_restore(&e, &wr, &src);
    n = li_splice(&wr, &e, g_d, BUF);
    CHECK_INT(n, len);
    CHECK(memcmp(g_d, g_a, len) == 0);

    /* a tooltype array with zero entries (stock 3.1 files) survives the round trip */
    len = make_tool(g_a, 1, tt, 0, 0);
    w32(g_a + 54, 0x0001ABCD);
    w32(g_a + len, 4);
    len += 4;
    CHECK_INT(li_parse(g_a, len, &src), 0);
    li_edit_apply(&e, &src, &nrm, &sel);
    n = li_splice(&src, &e, g_c, BUF);
    CHECK_INT(li_parse(g_c, n, &wr), 0);
    li_edit_restore(&e, &wr, &src);
    n = li_splice(&wr, &e, g_d, BUF);
    CHECK_INT(n, len);
    CHECK(memcmp(g_d, g_a, len) == 0);

    /* the cap is honoured, a truncated file is refused, never read past len */
    li_edit_apply(&e, &src, &nrm, NULL);
    CHECK_INT(li_splice(&src, &e, g_c, 100), 0);
    for (i = 0; i < (int)len; i += 7) CHECK(li_parse(g_a, (size_t)i, &wr) != 0 || i >= (int)len);
    g_a[0] = 0;
    CHECK_INT(li_parse(g_a, len, &wr), LI_E_MAGIC);
}

static void test_pick_backup(void)
{
    char out[128];
    CHECK_INT(li_pick_variant(640, 256), 1);
    CHECK_INT(li_pick_variant(640, 200), 1);
    CHECK_INT(li_pick_variant(640, 512), 0);
    CHECK_INT(li_pick_variant(320, 256), 0);
    CHECK_INT(li_pick_variant(800, 600), 0);
    CHECK_INT(li_pick_variant(1280, 720), 0);
    CHECK_INT(li_pick_variant(0, 0), 0);

    CHECK_INT(li_backup_path("Work:Tools/Foo.info", out, sizeof out), 0);
    CHECK_STR(out, "SYS:Storage/Icons/Amarchy/Work/Tools/Foo.info");
    CHECK_INT(li_backup_path("SYS:Prefs.INFO", out, sizeof out), 0);
    CHECK_STR(out, "SYS:Storage/Icons/Amarchy/SYS/Prefs.INFO");
    CHECK_INT(li_backup_path("Ram Disk:disk.info", out, sizeof out), 0);
    CHECK_STR(out, "SYS:Storage/Icons/Amarchy/Ram Disk/disk.info");
    CHECK_INT(li_backup_path("Tools/Foo.info", out, sizeof out), -1);
    CHECK_INT(li_backup_path("Work:Tools/Foo", out, sizeof out), -1);
    CHECK_INT(li_backup_path("a/b:Foo.info", out, sizeof out), -1);
    CHECK_INT(li_backup_path("Work:", out, sizeof out), -1);
    CHECK_INT(li_backup_path("Work:Tools/Foo.info", out, 20), -1);
}

static void test_dat(void)
{
    static uint8_t dat[8192];
    li_dat d;
    li_image n, s;
    size_t len = make_dat(dat);
    CHECK_INT(li_dat_open(dat, len, &d), 0);
    CHECK_INT(li_dat_find(&d, "alpha", 0, &n, &s), 0);
    CHECK_INT(n.width, 32); CHECK_INT(n.height, 32); CHECK_INT(n.depth, 6); CHECK_INT(n.planepick, 63);
    CHECK_INT(n.dlen, 6 * 32 * 4);
    CHECK_INT(n.data[0], 0x11); CHECK_INT(s.data[0], 0x11);
    CHECK_INT(li_dat_find(&d, "folder", 1, &n, &s), 0);
    CHECK_INT(n.height, 16);
    CHECK_INT(n.data[0], 0xA2); CHECK_INT(s.data[0], 0xA3);
    CHECK_INT(li_dat_find(&d, "folder", 0, &n, &s), 0);
    CHECK_INT(n.data[0], 0xA0); CHECK_INT(s.data[0], 0xA1);
    CHECK_INT(li_dat_find(&d, "nothing", 0, &n, &s), -1);
    CHECK_INT(li_dat_find(&d, "alpha", 2, &n, &s), -1);
    CHECK_INT(li_dat_open(dat, 10, &d), -1);
    dat[0] = 'X';
    CHECK_INT(li_dat_open(dat, len, &d), -1);
}

/* every .info under dir: parse, identity splice and the apply/restore cycle; counts */
static int g_files, g_bad;

static void walk(const char *dir, const li_dat *d)
{
    DIR *dp = opendir(dir);
    struct dirent *de;
    char path[1024];
    struct stat st;
    size_t nl;
    long n;

    if (!dp) return;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.' && (de->d_name[1] == 0 || (de->d_name[1] == '.' && de->d_name[2] == 0))) continue;
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { walk(path, d); continue; }
        nl = strlen(de->d_name);
        if (nl < 6 || strcmp(de->d_name + nl - 5, ".info") != 0 || st.st_size > BUF) continue;
        n = slurp(path, g_b, BUF);
        g_files++;
        if (n <= 0 || !identity(g_b, (size_t)n) || !cycle(g_b, (size_t)n, d, g_files & 1)) {
            g_bad++;
            printf("  corpus: %s fails\n", path);
        }
    }
    closedir(dp);
}

static void test_corpus(void)
{
    static uint8_t dat[8192];
    li_dat d;
    const char *root = getenv("UPTERM_ICON_ROOT");
    char dir[512];
    static const char *trees[] = { "os-31", "os-322" };
    int i;
    size_t dl = make_dat(dat);

    CHECK_INT(li_dat_open(dat, dl, &d), 0);
    if (!root) root = "build/rig";
    for (i = 0; i < 2; i++) {
        snprintf(dir, sizeof dir, "%s/%s", root, trees[i]);
        walk(dir, &d);
    }
    CHECK_INT(g_bad, 0);
    printf("  iconfile corpus: %d .info files under %s/os-31 and os-322%s\n", g_files, root, g_files ? "" : " (trees absent, only the tests/data/icons fixtures ran)");
}

void suite_iconfile(void)
{
    test_fixtures();
    test_newicon_form();
    test_pick_backup();
    test_dat();
    test_corpus();
}
