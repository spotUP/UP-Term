/* bar.conf: parse, defaults on a missing file or missing keys, bad rows ignored with a count (amarchy/barconf_core.c). */
#include "../amarchy/barconf_core.h"
#include "harness.h"
#include <stdio.h>
#include <string.h>

static int parse(const char *text, barconf *c)
{
    return barconf_parse(text, (long)strlen(text), c);
}

static void test_defaults(void)
{
    barconf c;
    int i;
    parse("", &c);
    CHECK_INT(c.start, 1);
    CHECK_INT(c.clock[0], 0);
    CHECK_INT(c.nrun, 0);
    CHECK_INT(c.bad, 0);
    for (i = 0; i < BC_NSEGS; i++)
        CHECK_INT(c.seg_on[i], 1);
    for (i = 0; i < BC_NGROUPS; i++)
        CHECK_INT(c.norder[i], 0);
    parse("; only a comment\n# another\n\n   \n", &c);
    CHECK_INT(c.bad, 0);
    CHECK_INT(c.start, 1);
}

static void test_keys(void)
{
    barconf c;
    parse("start off\nclock %H:%M\nclockalt %d %b %Y\n"
          "right cpu ram system\nsegment ram off\nrun Files Dir SYS:\nrun Edit 'ed' ram:x\r\n", &c);
    CHECK_INT(c.start, 0);
    CHECK(!strcmp(c.clock, "%H:%M"));
    CHECK(!strcmp(c.clockalt, "%d %b %Y"));
    CHECK_INT(c.norder[BC_G_RIGHT], 3);
    CHECK_INT(c.order[BC_G_RIGHT][0], BC_SEG_CPU);
    CHECK_INT(c.order[BC_G_RIGHT][2], BC_SEG_SYSTEM);
    CHECK_INT(c.norder[BC_G_LEFT], 0);
    CHECK_INT(c.seg_on[BC_SEG_RAM], 0);
    CHECK_INT(c.seg_on[BC_SEG_CPU], 1);
    CHECK_INT(c.nrun, 2);
    CHECK(!strcmp(c.run[0].label, "Files") && !strcmp(c.run[0].cmd, "Dir SYS:"));
    CHECK(!strcmp(c.run[1].cmd, "'ed' ram:x"));
    CHECK_INT(c.bad, 0);
    parse("start on\n", &c);
    CHECK_INT(c.start, 1);
}

static void test_bad(void)
{
    barconf c;
    parse("start maybe\nclock\nleft nothing\nright\nsegment ram\nsegment foo on\nrun Only\nbogus 1\nstart off", &c);
    CHECK_INT(c.bad, 8);
    CHECK_INT(c.start, 0);              /* the last row has no newline and still counts */
    CHECK_INT(c.norder[BC_G_LEFT], 0);
    CHECK_INT(c.nrun, 0);
    CHECK_INT(barconf_seg("tray"), BC_SEG_TRAY);
    CHECK_INT(barconf_seg("Tray"), -1);
}

static int plan_pos(const bc_place *p, int n, int seg)
{
    int i;
    for (i = 0; i < n; i++)
        if (p[i].seg == seg)
            return i;
    return -1;
}

static void test_plan(void)
{
    barconf c;
    bc_place p[BC_NSEGS];
    int n;
    barconf_defaults(&c);
    n = barconf_plan(&c, p);
    CHECK_INT(n, BC_NSEGS);                              /* defaults: every segment, built-in groups */
    CHECK_INT(p[plan_pos(p, n, BC_SEG_CLOCK)].group, BC_G_CENTRE);
    CHECK_INT(p[plan_pos(p, n, BC_SEG_SCREENS)].group, BC_G_LEFT);
    CHECK(plan_pos(p, n, BC_SEG_CPU) < plan_pos(p, n, BC_SEG_NETWORK));
    CHECK(plan_pos(p, n, BC_SEG_AGENTS) < plan_pos(p, n, BC_SEG_SYSTEM));
    /* a group row orders its segments first; off leaves one out; a named segment moves group; screens stays left */
    parse("right system ram\nsegment cpu off\nleft clock screens\ncentre screens\n", &c);
    n = barconf_plan(&c, p);
    CHECK_INT(n, BC_NSEGS - 1);
    CHECK_INT(plan_pos(p, n, BC_SEG_CPU), -1);
    CHECK(plan_pos(p, n, BC_SEG_RAM) == plan_pos(p, n, BC_SEG_SYSTEM) + 1);
    CHECK_INT(p[plan_pos(p, n, BC_SEG_CLOCK)].group, BC_G_LEFT);
    CHECK_INT(p[plan_pos(p, n, BC_SEG_SCREENS)].group, BC_G_LEFT);
    CHECK(plan_pos(p, n, BC_SEG_SYSTEM) < plan_pos(p, n, BC_SEG_NETWORK));  /* unnamed ones follow the named */
}

/* the file the release installs (dist/bar.conf): it loads with no bad row and says what the parser defaults say, so
 * a fresh install behaves like no file at all */
static void test_shipped_file(void)
{
    static char file[4096];
    FILE *f = fopen("dist/bar.conf", "rb");
    long n = f ? (long)fread(file, 1, sizeof file - 1, f) : -1;
    barconf c, d;
    if (f)
        fclose(f);
    CHECK(n > 0);
    if (n <= 0)
        return;
    barconf_parse(file, n, &c);
    barconf_defaults(&d);
    CHECK_INT(c.bad, 0);
    CHECK_INT(c.start, d.start);
    CHECK_INT(c.nrun, 0);
    CHECK_INT(c.norder[BC_G_LEFT] + c.norder[BC_G_CENTRE] + c.norder[BC_G_RIGHT], 0);
    CHECK(!strcmp(c.clock, "%a %H:%M"));
    CHECK(!strcmp(c.clockalt, "%a %d %b %Y"));
}

void suite_barconf(void)
{
    test_shipped_file();
    test_plan();
    test_defaults();
    test_keys();
    test_bad();
}
