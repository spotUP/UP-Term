/* A5 Installer margin patch (amarchy/inst_core.c): Installer 43.3 places its button rows 1 px apart and 1 px above
 * the window bottom. The patch makes both 4 px (its own side margin and button gap) and grows the window by the
 * added pixels: topaz 8 159 -> 165, helvetica 11 210 -> 216. The code image below carries the exact hunk-0 bytes
 * at the four signature sites; everything else is zero. */
#include <stdlib.h>
#include <string.h>
#include "../amarchy/inst_core.h"
#include "harness.h"

static unsigned char code[INST_CODE_MIN + 16];

static void put(unsigned long at, const char *hex)
{
    while (hex[0] && hex[1]) {
        unsigned v;
        char b[3];
        b[0] = hex[0]; b[1] = hex[1]; b[2] = 0;
        v = (unsigned)strtoul(b, 0, 16);
        code[at++] = (unsigned char)v;
        hex += 2;
    }
}

static void load(void)
{
    memset(code, 0, sizeof code);
    put(0xc0ee, "322c5dce9240534135410006");
    put(0xc274, "322c5dce924053413541000637410006");
    put(0x441c, "2c6c003c4eaeff34");
    put(0x02ca, "52402200e941d26800145e41");
}

/* Installer's row placement (0xc0ee): y = bottom - h - margin; the next row's bottom is y. */
static void rows(int win_h, int border_bottom, int tx_h, int *help_margin, int *row_gap)
{
    int m = inst_row_margin(code);
    int h = tx_h + 5;
    int bottom = win_h - border_bottom;
    int help_y = bottom - h - m;
    int pa_y = help_y - h - m;
    *help_margin = bottom - (help_y + h);
    *row_gap = help_y - (pa_y + h);
}

static void test_signature(void)
{
    load();
    CHECK_INT(inst_sig(code, sizeof code), INST_SIG_ORIG);
    CHECK_INT(inst_sig(code, INST_CODE_MIN - 1), INST_SIG_UNKNOWN);
    CHECK_INT(inst_sig(0, sizeof code), INST_SIG_UNKNOWN);
    inst_apply(code);
    CHECK_INT(inst_sig(code, sizeof code), INST_SIG_PATCHED);
    load();
    code[0x441c + 7] = 0x30;                        /* OpenWindowTagList, not the plain OpenWindow */
    CHECK_INT(inst_sig(code, sizeof code), INST_SIG_UNKNOWN);
    load();
    code[0xc27a] = 0x55;                            /* one site differs: leave both alone */
    CHECK_INT(inst_sig(code, sizeof code), INST_SIG_UNKNOWN);
    load();
    code[0x02ca + 11] = 0x00;                       /* other size formula: the height gate would lie */
    CHECK_INT(inst_sig(code, sizeof code), INST_SIG_UNKNOWN);
}

static void test_margins(void)
{
    int m, g;
    load();
    rows(159, 2, 8, &m, &g);
    CHECK_INT(m, 1);                                /* the bug */
    CHECK_INT(g, 1);
    inst_apply(code);
    CHECK_INT(code[0xc0f4], 0x59);                  /* subq.w #4,d1 */
    CHECK_INT(code[0xc27a], 0x59);
    CHECK_INT(code[0xc0f5], 0x41);
    rows(165, 2, 8, &m, &g);
    CHECK_INT(m, INST_K);
    CHECK_INT(g, INST_K);
    CHECK_INT(INST_K, 4);
}

static void test_grow(void)
{
    int t, h;
    CHECK_INT(inst_grow(), 6);
    CHECK_INT(inst_font_y(159), 8);
    CHECK_INT(inst_font_y(210), 11);
    CHECK_INT(inst_font_y(160), 0);
    CHECK_INT(inst_font_y(17 * 7 + 23), 0);
    CHECK_INT(inst_box(48, 159, 256, &t, &h), 1);   /* topaz 8 on a 256-line screen */
    CHECK_INT(h, 165);
    CHECK_INT(t, 45);
    CHECK_INT(inst_box(23, 210, 256, &t, &h), 1);   /* helvetica 11 */
    CHECK_INT(h, 216);
    CHECK_INT(t, 20);
    CHECK_INT(inst_box(240, 159, 256, &t, &h), 1);  /* clamped to the screen bottom */
    CHECK_INT(t, 91);
    CHECK_INT(inst_box(0, 252, 256, &t, &h), 0);    /* grown window would not fit: no patch */
}

void suite_inst(void)
{
    test_signature();
    test_margins();
    test_grow();
}
