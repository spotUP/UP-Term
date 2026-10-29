/* W43: the ScreenMode prefs parser and the wait-for-final-mode decision (amarchy/wbmode_core.c). The bytes are the
 * rig's own files (tools/rig/rig.py screenmode_prefs_bytes: FORM/PREF/PRHD(6)/SCRM(28)). */
#include "../amarchy/wbmode_core.h"
#include "harness.h"

#define RTG_ID 0x50041303UL
#define PAL_ID 0x00029000UL

static void put32(unsigned char *p, unsigned long v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void put16(unsigned char *p, unsigned v) { p[0] = v >> 8; p[1] = v; }

/* the same 62 bytes screenmode_prefs_bytes makes */
static long make_prefs(unsigned char *b, unsigned long id, unsigned w, unsigned h, unsigned d)
{
    int i;
    for (i = 0; i < 62; i++)
        b[i] = 0;
    b[0] = 'F'; b[1] = 'O'; b[2] = 'R'; b[3] = 'M';
    put32(b + 4, 54);
    b[8] = 'P'; b[9] = 'R'; b[10] = 'E'; b[11] = 'F';
    b[12] = 'P'; b[13] = 'R'; b[14] = 'H'; b[15] = 'D';
    put32(b + 16, 6);
    b[26] = 'S'; b[27] = 'C'; b[28] = 'R'; b[29] = 'M';
    put32(b + 30, 28);
    put32(b + 34 + 16, id);
    put16(b + 34 + 20, w);
    put16(b + 34 + 22, h);
    put16(b + 34 + 24, d);
    put16(b + 34 + 26, 1);
    return 62;
}

static void test_parse(void)
{
    unsigned char b[64];
    wbm_prefs p;
    long n = make_prefs(b, RTG_ID, 0xffff, 0xffff, 24);
    CHECK_INT(wbm_parse_prefs(b, n, &p), 1);
    CHECK_INT(p.id == RTG_ID, 1);
    CHECK_INT(p.width, 0xffff);
    CHECK_INT(p.depth, 24);
    n = make_prefs(b, PAL_ID, 0x280, 0x100, 4);
    CHECK_INT(wbm_parse_prefs(b, n, &p), 1);
    CHECK_INT(p.id == PAL_ID, 1);
    CHECK_INT(p.width, 0x280);
    CHECK_INT(p.height, 0x100);
    CHECK_INT(p.depth, 4);
    CHECK_INT(wbm_parse_prefs(b, 40, &p), 0);          /* SCRM cut short */
    CHECK_INT(wbm_parse_prefs(b, 8, &p), 0);           /* too short for a header */
    CHECK_INT(wbm_parse_prefs(0, 62, &p), 0);
    b[8] = 'X';
    CHECK_INT(wbm_parse_prefs(b, 62, &p), 0);          /* not PREF */
    b[8] = 'P';
    b[26] = 'Z';
    CHECK_INT(wbm_parse_prefs(b, 62, &p), 0);          /* no SCRM chunk */
}

static void test_ready(void)
{
    CHECK_INT(wb_mode_ready(0, 0, PAL_ID, 0, 30), WBM_READY);                 /* no prefs file */
    CHECK_INT(wb_mode_ready(WBM_INVALID_ID, 1, PAL_ID, 0, 30), WBM_READY);    /* prefs say "any" */
    CHECK_INT(wb_mode_ready(RTG_ID, 1, RTG_ID, 0, 30), WBM_READY);            /* already final */
    CHECK_INT(wb_mode_ready(RTG_ID, 1, PAL_ID, 0, 30), WBM_WAIT);
    CHECK_INT(wb_mode_ready(RTG_ID, 1, PAL_ID, 29, 30), WBM_WAIT);
    CHECK_INT(wb_mode_ready(RTG_ID, 1, PAL_ID, 30, 30), WBM_TIMEOUT);
    CHECK_INT(wb_mode_ready(RTG_ID, 1, PAL_ID, 99, 30), WBM_TIMEOUT);
    CHECK_INT(wb_mode_ready(RTG_ID, 1, RTG_ID, 99, 30), WBM_READY);           /* the mode arrived late: READY wins */
}

void suite_wbmode(void)
{
    test_parse();
    test_ready();
}
