/* Lucide icon pens, pure half (amarchy/iconpen_core.c, desk_pen_plan_for; plan 2026-10-31-amarchy-lucide-icons.md L5, L6, L7). */
#include <stdio.h>
#include <string.h>
#include "../amarchy/iconpen_core.h"
#include "harness.h"

static void theme(gtheme *t)
{
    const char *s = "name = ipt\nbackground = #101820\nforeground = #e0e8f0\ndark_foreground = #607080\naccent = #ff8800\n"
                    "selection = #304860\nmuted = #8090a0\n";
    gt_parse(s, (long)strlen(s), t);
}

static void test_colours_and_fold(void)
{
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    int slot, depth;
    theme(&t);
    ip_colours(&t, rgb);
    CHECK(rgb[IP_FG100][0] == 0xe0 && rgb[IP_FG100][1] == 0xe8 && rgb[IP_FG100][2] == 0xf0);
    CHECK(rgb[IP_ACCENT100][0] == 0xff && rgb[IP_ACCENT100][2] == 0x00);
    CHECK(rgb[IP_SELBG][0] == 0x30 && rgb[IP_SELBG][1] == 0x48 && rgb[IP_SELBG][2] == 0x60);
    CHECK(rgb[IP_MUTED100][0] == 0x80);
    CHECK(rgb[IP_SPARE12][0] == 0x10 && rgb[IP_SPARE15][2] == 0x20);
    CHECK(rgb[IP_FG33][0] > 0x10 && rgb[IP_FG33][0] < rgb[IP_FG66][0] && rgb[IP_FG66][0] < rgb[IP_FG100][0]);
    /* fold: full roles land on their own pen, no fold ever lands on green or red */
    CHECK_INT(ip_fold(&t, IP_FG100, 3), DK_FG);
    CHECK_INT(ip_fold(&t, IP_ACCENT100, 3), DK_ACCENT);
    CHECK_INT(ip_fold(&t, IP_SELBG, 3), DK_SELECTION);
    CHECK_INT(ip_fold(&t, IP_MUTED100, 3), DK_MUTED);
    for (depth = 2; depth <= 8; depth++)
        for (slot = 0; slot < IP_PENS; slot++) {
            int p = ip_fold(&t, slot, depth);
            CHECK(p != DK_GREEN && p != DK_RED);
            if (depth == 2) CHECK(p >= 0 && p <= DK_ACCENT);
            if (depth >= 6) CHECK_INT(p, IP_FIRST + slot);
            if (depth == 4 || depth == 5) CHECK(slot >= 8 ? p == slot : (p >= 0 && p <= DK_MUTED));
        }
}

static void test_plan_no_overlap(void)
{
    unsigned char pens[DK_SLOTS];
    int depth, i, f, k;
    CHECK_INT(DK_SELECTION, 4);
    CHECK_INT(DK_MUTED, 5);
    for (depth = 2; depth <= 8; depth++) {
        desk_pen_plan p;
        desk_pen_plan_for(depth, &p);
        CHECK_INT(p.button, 4);
        CHECK_INT(p.backdrop, 5);
        CHECK(p.button != p.backdrop);
        CHECK(p.icon_first + p.icon_count <= (1 << depth));
        for (i = 0; i < p.icon_count; i++) {
            int pen = p.icon_first + i;
            CHECK(pen != p.button && pen != p.backdrop && pen >= 8);
            for (f = 0; f <= 1; f++) {
                desk_pens_for(f, pens);
                for (k = 0; k < DK_SLOTS; k++) CHECK(pens[k] != pen);
            }
        }
    }
}

typedef struct fake { int taken[64]; int mine[64]; int releases; } fake;
static int f_obtain(void *c, int pen, unsigned r, unsigned g, unsigned b)
{
    fake *f = c;
    (void)r; (void)g; (void)b;
    if (f->taken[pen] || f->mine[pen]) return -1;
    f->mine[pen] = 1;
    return pen;
}
static void f_release(void *c, int pen)
{
    fake *f = c;
    f->releases++;
    f->mine[pen] = 0;
}

static void test_holder(void)
{
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    fake f;
    ip_ops o;
    ip_holder h;
    char line[32];
    int s1, s2, i, left = 0;
    theme(&t);
    ip_colours(&t, rgb);
    memset(&f, 0, sizeof f);
    o.obtain = f_obtain; o.release = f_release; o.ctx = &f;
    ip_holder_init(&h);
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 6, rgb), 16);
    CHECK(ip_status_line(&h, line, sizeof line) > 0);
    CHECK_STR(line, "pens-held 16");
    CHECK(!ip_holder_stale(&h, &s1, 6));
    CHECK(ip_holder_stale(&h, &s2, 6));
    CHECK(ip_holder_stale(&h, &s1, 4));
    /* the screen closes and reopens: re-obtain releases the old pens first, holds 16 again */
    CHECK_INT(ip_holder_obtain(&h, &o, &s2, 6, rgb), 16);
    CHECK_INT(f.releases, 16);
    /* a taken pen is skipped, not held */
    ip_holder_release(&h, &o);
    f.taken[40] = 1;
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 6, rgb), 15);
    ip_status_line(&h, line, sizeof line);
    CHECK_STR(line, "pens-held 15");
    /* flat off releases everything */
    CHECK_INT(ip_holder_release(&h, &o), 15);
    ip_status_line(&h, line, sizeof line);
    CHECK_STR(line, "pens-held 0");
    for (i = 0; i < 64; i++) left += f.mine[i];
    CHECK_INT(left, 0);
    /* depth 5 holds pens 8..15 only; depth 3 holds none */
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 5, rgb), 8);
    CHECK(f.mine[8] && f.mine[15] && !f.mine[32]);
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 3, rgb), 0);
}

void suite_iconpen(void)
{
    test_colours_and_fold();
    test_plan_no_overlap();
    test_holder();
}
