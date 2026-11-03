/* The amarchy-flat state block (amarchy/flat_core.c; plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0). The block
 * is shared with Amarchy through the public semaphore, so its offsets are a contract: pinned here byte by byte. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/flat_core.h"
#include "../amarchy/flat_glyph.h"
#include "../amarchy/desk_core.h"
#define ROMSPAN_VEC
#include "../device/romspan.h"
#include "harness.h"

static void test_layout(void)
{
    CHECK_INT(sizeof(flat_state), FLAT_STATE_SIZE);
    CHECK_INT(offsetof(flat_state, version), 0);
    CHECK_INT(offsetof(flat_state, size), 2);
    CHECK_INT(offsetof(flat_state, enabled), 4);
    CHECK_INT(offsetof(flat_state, style), 6);
    CHECK_INT(offsetof(flat_state, held), 8);
    CHECK_INT(offsetof(flat_state, still_chained), 10);
    CHECK_INT(offsetof(flat_state, calls), 12);
    CHECK_INT(offsetof(flat_state, pen_text), 16);
    CHECK_INT(offsetof(flat_state, pen_line), 17);
    CHECK_INT(offsetof(flat_state, pen_back), 18);
    CHECK_INT(offsetof(flat_state, pen_outline), 19);
    CHECK_INT(offsetof(flat_state, pen_button), 20);
    CHECK_INT(offsetof(flat_state, pen_ptext), 21);
    CHECK_INT(offsetof(flat_state, pen_dim), 22);
    CHECK_INT(offsetof(flat_state, pen_dimtext), 23);
    CHECK_INT(offsetof(flat_state, pen_knob), 24);
    CHECK_INT(offsetof(flat_state, pen_inbar), 25);
    CHECK_INT(offsetof(flat_state, inbar_rgb), 26);
    CHECK_INT(FLAT_STATE_SIZE, 32);
    CHECK_STR(FLAT_SEM_NAME, "Amarchy.Flat");
    CHECK_INT(FLAT_VERSION, 3);
}

static void test_init_check(void)
{
    flat_state s;
    unsigned char raw[40];
    memset(&s, 0xAA, sizeof s);
    flat_init(&s);
    CHECK_INT(s.version, FLAT_VERSION);
    CHECK_INT(s.size, FLAT_STATE_SIZE);
    CHECK_INT(s.enabled | s.style | s.held | s.still_chained | s.calls | s.pen_text | s.pen_line | s.pen_back | s.pen_outline | s.pen_button | s.pen_ptext | s.pen_dim | s.pen_dimtext | s.pen_knob | s.pen_inbar | s.inbar_rgb[0] | s.inbar_rgb[1] | s.inbar_rgb[2], 0);
    CHECK_INT(flat_check(&s, sizeof s), FLAT_OK);
    CHECK_INT(flat_check(&s, sizeof s + 20), FLAT_OK);
    CHECK_INT(flat_check(&s, FLAT_STATE_SIZE - 1), FLAT_ERR_SIZE);      /* fewer readable bytes than the layout */
    CHECK_INT(flat_check(&s, 3), FLAT_ERR_SIZE);                         /* not even the header */
    CHECK_INT(flat_check(0, 40), FLAT_ERR_SIZE);
    {   flat_state old;                                                  /* a v1 block (20 bytes) is refused by version, then by size */
        memset(&old, 0, sizeof old);
        old.version = 1; old.size = 20;
        CHECK_INT(flat_check(&old, 20), FLAT_ERR_VERSION);
        old.version = FLAT_VERSION;
        CHECK_INT(flat_check(&old, 20), FLAT_ERR_SIZE);
    }
    s.version = FLAT_VERSION + 1;
    CHECK_INT(flat_check(&s, sizeof s), FLAT_ERR_VERSION);               /* another version: refused */
    s.version = 0;
    CHECK_INT(flat_check(&s, sizeof s), FLAT_ERR_VERSION);
    flat_init(&s);
    s.size = FLAT_STATE_SIZE + 4;                                        /* a grown block of the same version: refused */
    CHECK_INT(flat_check(&s, sizeof s + 8), FLAT_ERR_SIZE);
    s.size = FLAT_STATE_SIZE - 4;
    CHECK_INT(flat_check(&s, sizeof s), FLAT_ERR_SIZE);
    /* version is judged before size: a block of another version with a bad size is a version error */
    s.version = 7;
    CHECK_INT(flat_check(&s, sizeof s), FLAT_ERR_VERSION);
    /* the bytes of a valid block as Amarchy writes them: big-endian on the Amiga, native on the host */
    memset(raw, 0, sizeof raw);
    flat_init((flat_state *)raw);
    ((flat_state *)raw)->calls = 5;
    CHECK_INT(flat_check(raw, sizeof raw), FLAT_OK);
}

static void test_status(void)
{
    flat_state s;
    char line[96];
    flat_init(&s);
    CHECK_STR(flat_status_word(0, 0), "absent");
    CHECK_STR(flat_status_word(&s, 1), "off");
    s.enabled = 1;
    CHECK_STR(flat_status_word(&s, 1), "on");
    s.held = 1;
    CHECK_STR(flat_status_word(&s, 1), "held");                         /* a kill switch beats the switch */
    CHECK_STR(flat_status_word(&s, 0), "absent");
    CHECK_INT(flat_status_line(0, 0, 0, line, sizeof line) > 0, 1);
    CHECK_STR(line, "flat=absent");
    s.held = 0;
    s.calls = 12;
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=on calls=12 chained=0");
    s.still_chained = 1;
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=on calls=12 chained=1");
    s.version = 9;                                                       /* a block of another layout reads as absent */
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=absent bad=version");
    flat_init(&s);
    s.size = 99;
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=absent bad=size");
    CHECK_INT(flat_status_line(&s, sizeof s, 1, line, 47), 0);          /* a buffer under 48 bytes is refused */
}

static void test_exit(void)
{
    flat_state s;
    flat_init(&s);
    CHECK_INT(flat_can_exit(&s), 1);
    CHECK_INT(flat_can_exit(0), 1);
    s.still_chained = 1;
    CHECK_INT(flat_can_exit(&s), 0);                                     /* never unload while a foreign hook sits on top */
}


/* FLAT-C1: the hook swap over a fake IClass dispatcher Hook (entry / sub entry / data, as struct Hook lays them out). */
typedef struct { uintptr_t entry, sub, data; } fake_hook;

static void test_swap(void)
{
    fake_hook hk = {0x1118, 0x2F02, 0xBDC4}, snap;
    flat_swap w;
    flat_state st;
    uintptr_t foreign_saved;
    flat_init(&st);
    flat_swap_init(&w);
    snap = hk;
    /* install: sub entry swapped, entry and data untouched */
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0x9000), FLAT_OK);
    CHECK_INT(hk.sub, 0x9000);
    CHECK_INT(hk.entry, 0x1118);
    CHECK_INT(hk.data, 0xBDC4);
    CHECK_INT(w.saved, 0x2F02);
    /* double install refused, nothing changes (saved stays the ROM routine, not our own trampoline) */
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0x9100), FLAT_ERR_DOUBLE);
    CHECK_INT(hk.sub, 0x9000);
    CHECK_INT(w.saved, 0x2F02);
    /* clean remove: byte-equal restore */
    CHECK_INT(flat_swap_remove(&w, &hk.sub, &st), FLAT_REMOVED);
    CHECK_INT(memcmp(&hk, &snap, sizeof hk), 0);
    CHECK_INT(st.still_chained, 0);
    CHECK_INT(flat_swap_remove(&w, &hk.sub, &st), FLAT_NOT_INSTALLED);
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0), FLAT_ERR_ARG);
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0x2F02), FLAT_ERR_DOUBLE);  /* a slot that already holds ours */
    /* a foreign patcher chains on top: still_chained, inert, its hook is never overwritten */
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0x9000), FLAT_OK);
    foreign_saved = hk.sub;
    hk.sub = 0xA000;
    CHECK_INT(flat_swap_remove(&w, &hk.sub, &st), FLAT_STILL_CHAINED);
    CHECK_INT(hk.sub, 0xA000);
    CHECK_INT(foreign_saved, 0x9000);
    CHECK_INT(st.still_chained, 1);
    CHECK_INT(w.inert, 1);
    CHECK_INT(flat_can_exit(&st), 0);
    CHECK_INT(flat_swap_install(&w, &hk.sub, 0x9200), FLAT_ERR_DOUBLE);  /* still installed: no second swap either */
    CHECK_INT(hk.sub, 0xA000);
    /* the foreign patcher removes itself: the next remove is clean again */
    hk.sub = foreign_saved;
    CHECK_INT(flat_swap_remove(&w, &hk.sub, &st), FLAT_REMOVED);
    CHECK_INT(memcmp(&hk, &snap, sizeof hk), 0);
    CHECK_INT(st.still_chained, 0);
    CHECK_INT(flat_can_exit(&st), 1);
    CHECK_INT(flat_swap_remove(&w, &hk.sub, 0), FLAT_NOT_INSTALLED);     /* st may be NULL */
}

static void test_tramp(void)
{
    unsigned char c[FLAT_TRAMP_SIZE];
    flat_tramp_build(c, 0x01020304u, 0xAABBCCDDu);
    CHECK_INT(c[0] << 8 | c[1], 0x2F08);        /* move.l a0,-(sp) */
    CHECK_INT(c[2] << 8 | c[3], 0x207A);        /* movea.l d16(pc),a0 */
    CHECK_INT(c[4] << 8 | c[5], 12);            /* ext word at 4 + 12 = 16: the counter address */
    CHECK_INT(c[6] << 8 | c[7], 0x5290);        /* addq.l #1,(a0) */
    CHECK_INT(c[8] << 8 | c[9], 0x205F);        /* movea.l (sp)+,a0 */
    CHECK_INT(c[10] << 8 | c[11], 0x2F3A);      /* move.l d16(pc),-(sp) */
    CHECK_INT(c[12] << 8 | c[13], 8);           /* ext word at 12 + 8 = 20: the saved routine */
    CHECK_INT(c[14] << 8 | c[15], 0x4E75);      /* rts: tail jump, registers and stack as the caller left them */
    CHECK_INT(c[16] == 1 && c[17] == 2 && c[18] == 3 && c[19] == 4, 1);
    CHECK_INT(c[20] == 0xAA && c[21] == 0xBB && c[22] == 0xCC && c[23] == 0xDD, 1);
    CHECK_INT(flat_conv(40), FLAT_CONV_STACK);  /* 3.1: hook / object / message on the C stack */
    CHECK_INT(flat_conv(47), FLAT_CONV_REG);    /* 3.2: a0 / a2 / a1 */
}

/* FLAT-D4: version / checksum gate, install and remove of the two LVO slots, the flat-draw decision per element. */
typedef struct { unsigned char jmp[6]; } fake_entry;

static void test_dopus_gate(void)
{
    flat_dopus_build tab[2] = { { 73, 8, { 0x1111, 0x2222 } }, { 73, 9, { 0, 0 } } };
    uint32_t sums[2] = { 0x1111, 0x2222 };
    unsigned char fn[8] = { 0, 0, 0, 1, 0, 0, 0, 2 };
    unsigned char e[6] = { 0x4E, 0xF9, 0x00, 0x12, 0x34, 0x56 };
    int why = -1, n;
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 8, sums, &why), 3);           /* known build, both sums match: both elements */
    CHECK_INT(why, FLAT_GATE_OK);
    sums[1] = 0x9999;
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 8, sums, &why), 1);           /* one routine differs: only the other is patched */
    sums[0] = 0x8888;
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 8, sums, &why), 0);           /* no sum matches: inert */
    CHECK_INT(why, FLAT_GATE_SUM);
    CHECK_INT(flat_dopus_gate(tab, 2, 74, 0, sums, &why), 0);           /* unknown version: inert */
    CHECK_INT(why, FLAT_GATE_UNKNOWN);
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 9, sums, &why), 0);           /* row without measured sums never matches */
    CHECK_INT(why, FLAT_GATE_UNMEASURED);
    sums[0] = sums[1] = 0;
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 9, sums, &why), 0);           /* a zero sum must not match an unmeasured row */
    CHECK_INT(flat_dopus_gate(tab, 2, 73, 8, 0, &why), 0);
    CHECK_INT(flat_dopus_gate(0, 0, 73, 8, sums, &why), 0);
    CHECK_INT(why, FLAT_GATE_UNKNOWN);
    /* the built-in table: 73.8 with the sums measured on the rig (STATUS sums=); a wrong sum stays inert */
    CHECK(flat_dopus_table(&n) != 0 && n >= 1);
    sums[0] = sums[1] = 1;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 73, 8, sums, &why), 0);
    CHECK_INT(why, FLAT_GATE_SUM);
    sums[0] = 0x780fd46bu; sums[1] = 0xba2cf172u;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 73, 8, sums, &why), 3);
    CHECK_INT(why, FLAT_GATE_OK);
    /* sum and jump parsing */
    CHECK_INT(flat_sum(fn, 8), 3);
    CHECK_INT(flat_sum(fn, 3), 1);                                      /* never 0 */
    {   /* DrawFieldBox prologue, as measured on dopus5.library 73.8: MOVE.L abs.l,-(A7) with a relocated operand */
        const unsigned char r1[16] = {0x2f,0x0a,0x2f,0x39,0x10,0x09,0xad,0x38,0x2f,0x0a,0x2f,0x09,0x2f,0x08,0x4e,0xba};
        unsigned char r2[16], r3[16];
        memcpy(r2, r1, 16); r2[4] = 0x20; r2[7] = 0x00;                 /* loaded elsewhere: operand differs */
        memcpy(r3, r1, 16); r3[11] = 0x08;                              /* a real opcode change */
        CHECK_INT(flat_sum_reloc(r1, 16) == flat_sum_reloc(r2, 16), 1);
        CHECK_INT(flat_sum_reloc(r1, 16) != flat_sum_reloc(r3, 16), 1);
    }
    CHECK_INT(flat_jmp_target(e), 0x123456);
    e[1] = 0xF8;
    CHECK_INT(flat_jmp_target(e), 0);                                   /* not a JMP abs.l: foreign code, no patch */
    CHECK_INT(flat_jmp_target(0), 0);
}

static void test_dopus_swap(void)
{
    uintptr_t slot[FLAT_EL_COUNT] = { 0x5000, 0x6000 }, snap[FLAT_EL_COUNT];
    flat_swap w[FLAT_EL_COUNT];
    flat_state st;
    int el;
    flat_init(&st);
    memcpy(snap, slot, sizeof slot);
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        flat_swap_init(&w[el]);
        CHECK_INT(flat_swap_install(&w[el], &slot[el], 0x7000 + el), FLAT_OK);
        CHECK_INT(slot[el], 0x7000 + el);
        CHECK_INT(flat_swap_install(&w[el], &slot[el], 0x7100), FLAT_ERR_DOUBLE);
    }
    CHECK_INT(flat_swap_remove(&w[0], &slot[0], &st), FLAT_REMOVED);
    CHECK_INT(slot[0], snap[0]);
    slot[1] = 0xA000;                                                   /* a foreign patcher on the second entry */
    CHECK_INT(flat_swap_remove(&w[1], &slot[1], &st), FLAT_STILL_CHAINED);
    CHECK_INT(slot[1], 0xA000);
    CHECK_INT(st.still_chained, 1);
    slot[1] = 0x7001;
    CHECK_INT(flat_swap_remove(&w[1], &slot[1], &st), FLAT_REMOVED);
    CHECK_INT(memcmp(slot, snap, sizeof slot), 0);
}

static void test_dopus_decide(void)
{
    flat_state s;
    char line[96];
    const uint32_t sm[FLAT_EL_COUNT] = { 0x0001abcdu, 0xdeadbeefu };
    flat_init(&s);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0), FLAT_DRAW_STOCK);       /* flat off: stock */
    s.enabled = 1;
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0), FLAT_DRAW_SKIP);        /* flat on, flat pens: no shadow L */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 1, 0, 0), FLAT_DRAW_SKIP);        /* recessed too */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 0, 0, 0), FLAT_DRAW_SKIP);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 2, 0), FLAT_DRAW_STOCK);       /* stock pens (shine != back): stock bevel */
    s.held = 1;
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0), FLAT_DRAW_STOCK);       /* kill switch */
    s.held = 0;
    CHECK_INT(flat_dopus_decide(&s, 5, 0, 0, 0), FLAT_DRAW_STOCK);
    CHECK_INT(flat_dopus_decide(0, FLAT_EL_DRAWBOX, 0, 0, 0), FLAT_DRAW_STOCK);
    flat_dopus_status_line(0, 0, 0, 0, 0, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=absent");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNKNOWN, 0, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unknown");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNMEASURED, 0, 0, 0, 0, sm, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unmeasured sums=0001abcd,deadbeef");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_OK, 3, 1, 12, 9, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 patched=3 calls=12 flat=9");
}

/* The whole path as flat.c runs it: gate -> install over a fake jump table -> a patched call decides per call -> remove. */
static unsigned long g_skipped, g_stock;
static void patched_call(const flat_state *s, int el, int shine, int back)
{
    if (flat_dopus_decide(s, el, 0, shine, back) == FLAT_DRAW_SKIP)
        g_skipped++;
    else
        g_stock++;
}

static void test_dopus_path(void)
{
    flat_dopus_build tab[1] = { { 73, 8, { 0xAB, 0xCD } } };
    uint32_t sums[2] = { 0xAB, 0xCD };
    uintptr_t slot[2] = { 0x5000, 0x6000 };
    flat_swap w[2];
    flat_state st;
    int why, mask, el;
    flat_init(&st);
    st.enabled = 1;
    mask = flat_dopus_gate(tab, 1, 73, 8, sums, &why);
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        flat_swap_init(&w[el]);
        if (mask & (1 << el))
            flat_swap_install(&w[el], &slot[el], 0x7000 + el);
    }
    CHECK_INT(slot[0], 0x7000);
    CHECK_INT(slot[1], 0x7001);
    patched_call(&st, FLAT_EL_DRAWBOX, 0, 0);
    patched_call(&st, FLAT_EL_FIELDBOX, 0, 0);
    st.enabled = 0;
    patched_call(&st, FLAT_EL_DRAWBOX, 0, 0);
    CHECK_INT(g_skipped, 2);
    CHECK_INT(g_stock, 1);
    for (el = 0; el < FLAT_EL_COUNT; el++)
        flat_swap_remove(&w[el], &slot[el], &st);
    CHECK_INT(slot[0], 0x5000);
    CHECK_INT(slot[1], 0x6000);
    /* an unknown build: nothing is installed, slots stay stock */
    sums[0] = 0x1;
    mask = flat_dopus_gate(tab, 1, 73, 8, sums, &why);
    CHECK_INT(mask & 1, 0);
}

/* ---- FLAT-C2: glyph geometry against pixel goldens, and the dispatcher decision (what flat.c's sysiclass dispatcher
 * runs: count, flat_glyph_draw, else the old hook). The canvas is a char grid; a pen value is its character. */
#define CV 40
static char cv[CV][CV + 1];
static int g_old_calls, g_disp_calls;
static int g_frame_pen = -1;                            /* the frame pen the fake dispatcher passes (-1: none known) */

static void cv_clear(void)
{
    int y;
    for (y = 0; y < CV; y++) {
        memset(cv[y], '?', CV);
        cv[y][CV] = 0;
    }
}

static void cv_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    int x, y;
    (void)ctx;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < CV && y >= 0 && y < CV)
                cv[y][x] = (char)pen;
}

/* the fake dispatcher: the same three steps as flat.c c2_disp_reg / c2_disp_stk */
static int fake_dispatch(const flat_state *st, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy, int w, int h)
{
    g_disp_calls++;
    if (flat_glyph_draw(st, which, method, ids, ox, oy, w, h, g_frame_pen, 1, cv_fill, 0))
        return 1;
    g_old_calls++;                                      /* the saved routine */
    return 0;
}

static void glyph_state(flat_state *st)
{
    flat_init(st);
    st->enabled = 1;
    st->pen_text = '#';
    st->pen_line = '+';
    st->pen_back = '.';
}

static void test_glyph_goldens(void)
{
    static const struct { const char *name; unsigned long which; int w, h; const char *rows; } gold[] = {
#include "data/flat_glyph_golden.inc"
    };
    flat_state st;
    unsigned i;
    glyph_state(&st);
    CHECK_INT(sizeof gold / sizeof gold[0], 24);                    /* 8 glyphs x 3 box sizes */
    for (i = 0; i < sizeof gold / sizeof gold[0]; i++) {
        char got[CV * (CV + 1) + 1], *o = got;
        const char *r = gold[i].rows;
        int y, x, ok = 1;
        cv_clear();
        CHECK_INT(fake_dispatch(&st, gold[i].which, FLAT_IM_DRAW, 0, 3, 2, gold[i].w, gold[i].h), 1);
        for (y = 0; y < gold[i].h && ok; y++) {                      /* the box sits at (3, 2); the golden row is w chars and '|' */
            for (x = 0; x < gold[i].w; x++)
                *o++ = cv[2 + y][3 + x];
            *o++ = '|';
            *o = 0;
            if (strncmp(o - gold[i].w - 1, r, gold[i].w + 1)) {
                ok = 0;
                printf("  glyph %s %dx%d row %d: got [%.*s] want [%.*s]\n", gold[i].name, gold[i].w, gold[i].h, y,
                       gold[i].w, o - gold[i].w - 1, gold[i].w, r);
            }
            r += gold[i].w + 1;
        }
        CHECK(ok);
        CHECK(cv[1][3] == '?' && cv[2 + gold[i].h][3] == '?' && cv[2][2] == '?' && cv[2][3 + gold[i].w] == '?');   /* nothing outside the box */
    }
}

/* FLAT-GADGET-BG: the window gadgets' box takes the frame pen; selected and menu glyphs keep their own background */
static void test_glyph_frame_bg(void)
{
    flat_state st;
    glyph_state(&st);
    g_frame_pen = '7';
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);              /* normal */
    CHECK_INT(cv[0][0], '7');
    CHECK_INT(cv[11][13], '7');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 5, 0, 0, 14, 12);              /* inactive */
    CHECK_INT(cv[0][0], '7');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 14, 12);              /* selected: inverted, not the frame */
    CHECK_INT(cv[0][0], '#');
    cv_clear();
    fake_dispatch(&st, FLAT_W_MENUCHECK, FLAT_IM_DRAW, 0, 0, 0, 14, 12);          /* menu glyph: the back pen */
    CHECK_INT(cv[0][0], '.');
    g_frame_pen = -1;
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);              /* no frame pen known: the back pen */
    CHECK_INT(cv[0][0], '.');
}

static void test_glyph_states(void)
{
    flat_state st;
    flat_prim n[FLAT_GLYPH_MAX_PRIMS], s[FLAT_GLYPH_MAX_PRIMS], d[FLAT_GLYPH_MAX_PRIMS];
    int cn, cs, cd, i;
    glyph_state(&st);
    cn = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_NORMAL, n, FLAT_GLYPH_MAX_PRIMS);
    cs = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_SELECTED, s, FLAT_GLYPH_MAX_PRIMS);
    cd = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_INACTIVE, d, FLAT_GLYPH_MAX_PRIMS);
    CHECK_INT(cn, 2);                                               /* the box and one solid block */
    CHECK_INT(cs, cn);
    CHECK_INT(cd, cn);
    CHECK_INT(n[0].role, FLAT_ROLE_FRAME);                          /* first rect = the box in the frame's colour */
    CHECK_INT(s[0].role, FLAT_ROLE_TEXT);                           /* selected inverts */
    CHECK_INT(d[0].role, FLAT_ROLE_FRAME);
    for (i = 1; i < cn; i++) {
        CHECK_INT(n[i].role, FLAT_ROLE_TEXT);
        CHECK_INT(s[i].role, FLAT_ROLE_BACK);
        CHECK_INT(d[i].role, FLAT_ROLE_LINE);                       /* inactive: only the pen differs */
        CHECK(n[i].x0 == s[i].x0 && n[i].y1 == d[i].y1);            /* same geometry in every state */
    }
    CHECK_INT(flat_glyph_state(0), FLAT_G_NORMAL);
    CHECK_INT(flat_glyph_state(1), FLAT_G_SELECTED);
    CHECK_INT(flat_glyph_state(5), FLAT_G_INACTIVE);
    CHECK_INT(flat_glyph_state(7), FLAT_G_INACTIVE);
    /* drawn through the dispatcher: the selected box is the text pen with background-pen lines */
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 14, 12);
    CHECK_INT(cv[0][0], '#');
    CHECK_INT(cv[5][5], '.');
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 5, 0, 0, 14, 12);
    CHECK_INT(cv[0][0], '.');
    CHECK_INT(cv[5][5], '+');
    /* solid blocks: the inside of the close square is filled (FLAT-SOLID-FILL) */
    cv_clear();
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);
    CHECK_INT(cv[5][6], '#');
    CHECK_INT(cv[0][0], '.');
}

/* FLAT-SOLID-FILL: every window glyph is drawn as filled blocks in every state, and an inactive window differs
 * from an active one in colour only (same rectangles, only the role of the glyph pen changes). */
static void test_glyph_solid_and_state_equal(void)
{
    static const unsigned long win[] = { FLAT_W_CLOSE, FLAT_W_DEPTH, FLAT_W_ZOOM, FLAT_W_SIZE, FLAT_W_ICONIFY };
    static const int dim[2][2] = { { 14, 12 }, { 24, 20 } };
    static const int states[2] = { FLAT_G_NORMAL, FLAT_G_INACTIVE };
    unsigned i;
    int k, z, j;
    for (i = 0; i < sizeof win / sizeof win[0]; i++) {
        for (z = 0; z < 2; z++) {
            flat_prim p[2][FLAT_GLYPH_MAX_PRIMS];
            int c[2];
            for (k = 0; k < 2; k++)
                c[k] = flat_glyph_plan(win[i], dim[z][0], dim[z][1], states[k], p[k], FLAT_GLYPH_MAX_PRIMS);
            CHECK(c[0] > 1);
            CHECK_INT(c[1], c[0]);                                  /* same number of rectangles */
            for (k = 0; k < 2; k++) {
                int solid = 0;                                      /* a glyph-pen rectangle at least 2 x 2: a block, not a line */
                for (j = 1; j < c[k]; j++)
                    if (p[k][j].role != FLAT_ROLE_FRAME && p[k][j].role != FLAT_ROLE_BACK &&
                        p[k][j].x1 - p[k][j].x0 >= 1 && p[k][j].y1 - p[k][j].y0 >= 1)
                        solid = 1;
                CHECK(solid);
            }
            for (j = 0; j < c[0]; j++) {                            /* identical geometry; only the glyph pen role moves */
                CHECK(p[0][j].x0 == p[1][j].x0 && p[0][j].y0 == p[1][j].y0 &&
                      p[0][j].x1 == p[1][j].x1 && p[0][j].y1 == p[1][j].y1);
                if (p[0][j].role != p[1][j].role)
                    CHECK(p[0][j].role == FLAT_ROLE_TEXT && p[1][j].role == FLAT_ROLE_LINE);
            }
        }
    }
}

static void test_glyph_route(void)
{
    flat_state st;
    static const unsigned long unknown[] = { 0x04, 0x0E, 0x0F, 0x05, 0x1B, 0x2A, 0xFFFF };
    static const unsigned long other[] = { 0x101UL /* OM_NEW */, 0x103UL /* OM_GET */, 0x203UL /* IM_HITTEST */,
                                           0x204UL /* IM_ERASE */, 0x206UL /* IM_DRAWFRAME */, 0x207UL /* IM_FRAMEBOX */, 0 };
    unsigned i;
    glyph_state(&st);
    g_old_calls = g_disp_calls = 0;
    for (i = 0; i < sizeof unknown / sizeof unknown[0]; i++) {
        cv_clear();
        CHECK_INT(fake_dispatch(&st, unknown[i], FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
        CHECK_INT(cv[0][0], '?');                                    /* nothing drawn */
    }
    CHECK_INT(g_old_calls, (int)(sizeof unknown / sizeof unknown[0]));      /* every unknown Which reached the old hook */
    g_old_calls = 0;
    for (i = 0; i < sizeof other / sizeof other[0]; i++) {
        cv_clear();
        CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, other[i], 0, 0, 0, 14, 12), 0);
        CHECK_INT(cv[0][0], '?');
    }
    CHECK_INT(g_old_calls, (int)(sizeof other / sizeof other[0]));          /* every other method too */
    /* switch off, held, box too small: the old hook, nothing drawn */
    g_old_calls = 0;
    cv_clear();
    st.enabled = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
    st.enabled = 1;
    st.held = 1;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 0);
    st.held = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, FLAT_GLYPH_MIN_SIZE - 1, 12), 0);
    CHECK_INT(fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, FLAT_GLYPH_MIN_SIZE - 1), 0);
    CHECK_INT(g_old_calls, 4);
    CHECK_INT(cv[0][0], '?');
    CHECK_INT(flat_glyph_route(0, FLAT_W_CLOSE, FLAT_IM_DRAW), 0);
    /* and the draw path does not reach the old hook */
    g_old_calls = 0;
    CHECK_INT(fake_dispatch(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 0, 0, 0, 14, 12), 1);
    CHECK_INT(g_old_calls, 0);
    CHECK_INT(cv[0][0], '.');
}

/* 3.1 sysiclass answers no OM_GET: the gadget type of the drawn window gadget gives SYSIA_Which */

struct outline_cv_t { unsigned char *cv; int w, h, bad; };
static void outline_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct outline_cv_t *c = ctx;
    int x, y;
    if (pen != 5 || x0 < 0 || y0 < 0 || x1 >= c->w || y1 >= c->h)
        c->bad++;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < c->w && y < c->h && c->cv[y * c->w + x]++)
                c->bad++;                                                   /* painted twice */
}

struct frame_rec_t { int n, r[8][5]; };

static void round_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    int (*p)[60] = (int (*)[60])ctx, x, y;
    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) p[y][x] = pen;
}
static int round_read(void *ctx, int x, int y)
{
    int (*p)[60] = (int (*)[60])ctx;
    return x < 0 || y < 0 || x >= 60 || y >= 40 ? -1 : p[y][x];
}

static void frame_rec(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct frame_rec_t *f = (struct frame_rec_t *)ctx;
    if (f->n < 8) { f->r[f->n][0] = x0; f->r[f->n][1] = y0; f->r[f->n][2] = x1; f->r[f->n][3] = y1; f->r[f->n][4] = pen; f->n++; }
}

static void test_which_from_gtype(void)
{
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x40), FLAT_W_DEPTH);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x60 | 1), FLAT_W_ZOOM);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x10), FLAT_W_SIZE);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x80 | 1), FLAT_W_CLOSE);
    CHECK_INT(flat_which_from_gtype(0x8000u | 0x20), -1);      /* GTYP_WDRAGGING is not a glyph */
    CHECK_INT(flat_which_from_gtype(0x0001u), -1);
    /* FLAT-C7: the screen title bar's depth gadget is type 0xc055 (SYSGADGET|SCRGADGET|GTYP_SDEPTH|BOOLGADGET) */
    CHECK_INT(flat_which_from_gtype(0xc055u), FLAT_W_DEPTH);
    {
        int img, other;
        CHECK_INT(flat_gadget_which(0xc055u, &img, 0, &img), FLAT_W_DEPTH);
        CHECK_INT(flat_gadget_which(0xc055u, &other, &img, &img), FLAT_W_DEPTH);
        CHECK_INT(flat_gadget_which(0xc055u, &other, 0, &img), -1);         /* another gadget's image */
        CHECK_INT(flat_gadget_which(0x4055u, &img, 0, &img), -1);           /* not a system gadget */
        CHECK_INT(flat_gadget_which(0xc035u, &img, 0, &img), -1);           /* screen drag bar: no glyph */
    }
    /* FLAT-C8: the frameiclass decision. Skip only for IM_DRAW / IM_DRAWFRAME, switch on, not held, flat pens. */
    {
        flat_state fs;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, FLAT_IM_DRAWFRAME, 2, 2), FLAT_DRAW_SKIP);
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 2, 0), FLAT_DRAW_STOCK);        /* stock pens keep stock bevels */
        CHECK_INT(flat_frame_decide(&fs, 0x203UL, 0, 0), FLAT_DRAW_STOCK);        /* IM_ERASE */
        CHECK_INT(flat_frame_decide(&fs, 0x204UL, 0, 0), FLAT_DRAW_STOCK);        /* IM_HITTEST: never skipped */
        CHECK_INT(flat_frame_decide(&fs, 0x0UL, 0, 0), FLAT_DRAW_STOCK);
        fs.held = 1;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0), FLAT_DRAW_STOCK);
        fs.held = 0;
        fs.enabled = 0;
        CHECK_INT(flat_frame_decide(&fs, 0x202UL, 0, 0), FLAT_DRAW_STOCK);
        CHECK_INT(flat_frame_decide(0, 0x202UL, 0, 0), FLAT_DRAW_STOCK);
    }
    {
        char fl[64];
        CHECK_INT(flat_frame_status_line(0, 0, 0, fl, sizeof fl) > 0 && !strcmp(fl, "frame=absent"), 1);
        CHECK_INT(flat_frame_status_line(1, 7, 3, fl, sizeof fl) > 0 && !strcmp(fl, "frame=hooked calls=7 flat=3"), 1);
        CHECK_INT(flat_frame_status_line(1, 7, 3, fl, 40), 0);
    }
    {   /* FLAT-C8: the flat frame geometry: fill, then four 1 px sides; edges-only has no fill; selected fills pen_line */
        struct frame_rec_t f;
        flat_state fs;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n, 1);                                                     /* FLAT-BUTTON-FILL: one fill, NO outline */
        CHECK_INT(f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 25 && f.r[0][4] == 4, 1);   /* pen_button */
        fs.pen_button = 0;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 0, 1);                              /* pen_button 0: pen_back */
        fs.pen_button = 4;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 0, 0, frame_rec, &f), 1);
        CHECK_INT(f.n, 4);                                                     /* not fill: the outline only, unchanged */
        CHECK_INT(f.r[0][4] == 1 && f.r[1][4] == 1 && f.r[2][4] == 1 && f.r[3][4] == 1, 1);
        CHECK_INT(f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 20, 1);
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 1, 10, 20, 8, 6, 1, 0, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 1, 1);                              /* pressed: accent (pen_line) fill, no outline */
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 2, 6, 1, 0, frame_rec, &f), 0);   /* too small: stock */
        CHECK_INT(flat_frame_draw(0, 0, 10, 20, 8, 6, 1, 0, frame_rec, &f), 0);
    }
    {   /* FLAT-BUTTON-ROUND: corners take the pen read just outside the button, not a fixed pen */
        struct round_canvas { int pix[40][60]; } *cv = (struct round_canvas *)calloc(1, sizeof *cv);
        flat_state fs;
        int cut[FLAT_ROUND_MAX][4], x, y, n, bg7 = 0, ok = 1;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0; fs.pen_button = 4;
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = 7;       /* group box pen 7, not pen 0 */
        CHECK_INT(flat_frame_draw_round(&fs, 0, 10, 10, 20, 8, 1, 0, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[10][10] == 7 && cv->pix[10][29] == 7 && cv->pix[17][10] == 7 && cv->pix[17][29] == 7, 1);   /* corners: outside pen */
        CHECK_INT(cv->pix[10][11] == 4 && cv->pix[11][10] == 4 && cv->pix[12][12] == 4, 1);                          /* small: 1 px cut only */
        for (y = 0; y < 40; y++) for (x = 0; x < 60; x++) cv->pix[y][x] = (x + y) & 3;   /* patterned fill: each corner its own neighbour */
        CHECK_INT(flat_frame_draw_round(&fs, 1, 10, 10, 20, 8, 1, 0, round_fill, round_read, cv), 1);
        CHECK_INT(cv->pix[10][10] == (9 + 9 & 3) && cv->pix[17][29] == (30 + 18 & 3) && cv->pix[10][29] == (30 + 9 & 3), 1);   /* pressed too */
        CHECK_INT(cv->pix[12][12] == 1, 1);                                                                          /* pressed body: pen_line */
        CHECK_INT(flat_button_corners(20, 8, cut), 4);                                                               /* small: 4 cuts */
        CHECK_INT(cut[0][0] == 0 && cut[0][1] == 0 && cut[0][2] == -1 && cut[0][3] == -1, 1);
        CHECK_INT(cut[3][0] == 19 && cut[3][1] == 7 && cut[3][2] == 20 && cut[3][3] == 8, 1);
        CHECK_INT(flat_button_corners(20, 11, cut), 4);
        n = flat_button_corners(20, 12, cut);
        CHECK_INT(n, 12);                                                                                            /* large: corner + two beside each */
        for (x = 0; x < n; x++) {                                                                                   /* inside pixels distinct, all on the box edge ring */
            int ex = cut[x][0] == 0 || cut[x][0] == 19, ey = cut[x][1] == 0 || cut[x][1] == 11, k;
            if (!ex && !ey) ok = 0;
            for (k = 0; k < x; k++) if (cut[k][0] == cut[x][0] && cut[k][1] == cut[x][1]) ok = 0;
        }
        CHECK_INT(ok, 1);
        CHECK_INT(cut[1][0] == 1 && cut[1][1] == 0 && cut[1][2] == 1 && cut[1][3] == -1, 1);                         /* beside the corner, along the top */
        CHECK_INT(cut[2][0] == 0 && cut[2][1] == 1 && cut[2][2] == -1 && cut[2][3] == 1, 1);                         /* beside the corner, down the left */
        CHECK_INT(flat_button_corners(2, 12, cut), 0);
        (void)bg7;
        free(cv);
    }
    {   /* BUTTON-ROLES: rightmost of a row is outlined, others and a lone button are filled; fill pen per depth; pressed is accent */
        struct frame_rec_t f;
        flat_state fs;
        flat_box row[3] = { {10, 5, 40, 12}, {60, 5, 40, 12}, {110, 5, 40, 12} }, other = {10, 40, 40, 12}, lone = {10, 5, 40, 12};
        int i;
        memset(&fs, 0, sizeof fs);
        fs.enabled = 1; fs.pen_text = 2; fs.pen_line = 3; fs.pen_back = 0; fs.pen_button = 4;
        CHECK_INT(flat_button_role(&row[2], row, 3), FLAT_ROLE_SECONDARY);   /* rightmost button in a row is outlined */
        CHECK_INT(flat_button_role(&row[0], row, 3), FLAT_ROLE_PRIMARY);     /* left button is filled */
        CHECK_INT(flat_button_role(&row[1], row, 3), FLAT_ROLE_PRIMARY);
        CHECK_INT(flat_button_role(&lone, &lone, 1), FLAT_ROLE_PRIMARY);     /* lone button is filled */
        { flat_box two[2]; two[0] = row[2]; two[1] = other;
          CHECK_INT(flat_button_role(&row[2], two, 2), FLAT_ROLE_PRIMARY); } /* a button in another row is no sibling */
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n, 5);                                                   /* background clear + four sides */
        for (i = 1; i < 5; i++)
            CHECK_INT(f.r[i][4], 2);                                         /* secondary: text-pen outline */
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 1, 10, 20, 8, 6, 1, FLAT_ROLE_SECONDARY, frame_rec, &f), 1);
        CHECK_INT(f.n == 1 && f.r[0][4] == 3, 1);                            /* pressed secondary gets accent fill */
        CHECK_INT(flat_button_pen(&fs, 2), 3);                               /* 4-colour primary uses accent fill */
        CHECK_INT(flat_button_pen(&fs, 3), 4);                               /* 8 colours: lighter background */
    }
    {   /* FLAT-BUTTON-FILL: the pressed-label arm and the depth-checked fill pen */
        flat_tx t; flat_state bs; int a, b, c1, c2;
        memset(&t, 0, sizeof t);
        a = 1; b = 2; c1 = 3; c2 = 4;
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 0);                      /* not armed */
        flat_tx_frame(&t, &a, &c1, 0, 1, 10, 20, 17, 25);                     /* normal frame: no arm */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 0, 10, 20, 17, 25);                     /* pressed but edges-only: no arm */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 1);                      /* match */
        CHECK_INT(flat_tx_text(&t, &a, &c1, 10, 25) && flat_tx_text(&t, &a, &c1, 17, 20), 1);   /* box corners are inside */
        CHECK_INT(flat_tx_text(&t, &b, &c1, 12, 22), 0);                      /* other rastport: untouched */
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c2, 12, 22), 0);                      /* other task: untouched */
        CHECK_INT(t.armed, 1);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 9, 22), 0);                       /* outside the box: disarms */
        CHECK_INT(t.armed, 0);
        CHECK_INT(flat_tx_text(&t, &a, &c1, 12, 22), 0);
        memset(&bs, 0, sizeof bs);                                           /* pen_ptext = DK_BG = pen 0: a real pen */
        bs.enabled = 1; bs.pen_ptext = DK_BG;
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), 0);                  /* pressed label: pen 0, not "unset" */
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c2, 12, 22), -1);                 /* other task: stock */
        bs.held = 1;
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), -1);                 /* held: stock */
        bs.held = 0; bs.enabled = 0;
        CHECK_INT(flat_tx_pen(&bs, &t, &a, &c1, 12, 22), -1);                 /* off: stock */
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        flat_tx_frame(&t, &a, &c1, 0, 1, 30, 20, 37, 25);                     /* another frame draw on the rastport: disarms */
        CHECK_INT(t.armed, 0);
        flat_tx_frame(&t, &a, &c1, 1, 1, 10, 20, 17, 25);
        flat_tx_frame(&t, &b, &c1, 0, 1, 30, 20, 37, 25);                     /* a draw on another rastport keeps it */
        CHECK_INT(t.armed, 1);
        {   /* pressed label drawn through a second RastPort of the same window layer (3.2 intuition) still matches */
            int layer, bm1, bm2;
            memset(&t, 0, sizeof t);
            flat_tx_frame(&t, flat_tx_surface(&layer, &bm1), &c1, 1, 1, 10, 44, 109, 59);
            bs.enabled = 1;
            CHECK_INT(flat_tx_pen(&bs, &t, flat_tx_surface(&layer, &bm2), &c1, 40, 53), 0);
            CHECK_INT(flat_tx_surface(0, &bm1) == (const void *)&bm1, 1);       /* no layer: the bitmap */
            CHECK_INT(flat_tx_surface(0, &bm1) != flat_tx_surface(0, &bm2), 1);
        }
        {   int w, h, img = 0, other = 0;
            flat_frame_size(0, 4096, -17232, 30, 12, &w, &h);                 /* IM_DRAW: the Dimensions words are junk */
            CHECK_INT(w == 30 && h == 12, 1);
            flat_frame_size(1, 50, 20, 30, 12, &w, &h);                       /* IM_DRAWFRAME: Dimensions win */
            CHECK_INT(w == 50 && h == 20, 1);
            flat_frame_size(1, 0, 0, 30, 12, &w, &h);
            CHECK_INT(w == 30 && h == 12, 1);
            CHECK_INT(flat_button_gadget(&img, &img, &img), 1);               /* button / cycle: render == select == frame */
            CHECK_INT(flat_button_gadget(&img, 0, &img), 0);                  /* slider / string: no select image */
            CHECK_INT(flat_button_gadget(&img, &other, &img), 0);
            CHECK_INT(flat_button_gadget(&other, &other, &img), 0);           /* another gadget's image */
            CHECK_INT(flat_button_gadget(0, 0, 0), 0);
        }
        memset(&bs, 0, sizeof bs);
        bs.pen_back = 0; bs.pen_button = 4;
        CHECK_INT(flat_button_pen(&bs, 3), 4);                                /* 8 colours: the button pen */
        CHECK_INT(flat_button_pen(&bs, 2), 0);                                /* 4 colours, no accent set: pen_back */
        bs.pen_button = 0;
        CHECK_INT(flat_button_pen(&bs, 3), 0);                                /* unset: pen_back */
    }
    {   /* FLAT-OUTER-EDGE: flat_outline_rects paints exactly the outer 1 px ring of a w x h window, each pixel once, nothing inside */
        static const int sz[][2] = { { 2, 2 }, { 3, 3 }, { 7, 5 }, { 64, 40 }, { 640, 256 } };
        int i, x, y;
        flat_state os;
        memset(&os, 0, sizeof os);
        os.pen_outline = 5;
        for (i = 0; i < 5; i++) {
            int w = sz[i][0], h = sz[i][1], bad = 0;
            static unsigned char cv[640 * 256];
            struct outline_cv_t c;
            memset(cv, 0, sizeof cv); c.cv = cv; c.w = w; c.h = h; c.bad = 0;
            CHECK_INT(flat_outline_rects(&os, w, h, outline_fill, &c), 4);
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    int edge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
                    if (cv[y * w + x] != (edge ? 1 : 0))
                        bad++;
                }
            CHECK_INT(bad + c.bad, 0);                                       /* ring only, each pixel once, in the pen, in bounds */
        }
        os.pen_outline = 0;
        {   unsigned char one[16]; struct outline_cv_t c;
            memset(one, 0, sizeof one); c.cv = one; c.w = 4; c.h = 4; c.bad = 0;
            CHECK_INT(flat_outline_rects(&os, 4, 4, outline_fill, &c), 0);   /* unset pen: no rects */
            CHECK_INT(one[0] | one[5] | one[15], 0);
            os.pen_outline = 5;
            CHECK_INT(flat_outline_rects(&os, 1, 9, outline_fill, &c), 0);   /* under 2 x 2: none */
            CHECK_INT(flat_outline_rects(0, 4, 4, outline_fill, &c), 0);
        }
    }
    {   /* FLAT-OUTER-EDGE: the RectFill patch's selector accepts exactly Intuition's four outer strips, nothing else */
        static const int sz[][2] = { { 220, 90 }, { 400, 100 }, { 640, 256 }, { 3, 3 } };
        int i;
        for (i = 0; i < 4; i++) {
            int w = sz[i][0], h = sz[i][1];
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 1, 0), 1);            /* top */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 1, 0, h - 1), 1);            /* left */
            CHECK_INT(flat_outer_edge_match(w, h, w - 1, 1, w - 1, h - 2), 1);    /* right */
            CHECK_INT(flat_outer_edge_match(w, h, 1, h - 1, w - 1, h - 1), 1);    /* bottom */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 2, 0), 0);            /* off by one */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w, 0), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 0, 1, 0, h - 2), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 0, h - 1, w - 1, h - 1), 0);    /* bottom starting at the corner */
            CHECK_INT(flat_outer_edge_match(w, h, 0, 0, w - 1, 1), 0);            /* 2 px thick */
            CHECK_INT(flat_outer_edge_match(w, h, 1, 1, w - 2, 1), 0);            /* inner bevel rects */
            CHECK_INT(flat_outer_edge_match(w, h, 1, 2, 1, h - 2), 0);
            CHECK_INT(flat_outer_edge_match(w, h, 3, 13, w - 18, 13), 0);
        }
        CHECK_INT(flat_outer_edge_match(1, 9, 0, 0, 0, 0), 0);                   /* under 2 x 2 */
    }
    {   /* FLAT-ASPECT-BORDERS: on PAL hires (22 x 44) the window's side lines are 2 px wide, so they look as thick as the top line */
        int r[4];
        CHECK_INT(flat_side_px(22, 44), 2);      /* PAL/NTSC hires non-interlace */
        CHECK_INT(flat_side_px(11, 44), 4);      /* super hires non-interlace */
        CHECK_INT(flat_side_px(22, 22), 1);      /* hires interlace: square */
        CHECK_INT(flat_side_px(44, 44), 1);      /* lores: square */
        CHECK_INT(flat_side_px(44, 22), 1);      /* lores interlace: wide pixels, sides stay 1 */
        CHECK_INT(flat_side_px(0, 0), 1);        /* unknown mode / RTG without DisplayInfo */
        CHECK_INT(flat_side_px(30, 44), 1);      /* 1.47 rounds to 1 */
        CHECK_INT(flat_outer_edge_fill(640, 256, 0, 1, 0, 255, flat_side_px(22, 44), r), 1);       /* left: x 0..1 */
        CHECK_INT(r[0], 0); CHECK_INT(r[1], 1); CHECK_INT(r[2], 1); CHECK_INT(r[3], 255);
        CHECK_INT(flat_outer_edge_fill(640, 256, 639, 1, 639, 254, 2, r), 1);   /* right: x 638..639 */
        CHECK_INT(r[0], 638); CHECK_INT(r[1], 1); CHECK_INT(r[2], 639); CHECK_INT(r[3], 254);
        CHECK_INT(flat_outer_edge_fill(640, 256, 0, 0, 639, 0, 2, r), 1);       /* top stays 1 px */
        CHECK_INT(r[0], 0); CHECK_INT(r[1], 0); CHECK_INT(r[2], 639); CHECK_INT(r[3], 0);
        CHECK_INT(flat_outer_edge_fill(640, 256, 1, 255, 639, 255, 2, r), 1);   /* bottom stays 1 px */
        CHECK_INT(r[1], 255); CHECK_INT(r[3], 255);
        CHECK_INT(flat_outer_edge_fill(640, 512, 0, 1, 0, 511, flat_side_px(22, 22), r), 1);       /* interlace: left stays 1 px */
        CHECK_INT(r[0], 0); CHECK_INT(r[2], 0);
        CHECK_INT(flat_outer_edge_fill(3, 3, 0, 1, 0, 2, 4, r), 1);             /* px clamps to w / 2 */
        CHECK_INT(r[2], 0);
        CHECK_INT(flat_outer_edge_fill(640, 256, 1, 1, 1, 254, 2, r), 0);       /* not an outer strip */
    }
    CHECK_INT((int)flat_which_alias(5), FLAT_W_DEPTH);
    CHECK_INT((int)flat_which_alias(3), FLAT_W_CLOSE);
}

/* FLAT-C4: the kill switches. A table of fake hooks / task names / buttons / marker ages drives the same
 * functions flat.c runs at start. */
static void test_kill_switches(void)
{
    static const struct { const char *name; int want; } tasks[] = {
        { "VisualPrefs", FLAT_HELD_VISUALPREFS }, { "visualprefs handler", FLAT_HELD_VISUALPREFS },
        { "Birdie", FLAT_HELD_BIRDIE }, { "BirdieD", FLAT_HELD_BIRDIE }, { "BIRDIE", FLAT_HELD_BIRDIE },
        { "Workbench", FLAT_HELD_NONE }, { "Visual", FLAT_HELD_NONE }, { "", FLAT_HELD_NONE },
        { "Amarchy", FLAT_HELD_NONE }, { "bird", FLAT_HELD_NONE },
    };
    size_t i;
    flat_state s;
    char line[96], txt[64];
    unsigned char vec[6] = { 0x4E, 0xF9, 0x00, 0xF8, 0x12, 0x34 };

    for (i = 0; i < sizeof tasks / sizeof tasks[0]; i++)
        CHECK_INT(flat_other_task(tasks[i].name), tasks[i].want);
    CHECK_INT(flat_other_task(0), FLAT_HELD_NONE);

    /* the hook on the sysiclass slot: inside the ROM span is stock, outside is a foreign patcher */
    CHECK_INT(flat_other_hook(0x00F80000ul, 0x00F80000ul, 0x00FC0000ul), FLAT_HELD_NONE);       /* first byte */
    CHECK_INT(flat_other_hook(0x00FBFFFFul, 0x00F80000ul, 0x00FC0000ul), FLAT_HELD_NONE);       /* last byte */
    CHECK_INT(flat_other_hook(0x00FC0000ul, 0x00F80000ul, 0x00FC0000ul), FLAT_HELD_HOOK);       /* end is outside */
    CHECK_INT(flat_other_hook(0x00F7FFFFul, 0x00F80000ul, 0x00FC0000ul), FLAT_HELD_HOOK);
    CHECK_INT(flat_other_hook(0x07A00000ul, 0x00F80000ul, 0x00FC0000ul), FLAT_HELD_HOOK);       /* RAM: VisualPrefs/Birdie */

    /* the shared ROM-span helper that up-console.device also uses */
    CHECK_INT(rom_vec_patched(vec, 0x00F80000ul, 0x00FC0000ul), 0);
    vec[2] = 0x07;
    CHECK_INT(rom_vec_patched(vec, 0x00F80000ul, 0x00FC0000ul), 1);                          /* target in RAM */
    vec[2] = 0x00;
    vec[1] = 0xF8;
    CHECK_INT(rom_vec_patched(vec, 0x00F80000ul, 0x00FC0000ul), 1);                          /* not a jmp abs.l */

    /* the start decision: marker beats gesture beats other patcher */
    CHECK_INT(flat_start_decide(0, 0, 0, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(1, 0, 0, 0), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(1, 1, 1, FLAT_HELD_BIRDIE), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(0, 1, 1, 0), FLAT_HELD_GESTURE);
    CHECK_INT(flat_start_decide(0, 1, 0, 0), FLAT_HELD_NONE);                                /* one button is not the gesture */
    CHECK_INT(flat_start_decide(0, 0, 1, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(0, 1, 1, FLAT_HELD_HOOK), FLAT_HELD_GESTURE);
    CHECK_INT(flat_start_decide(0, 0, 0, FLAT_HELD_VISUALPREFS), FLAT_HELD_VISUALPREFS);

    /* marker -> inert with an injected clock: written at t, alive for FLAT_MARKER_SECS, then gone */
    CHECK_INT(flat_marker_remaining(1000, 1000), FLAT_MARKER_SECS);
    CHECK_INT(flat_marker_remaining(1000, 1059), 1);
    CHECK_INT(flat_marker_remaining(1000, 1060), 0);
    CHECK_INT(flat_marker_remaining(1000, 5000), 0);
    CHECK_INT(flat_marker_remaining(0xFFFFFFF0ul, 5), 60 - 21);                              /* the clock wrapped */

    /* a held block is inert: kill switch beats the switch, and STATUS names the patch */
    flat_init(&s);
    s.enabled = 1;
    s.held = (uint16_t)flat_start_decide(1, 0, 0, 0);
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=held calls=0 chained=0 why=marker");
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 1, 1), FLAT_DRAW_STOCK);
    s.held = FLAT_HELD_VISUALPREFS;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "held by VisualPrefs");
    s.held = FLAT_HELD_BIRDIE;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "held by Birdie");
    s.held = FLAT_HELD_MARKER;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "held by crash marker");
    s.held = FLAT_HELD_GESTURE;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "held by both mouse buttons");
    s.held = FLAT_HELD_HOOK;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "held by another hook on the gadget class");
    s.held = 0;
    flat_status_text(&s, 1, txt, sizeof txt);
    CHECK_STR(txt, "on");
    flat_status_text(0, 0, txt, sizeof txt);
    CHECK_STR(txt, "absent");
    CHECK_INT(flat_status_text(&s, 1, txt, 39), 0);
    CHECK_STR(FLAT_MARKER_PATH, "ENVARC:Amarchy/flat-starting");
}

/* FLAT-C9: the prefs -> flat_state mapping both writers (desk apply, amarchy-flat BOOT) call. The sentinel proves the function ran;
 * the counter rows (version, size, held, still_chained, calls) belong to the resident program and survive the write. */
static void test_desk_flat_state(void)
{
    flat_state s;
    unsigned char t, l, b;
    unsigned long before;
    int f, g;
    desk_flat_glyph_pens(&t, &l, &b);
    for (f = 0; f < 2; f++)
        for (g = 0; g < 2; g++) {
            flat_init(&s);
            s.held = 2; s.still_chained = 1; s.calls = 77;
            before = desk_flat_state_calls;
            desk_flat_state(f, g, &s);
            CHECK_INT(desk_flat_state_calls, before + 1);                      /* sentinel: the mapping ran */
            CHECK_INT(s.enabled, f || g);                                     /* either switch enables; both off = 0 */
            CHECK_INT(s.pen_text, t);
            CHECK_INT(s.pen_line, l);
            CHECK_INT(s.pen_back, b);
            CHECK_INT(s.pen_button, DK_SELECTION);                            /* FLAT-BUTTON-FILL */
            CHECK_INT(s.pen_ptext, DK_BG);
            CHECK_INT(s.pen_outline, DK_ACCENT);                              /* the theme accent pen, never 0 */
            CHECK(s.pen_outline != 0);
            CHECK_INT(s.version, FLAT_VERSION);
            CHECK_INT(s.size, FLAT_STATE_SIZE);
            CHECK_INT(s.held, 2);
            CHECK_INT(s.still_chained, 1);
            CHECK_INT(s.calls, 77);
            CHECK_INT(flat_check(&s, sizeof s), FLAT_OK);
        }
}

/* FLAT-SCROLLER: knob = the lighter pen (pen_button), track = the frame pen (pen_back), nothing outside the container, nothing
 * when flat is off or held. Same rule for every state: the function takes no active flag. */
static void test_scroller(void)
{
    flat_state s;
    int kn, tr;
    flat_init(&s);
    desk_flat_state(1, 1, &s);
    kn = DK_MUTED;                                                       /* KNOB-MUTED: the knob is the theme muted text pen, not the selection pen nor the lighter pen_outline */
    tr = s.pen_back;
    CHECK(kn != tr);
    CHECK(kn != DK_SELECTION);
    CHECK_INT(s.pen_dimtext, kn);
    CHECK(kn != s.pen_outline);
    /* vertical 16 x 100 container at (10,20), body half: knob height 50 */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 70, 25, 119), tr);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0xFFFF, 0, 0x8000, 10, 70, 25, 119), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0xFFFF, 0, 0x8000, 10, 20, 25, 69), tr);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 60, 25, 80), tr);   /* straddles the knob edge */
    /* FLAT-SLIDER-ROUND: a fill that is exactly the knob gets the button corner cuts; any other fill gets none */
    {
        int cuts[FLAT_ROUND_MAX][2], n, i, hit = 0;
        n = flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69, cuts);   /* knob 16 x 50 at (10,20): tall, 12 cuts */
        CHECK_INT(n, 12);
        for (i = 0; i < n; i++)
            if (cuts[i][0] == 10 && cuts[i][1] == 20) hit = 1;
        CHECK_INT(hit, 1);                                                                       /* top left knob pixel is cut */
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 11, 22, 24, 67, cuts), 0);
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 70, 25, 119, cuts), 0);   /* the track */
        CHECK_INT(flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 12, 22, 20, 30, cuts), 0);
    }
    /* RL6: 3.2 paints the knob as several fills (frame strips + interior), never one fill equal to the knob: every strip cuts the
     * corner pixels it contains, the interior cuts none, and across the strips every corner pixel is cut */
    {
        int cuts[FLAT_ROUND_MAX][2], all[FLAT_ROUND_MAX][2], n, i, j, total = 0, seen_tl = 0, seen_br = 0;
        static const int strip[5][4] = { {10, 20, 25, 20}, {10, 69, 25, 69}, {10, 21, 10, 68}, {25, 21, 25, 68}, {11, 21, 24, 68} };
        for (j = 0; j < 5; j++) {
            n = flat_scroller_cuts(10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, strip[j][0], strip[j][1], strip[j][2], strip[j][3], cuts);
            if (j == 4) CHECK_INT(n, 0);                                                         /* the interior has no corner */
            for (i = 0; i < n; i++) {
                CHECK_INT(cuts[i][0] >= strip[j][0] && cuts[i][0] <= strip[j][2] && cuts[i][1] >= strip[j][1] && cuts[i][1] <= strip[j][3], 1);
                all[total][0] = cuts[i][0]; all[total][1] = cuts[i][1]; total++;
                if (cuts[i][0] == 10 && cuts[i][1] == 20) seen_tl = 1;
                if (cuts[i][0] == 25 && cuts[i][1] == 69) seen_br = 1;
            }
        }
        CHECK_INT(seen_tl, 1);
        CHECK_INT(seen_br, 1);
        CHECK_INT(total >= 12, 1);                                                               /* every cut of the whole knob is covered */
    }
    /* a tiny body still gives a 1 px knob */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 1, 10, 20, 25, 20), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 1, 10, 21, 25, 21), tr);
    /* horizontal 100 x 10, body a quarter (knob width 25), pot at the end: knob x 75..99 */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 0, 0, 100, 10, 1, 0, 0xFFFF, 0, 0x4000, 0, 75, 0, 99, 9), kn);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 0, 0, 100, 10, 1, 0, 0xFFFF, 0, 0x4000, 0, 0, 0, 74, 9), tr);
    /* outside the container, flat held, flat off: -1, the call stays alone */
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 9, 20, 25, 69), -1);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 26, 69), -1);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 120), -1);
    s.held = 1;
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), -1);
    s.held = 0;
    /* under 8 colours the knob takes the accent pen_line (flat_button_pen), never a missing pen */
    CHECK_INT(flat_scroller_pen(&s, 2, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), s.pen_line);
    /* fallback: pen_dimtext 0 or not a pen of the depth keeps flat_button_pen */
    {
        int keep = s.pen_dimtext;
        s.pen_dimtext = 0;
        CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), flat_button_pen(&s, 3));
        s.pen_dimtext = 9;
        CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), flat_button_pen(&s, 3));
        s.pen_dimtext = keep;
    }
    s.enabled = 0;
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), -1);
}

/* KNOB-PEN: the knob has its own pen from 16 colours, a colour between the surface and the muted text on every theme; at 8 colours
 * or fewer (no free pen) it is DK_MUTED as before. */
static void test_knob_pen(void)
{
    flat_state s;
    flat_init(&s);
    desk_flat_state(1, 1, &s);
    s.pen_knob = 9;
    CHECK_INT(flat_knob_pen(&s, 4), 9);
    CHECK_INT(flat_scroller_pen(&s, 4, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);   /* reachable through the scroller entry */
    CHECK_INT(flat_scroller_pen(&s, 8, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);
    CHECK_INT(flat_scroller_pen(&s, 3, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), DK_MUTED);   /* 3 planes: no free pen */
    CHECK_INT(flat_knob_pen(&s, 3), DK_MUTED);
    s.pen_knob = 0;
    CHECK_INT(flat_knob_pen(&s, 5), DK_MUTED);
    s.pen_knob = 20;                                                     /* not a pen of 4 planes */
    CHECK_INT(flat_knob_pen(&s, 4), DK_MUTED);
    CHECK_INT(flat_knob_colour(0x000000UL, 0x7A7A7AUL), 0x494949UL);      /* vantablack */
    /* a light theme: white surface, 666666 muted text: strictly between both */
    CHECK(flat_knob_colour(0xFFFFFFUL, 0x666666UL) > 0x666666UL && flat_knob_colour(0xFFFFFFUL, 0x666666UL) < 0xFFFFFFUL);
    CHECK_INT(flat_knob_colour(0xFFFFFFUL, 0x666666UL) & 0xFF, (255 * 40 / 100) + (0x66 * 60 / 100));
}

/* disabled flat button gets solid dim fill, no dither: a pattern fill over a recorded filled button box takes the dim pen
 * (and the label is handed back for a dim redraw); the same fill over a non-flat area, a pattern-less fill, an unfilled
 * or pressed or secondary frame, a held or off block is not matched (stock). */
static void test_disabled_button_dim_fill(void)
{
    flat_state s;
    flat_gh g;
    int a, b, c1, c2;
    const flat_gbox *m;
    memset(&s, 0, sizeof s); memset(&g, 0, sizeof g);
    s.enabled = 1; s.pen_back = 0; s.pen_text = 2; s.pen_line = 3; s.pen_button = 4; s.pen_dim = 6; s.pen_dimtext = 5;
    flat_gh_frame(&g, &a, &c1, 0, 1, 0, 10, 20, 49, 39);
    flat_gh_text(&g, &a, &c1, 14, 32, "Use", 3);
    m = flat_gh_match(&s, &g, &a, &c1, 2, 10, 20, 49, 39);
    CHECK_INT(m != 0, 1);
    CHECK_INT(flat_dim_pen(&s, 8), 6);
    CHECK_INT(flat_dim_pen(&s, 8) != s.pen_button, 1);
    CHECK_INT(flat_dimtext_pen(&s, 8), 5);
    CHECK_INT(m && m->str && m->count == 3 && m->lx == 14 && m->ly == 32, 1);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 12, 22, 20, 30) != 0, 1);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 0, 10, 20, 49, 39) == 0, 1);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 60, 20, 80, 39) == 0, 1);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 5, 20, 49, 39) == 0, 1);
    CHECK_INT(flat_gh_match(&s, &g, &b, &c1, 2, 10, 20, 49, 39) == 0, 1);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c2, 2, 10, 20, 49, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 1, 1, 0, 10, 20, 49, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 10, 20, 49, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 0, 1, 1, 60, 20, 99, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 60, 20, 99, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 0, 0, 0, 60, 20, 99, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 60, 20, 99, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 0, 1, 0, 10, 20, 49, 39);
    s.held = 1;
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 10, 20, 49, 39) == 0, 1);
    s.held = 0; s.enabled = 0;
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 10, 20, 49, 39) == 0, 1);
    s.enabled = 1;
    CHECK_INT(flat_dim_pen(&s, 2), 3);
    s.pen_dim = 9;
    CHECK_INT(flat_dim_pen(&s, 3), 4);
    CHECK_INT(flat_dimtext_pen(&s, 2), 2);
}

static void test_title_accent(void)
{
    flat_state s;
    flat_title t;
    memset(&s, 0, sizeof s);
    s.enabled = 1; s.pen_outline = 3; s.pen_ptext = 0; s.pen_dimtext = 5; s.pen_text = 2;
    /* active: accent band over the title bar only (rows 0..bar_h-1), title text dark on it */
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 1);
    CHECK_INT(t.band_pen, 3);
    CHECK_INT(t.bx0 == 0 && t.by0 == 0 && t.bx1 == 199 && t.by1 == 10, 1);
    /* FLAT-GADGETS-RIG8: the band misses every system gadget (close left, depth + zoom relright, drag bar ignored) */
    {
        flat_gadrect g[4] = { { 0, 0, 18, 11, 0, 0, 0 }, { -18, 0, 18, 11, 1, 0, 0 }, { -36, 0, 18, 11, 1, 0, 0 },
                              { 0, 0, -54, 11, 0, 1, 1 } };
        int x0, x1, i, gl, gr;
        flat_title_span(g, 4, 200, 11, &x0, &x1);
        CHECK_INT(x0, 18);
        CHECK_INT(x1, 200 - 36 - 1);
        CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Shell", 5, 9, x0, x1, &t), 1);
        for (i = 0; i < 3; i++) {
            gl = g[i].x + (g[i].relright ? 200 : 0);
            gr = gl + g[i].w - 1;
            CHECK_INT(t.bx1 < gl || t.bx0 > gr, 1);
        }
        flat_title_span(g + 1, 2, 200, 11, &x0, &x1);       /* no close gadget */
        CHECK_INT(x0, 0);
        CHECK_INT(x1, 200 - 36 - 1);
        flat_title_span(g, 0, 200, 11, &x0, &x1);
        CHECK_INT(x0 == 0 && x1 == 199, 1);
    }
    CHECK_INT(t.text_pen, 0);
    CHECK_INT(t.bg_pen, 3);          /* JAM2 Text box must repaint the band pen, not pen 0 */
    /* inactive under 16 colours (no pen_inbar): the band takes the muted pen, the text pen_ptext (S32-INACTIVE-BAR) */
    CHECK_INT(flat_title_decide(&s, 3, 0, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 1);
    CHECK_INT(t.band_pen, 5);
    CHECK_INT(t.text_pen, 0);
    CHECK_INT(t.bg_pen, 5);
    /* inactive with the inactive pen on a 16 colour screen: band = pen_inbar, text = pen_ptext */
    s.pen_inbar = 9; s.pen_ptext = 1;
    CHECK_INT(flat_title_decide(&s, 4, 0, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 1);
    CHECK_INT(t.band_pen, 9);
    CHECK_INT(t.bg_pen, 9);
    CHECK_INT(t.text_pen, 1);
    /* neither pen: no band, the dim text as before */
    s.pen_inbar = 0; s.pen_dimtext = 0;
    CHECK_INT(flat_title_decide(&s, 4, 0, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 1);
    CHECK_INT(t.band_pen, -1);
    CHECK_INT(t.bg_pen, -1);
    s.pen_dimtext = 5; s.pen_ptext = 0;
    /* a clipped title (prefix) still matches; body text, wrong text, a row below the bar, off/held do not */
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "She", 3, 9, 0, 199, &t), 1);
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Hello", 5, 9, 0, 199, &t), 0);
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Shell", 5, 30, 0, 199, &t), 0);
    CHECK_INT(flat_title_decide(&s, 3, 1, 0, 200, 11, "Shell", 5, 9, 0, 199, &t), 0);
    s.held = 1;
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 0);
    s.held = 0; s.enabled = 0;
    CHECK_INT(flat_title_decide(&s, 3, 1, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 0);
    s.enabled = 1;
    /* accent pen not a pen of a 2 colour screen: no band, stock text pen */
    CHECK_INT(flat_title_decide(&s, 1, 1, "Shell", 200, 11, "Shell", 5, 9, 0, 199, &t), 1);
    CHECK_INT(t.band_pen, -1);
    CHECK_INT(t.text_pen, -1);
}

/* ---- FLAT-GLYPH-OS-SHAPES: the clone. A fake 3D close gadget (9 x 9) as the stock image draws it with marker pens: bevel ring
 * (shine top / left, shadow bottom / right), ground, a 3 x 3 glyph in the text marker. The one reachability test: through
 * flat_clone_draw (what flat.c runs) with a render sentinel; the pens are checked on the canvas. */
static int g_render_calls, g_render_fail;
static int g_render_selected;
static int fake_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y;
    (void)ctx;
    g_render_calls++;
    if (g_render_fail)
        return 0;
    g_render_selected = ids == 1;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int ring = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            int m = FLAT_CLONE_MARK(g_render_selected ? 5 : 7);                 /* ground: FILLPEN selected, BACKGROUNDPEN normal */
            if (ring)
                m = (x == 0 || y == 0) != g_render_selected ? FLAT_CLONE_SHINE : FLAT_CLONE_SHADOW;
            else if (x >= 3 && x <= 5 && y >= 3 && y <= 5)
                m = FLAT_CLONE_MARK(2);                                         /* the glyph */
            grid[y * w + x] = (uint8_t)m;
        }
    return 1;
}

static int blank_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    (void)ctx; (void)ids; (void)grid; (void)w; (void)h;
    g_render_calls++;
    return 1;                                                                   /* OS 3.2 window gadgets: rc 1, nothing drawn */
}

static uint8_t g_scr[FLAT_CLONE_SCRATCH];

/* S31-ARROW-ASPECT: on hires (flat_side_px 2) the owner saw narrow left/right and too-big up/down arrows. Every arrow must have
 * the same 45-degree shape in visual units (a row counts side columns): left/right base (rows * side) == 2 * length (columns),
 * up/down widest row == 2 * side * (rows - 1) + 1; the glyph is centred in its box. Box sizes are the rig's 3.2 hires ones. */
static void arrow_bbox(const flat_state *st, unsigned long w, int bw, int bh, int side, int *x0, int *x1, int *rows, int *top, int *bot)
{
    int x, y, y0 = -1, y1 = -1;
    cv_clear();
    *x0 = CV; *x1 = -1;
    CHECK_INT(flat_glyph_draw(st, w, FLAT_IM_DRAW, 0, 0, 0, bw, bh, st->pen_back, side, cv_fill, 0), 1);
    for (y = 0; y < bh; y++)
        for (x = 0; x < bw; x++)
            if (cv[y][x] == (char)st->pen_text) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y0 < 0) y0 = y;
                y1 = y;
            }
    *rows = y1 - y0 + 1;
    if (top) *top = y0;
    if (bot) *bot = y1;
}

static void test_arrow_aspect(void)
{
    flat_state st;
    unsigned long w;
    int side, x0, x1, rows, top, bot, lw[3] = { 0 }, uh[3] = { 0 }, gap[3] = { 0 };
    flat_init(&st);
    desk_flat_state(1, 1, &st);
    for (side = 1; side <= 2; side++) {
        for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++) {
            if (w == FLAT_W_LEFT || w == FLAT_W_RIGHT) {
                arrow_bbox(&st, w, 16, 10, side, &x0, &x1, &rows, 0, 0);
                CHECK_INT(rows * side, 2 * (x1 - x0 + 1));        /* base twice the length, in visual units */
                CHECK(abs(x0 - (16 - 1 - x1)) <= 1);              /* centred */
                lw[side] = x1 - x0 + 1;
            } else {
                arrow_bbox(&st, w, 18, 11, side, &x0, &x1, &rows, &top, &bot);
                CHECK_INT(x1 - x0 + 1, 2 * side * (rows - 1) + 1);
                CHECK(abs(x0 - (18 - 1 - x1)) <= 1);
                uh[side] = rows;
                if (w == FLAT_W_UP) gap[side] += 11 - 1 - bot;    /* up box sits above the down box */
                else gap[side] += top;
            }
        }
    }
    CHECK_INT(lw[2], 2 * lw[1]);                                  /* hires: left/right twice as wide */
    CHECK(uh[2] < uh[1]);                                         /* hires: up/down fewer rows */
    /* S31-ARROW-GAP: the shorter hires glyphs sit toward the shared edge, so the empty rows between the up and the down
     * glyph stay what they are on a square-pixel screen (owner: "the vertical spacing between the arrows are bigger"). */
    CHECK_INT(gap[2], gap[1]);
}

/* S31 SCROLLER-ARROWS-INVISIBLE: OS 3.2's sysiclass draws nothing offscreen for the arrow images, so the clone fails and the
 * block glyph is what shows. Through the same two steps flat.c runs (clone, then glyph draw) with the pens desk_flat_state
 * gives: the canvas must hold the glyph pen AND a different box pen, for every arrow, state and box size seen on the rig. */
static void test_arrow_glyph_pens(void)
{
    static const int dims[2][2] = { { 16, 10 }, { 18, 11 } };
    static const int states[2] = { 0, 5 };                       /* IDS_NORMAL, IDS_INACTIVEDISABLED */
    flat_state st;
    unsigned long w;
    int d, si, x, y, glyph, box;
    flat_init(&st);
    desk_flat_state(1, 1, &st);
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++)
        for (d = 0; d < 2; d++)
            for (si = 0; si < 2; si++) {
                int bw = dims[d][0], bh = dims[d][1], rc;
                cv_clear();
                rc = flat_clone_draw(&st, w, FLAT_IM_DRAW, (unsigned long)states[si], 0, 0, bw, bh, st.pen_back, -1, 99, -1, blank_render, 0, cv_fill, 0, g_scr);
                CHECK_INT(rc, FLAT_CLONE_FAIL);                  /* the system image gave no pixel */
                CHECK_INT(flat_glyph_draw(&st, w, FLAT_IM_DRAW, (unsigned long)states[si], 0, 0, bw, bh, st.pen_back, 1, cv_fill, 0), 1);
                glyph = box = 0;
                for (y = 0; y < bh; y++)
                    for (x = 0; x < bw; x++) {
                        glyph += cv[y][x] == (char)(states[si] ? st.pen_line : st.pen_text);
                        box += cv[y][x] == (char)st.pen_back;
                    }
                CHECK(glyph > 0);                                /* an arrow was drawn */
                CHECK(box > 0);                                  /* and the box shows around it */
                CHECK_INT(glyph + box, bw * bh);                 /* nothing else */
                CHECK((states[si] ? st.pen_line : st.pen_text) != st.pen_back);
            }
}

static void test_clone_arrows(void)
{
    flat_state st;
    unsigned long w;
    int g, b;
    glyph_state(&st);
    st.enabled = 1; st.held = 0;
    for (w = FLAT_W_LEFT; w <= FLAT_W_DOWN; w++) {
        CHECK_INT(flat_glyph_window_gadget(w), 1);                              /* scroller arrows take the clone path */
        CHECK_INT(flat_clone_route(&st, w, FLAT_IM_DRAW), 1);
        CHECK_INT(flat_glyph_on_band(w), 0);                                    /* they sit in the border, not the title band */
        CHECK_INT(flat_glyph_known(w), 1);                                      /* OS 3.2 draws nothing offscreen: the block glyph is the arrow */
        flat_glyph_roles(w, FLAT_G_NORMAL, &g, &b);
        CHECK_INT(b, FLAT_ROLE_FRAME);
        CHECK(g != b);                                                          /* glyph pen differs from box pen */
    }
    CHECK_INT(flat_glyph_on_band(FLAT_W_CLOSE), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_DEPTH), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_ZOOM), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_ICONIFY), 1);
    CHECK_INT(flat_glyph_on_band(FLAT_W_SIZE), 0);
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, 0), 0);                        /* only OM_DRAW */
    st.held = 1;
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, FLAT_IM_DRAW), 0);
    st.held = 0; st.enabled = 0;
    CHECK_INT(flat_clone_route(&st, FLAT_W_LEFT, FLAT_IM_DRAW), 0);
}

static void test_frame_pen(void)
{
    flat_state s;
    glyph_state(&s);
    s.enabled = 1; s.held = 0; s.pen_outline = 5;
    CHECK_INT(flat_frame_pen(&s, 3, 1), 5);          /* active, pen 5 < 8 */
    s.pen_dimtext = 0;
    CHECK_INT(flat_frame_pen(&s, 3, 0), -1);         /* inactive, no inactive pen: stock frame */
    s.pen_inbar = 9;
    CHECK_INT(flat_frame_pen(&s, 4, 0), 9);          /* inactive: the inactive pen (S32-INACTIVE-BAR) */
    CHECK_INT(flat_frame_pen(&s, 3, 0), -1);         /* pen 9 is not a pen of 3 planes */
    s.pen_inbar = 0;
    CHECK_INT(flat_frame_pen(&s, 9, 1), -1);         /* depth > 8 */
    CHECK_INT(flat_frame_pen(&s, 2, 1), -1);         /* pen not of the depth */
    s.pen_outline = 0;
    CHECK_INT(flat_frame_pen(&s, 3, 1), -1);
    s.pen_outline = 5; s.held = 1;
    CHECK_INT(flat_frame_pen(&s, 3, 1), -1);
}

/* S32-INACTIVE-BAR reachability: the STATUS dump (flat.c, DOS only) prints inbar= next to knob=, from the live block. */
static void test_inbar_status_dump(void)
{
    static char src[400000];
    size_t n;
    FILE *f = fopen("amarchy/flat.c", "rb");
    CHECK(f != 0);
    if (!f)
        return;
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "\" inbar=\");  put_num(line, &n, copy.pen_inbar)") != 0);
    CHECK(strstr(src, "flat_pens_refresh(&g_pens") != 0);                      /* the pen the dump reports is really obtained */
    CHECK(strstr(src, "flat_pens_follow(&g_pens") != 0);                       /* and follows the screen on every 1 s watch */
}

/* S32-INACTIVE-BAR: the outer ring and the scroller knob of an inactive window take the inactive pen; active ones are unchanged */
static void test_inactive_pens(void)
{
    flat_state s;
    glyph_state(&s);
    s.enabled = 1; s.pen_outline = 5; s.pen_inbar = 9; s.pen_dimtext = 0;
    CHECK_INT(flat_edge_pen(&s, 4, 1), 5);           /* active: the accent ring */
    CHECK_INT(flat_edge_pen(&s, 4, 0), 9);           /* inactive: the inactive ring */
    s.pen_inbar = 0;
    CHECK_INT(flat_edge_pen(&s, 4, 0), 5);           /* no inactive pen: the old ring */
    s.pen_inbar = 9;
    CHECK_INT(flat_inactive_pen(&s, 4), 9);
    CHECK_INT(flat_inactive_pen(&s, 3), -1);
    s.pen_dimtext = 6;
    CHECK_INT(flat_inactive_pen(&s, 3), 6);          /* under 16 colours: the muted pen */
    CHECK_INT(flat_inbar_colour(0x000000UL, 0x646464UL), 0x2d2d2dUL);   /* 45 percent */
    CHECK_INT(flat_scroller_pen(&s, 4, 0, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);   /* inactive knob */
    CHECK(flat_scroller_pen(&s, 4, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69) != 9);     /* active knob unchanged */
}


/* S32-STALE-HELD-PENS: the dim / knob / inbar pens are remembered with the screen and its epoch. After a Workbench reopen (new
 * address, or the same address with a new epoch) the held numbers belong to the old ColorMap: no ReleasePen on the new one, three
 * obtains on it, and the inbar colour derived from the NEW screen's pens. The 1 s step (flat_pens_follow) is the product entry. */
typedef struct { unsigned long col[256]; int mine[256]; int next; int obtains, releases, bad_releases; } wbscr;

static int wb_obtain(void *c, unsigned long rgb)
{
    wbscr *w = c;
    int pen = w->next++;
    w->col[pen] = rgb;
    w->mine[pen] = 1;
    w->obtains++;
    return pen;
}

static void wb_release(void *c, int pen)
{
    wbscr *w = c;
    w->releases++;
    if (!w->mine[pen])
        w->bad_releases++;
    w->mine[pen] = 0;
}

static unsigned long wb_rgb(void *c, int pen)
{
    return ((wbscr *)c)->col[pen];
}

static void wb_screen(wbscr *w, int first)
{
    memset(w, 0, sizeof *w);
    w->next = first;
    w->col[0] = 0x1a1b26UL;
    w->col[2] = 0xa9b1d6UL;
    w->col[5] = 0x414868UL;
    w->col[6] = 0x565f89UL;
}

static void test_stale_held_pens(void)
{
    wbscr a, b, c;
    flat_pens p;
    flat_pen_ops o;
    flat_state s;
    int screen_a, screen_b;
    flat_init(&s);
    s.pen_back = 0;
    s.pen_text = 2;
    s.pen_button = 5;
    s.pen_dimtext = 6;
    flat_pens_init(&p);
    o.obtain = wb_obtain;
    o.release = wb_release;
    o.rgb = wb_rgb;
    wb_screen(&a, 249);
    o.ctx = &a;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 1);
    CHECK_INT(a.obtains, 3);
    CHECK_INT(s.pen_inbar, 249);
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 0);       /* same screen, same epoch: nothing */
    CHECK_INT(a.obtains, 3);
    /* Workbench reopened as screen B: other clients own 249..251 there, ours would be 240.. */
    wb_screen(&b, 240);
    o.ctx = &b;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_b, 2, 8), 1);
    CHECK_INT(b.releases, 0);                       /* the old pens were never obtained on B */
    CHECK_INT(b.obtains, 3);
    CHECK_INT(b.col[s.pen_inbar], 0x5A5E75UL);      /* flat_inbar_colour of B's pen 0 and pen 2 */
    CHECK_INT(s.pen_inbar, 240);
    CHECK_INT(s.pen_knob != 0 && s.pen_dim != 0, 1);
    /* the same address with a new epoch is a reopened screen too */
    wb_screen(&c, 100);
    o.ctx = &c;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_b, 3, 8), 1);
    CHECK_INT(c.releases, 0);
    CHECK_INT(c.obtains, 3);
    /* Ctrl-D on the identity they were obtained on releases normally; on another identity it only forgets */
    flat_pens_release(&p, &o, &s, &screen_b, 4);
    CHECK_INT(c.releases, 0);
    CHECK_INT(s.pen_inbar, 0);
    flat_pens_refresh(&p, &o, &s, &screen_b, 4, 8);
    flat_pens_release(&p, &o, &s, &screen_b, 4);
    CHECK_INT(c.releases, 3);
    CHECK_INT(c.bad_releases, 0);
    /* the screen going away forgets */
    flat_pens_refresh(&p, &o, &s, &screen_b, 5, 8);
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, 0, 5, 0), 0);
    CHECK_INT(c.bad_releases, 0);
    CHECK_INT(s.pen_dim, 0);
}

/* S32 CLONE-PRIVATE-IMAGE: OS 3.2 sysiclass bakes its pens at OM_NEW and ignores imp_DrInfo at IM_DRAW, so drawing the window's
 * own image with the marker DrawInfo read back its baked screen pens (all 0 under flat pens: 'empty'). The clone keeps one private
 * image per (which, width, height), made with the marker DrawInfo; this is its table. The marker DrawInfo is deep enough for
 * every marker the clone reads back. */
static void test_clone_private_slot(void)
{
    flat_clone_key k[FLAT_CLONE_OBJS];
    int n = 0, i;
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 23, 11), 0);        /* empty table: append */
    k[n].which = FLAT_W_DEPTH; k[n].w = 23; k[n].h = 11; n++;
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 23, 11), 0);        /* the same image: reused, not remade */
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 23, 10), 1);        /* another size: its own image */
    CHECK_INT(flat_clone_slot(k, n, FLAT_CLONE_OBJS, FLAT_W_CLOSE, 23, 11), 1);        /* another gadget: its own image */
    for (i = n; i < FLAT_CLONE_OBJS; i++) {
        k[i].which = FLAT_W_CLOSE; k[i].w = 10 + i; k[i].h = 11;
    }
    CHECK_INT(flat_clone_slot(k, FLAT_CLONE_OBJS, FLAT_CLONE_OBJS, FLAT_W_ZOOM, 23, 11), -1);   /* full: refused, the block glyph */
    CHECK_INT(flat_clone_slot(k, FLAT_CLONE_OBJS, FLAT_CLONE_OBJS, FLAT_W_DEPTH, 23, 11), 0);   /* full, but a hit still answers */
    CHECK_INT((1 << FLAT_CLONE_DEPTH) > FLAT_CLONE_MARK(FLAT_CLONE_MARKS - 1), 1);
}

static void test_glyph_clone(void)
{
    flat_state st;
    int x, y, glyph, box;
    glyph_state(&st);
    st.enabled = 1;
    cv_clear();
    g_render_calls = g_render_fail = 0;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 2, 3, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);                                               /* the sentinel: the clone path ran */
    glyph = box = 0;
    for (y = 0; y < 9; y++)
        for (x = 0; x < 9; x++) {
            char c = cv[3 + y][2 + x];
            int in = x >= 3 && x <= 5 && y >= 3 && y <= 5;
            CHECK(c == (in ? '#' : 'F'));                                       /* ring gone, box = frame pen, glyph = text pen */
            glyph += c == '#';
            box += c == 'F';
        }
    CHECK_INT(glyph, 9);
    CHECK_INT(box, 72);
    CHECK_INT(cv[2][2], '?');                                                   /* nothing outside the box */
    /* inactive: the glyph takes the line pen, the box stays the frame pen */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 5, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(cv[4][4], '+');
    CHECK_INT(cv[0][0], 'F');
    /* inactive on the band (S32-INACTIVE-BAR): box = band pen, glyph = text_pen */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_ZOOM, FLAT_IM_DRAW, 5, 0, 0, 9, 9, 'B', 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK(cv[0][0] == 'B' && cv[4][4] == 'T');
    /* selected: ground is FILLPEN (the dominant inside marker), ring reversed; glyph in back pen on the text-pen box */
    cv_clear();
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 1, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(cv[0][0], '#');
    CHECK_INT(cv[4][4], '.');
    CHECK_INT(cv[1][1], '#');
    /* refusals: render failure is reported for the block fallback; menu glyphs, other methods, a held switch, small boxes: not routed, no render */
    {   /* active window: box = band pen, glyph = the title text pen (text_pen overrides the normal glyph only) */
        flat_state b = st;
        cv_clear();
        CHECK_INT(flat_clone_draw(&b, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'B', 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
        CHECK(cv[0][0] == 'B' && cv[4][4] == 'T');
        cv_clear();
        CHECK_INT(flat_clone_draw(&b, FLAT_W_CLOSE, FLAT_IM_DRAW, 2, 0, 0, 9, 9, 'B', 'T', 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
        CHECK(cv[4][4] == 'T');                                                 /* inactive takes the text pen too (S32-INACTIVE-BAR) */
        cv_clear();
    }
    g_render_calls = 0; g_render_fail = 1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_FAIL);
    g_render_fail = 0; g_render_calls = 0;
    cv_clear();                                                                 /* an empty clone is a failure and paints nothing */
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', 'T', 99, -1, blank_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_FAIL);
    CHECK_INT(g_render_calls, 1);
    CHECK(cv[0][0] == '?' && cv[4][4] == '?');
    g_render_calls = 0;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_MENUCHECK, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, 0x203, 0, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 6, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    st.held = 1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 9, 9, 'F', -1, 99, -1, fake_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_NO);
    CHECK_INT(g_render_calls, 0);
}

/* FLAT-NO-DIVIDERS: the stock images measured on OS 3.2 (rig 55, glyph-clone-probe-run.md, real pens: the 3.2 sysiclass
 * draws in its own pens whatever DrawInfo we pass). The close image's right columns 18-19 (shadow + shine) are the title bar
 * separator, the depth image's left columns 0-1 likewise; every full-span edge line must go, the glyph must stay whole. */
static const char *const g_os32_close[] = {
    "22222222222222222222", "23333333333333333312", "23333333333333333312", "23333331111133333312",
    "23333331222133333312", "23333331222133333312", "23333331222133333312", "23333331111133333312",
    "23333333333333333312", "23333333333333333312", "21111111111111111111" };
static const char *const g_os32_depth[] = {
    "222222222222222222222222", "123333333333333333333331", "123331111111111133333331", "123331000000000133333331",
    "123331000111111111113331", "123331000122222222213331", "123331111122222222213331", "123333333122222222213331",
    "123333333111111111113331", "123333333333333333333331", "111111111111111111111111" };
/* S32-CLONE-CLIP: the private depth image as rig 23 (3.2, intuition 47.53) drew it with the MARKER pens, dumped 32 wide with
 * the draw origin at column 4; LeftEdge is -1, so image column 0 is dump column 3 (s32-evidence/clone-clip-hunt.md).
 * Marker 5 = SHADOW (right bevel at image column 23), 8 = BACKGROUND (inside the back window). */
#define CLIP_PAD 4
static const char *const g_os32_depth_marker[] = {
    "00044444444444444444444444400000", "00054666666666666666666666500000", "00054666555555555556666666500000",
    "00054666588888888856666666500000", "00054666588855555555555666500000", "00054666588854444444445666500000",
    "00054666555554444444445666500000", "00054666666654444444445666500000", "00054666666655555555555666500000",
    "00054666666666666666666666500000", "00055555555555555555555555500000" };
static int g_clip_le;
/* what clone_render reads: the image drawn at offset flat_clone_offset(le), so image column j lands at grid column off + le + j */
static int clip_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y, j, off = flat_clone_offset(g_clip_le);
    (void)ctx; (void)ids;
    g_render_calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            j = x - off - g_clip_le;                                    /* image column at grid column x */
            grid[y * w + x] = (uint8_t)(j < 0 || j >= w ? 0 : g_os32_depth_marker[y][CLIP_PAD + g_clip_le + j] - '0');
        }
    return 1;
}
static const char *const *g_grid_rows;
static int grid_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    int x, y;
    (void)ctx; (void)ids;
    g_render_calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            grid[y * w + x] = (uint8_t)(g_grid_rows[y][x] - '0');
    return 1;
}

/* S32-START-LOOK: started before LoadWB, the first install finds no class; the 1 s watch retries it once the screen is up */
static void test_boot_before_workbench_retries_install(void)
{
    CHECK_INT(flat_install_due(0, 0, FLAT_ERR_NOCLASS), 1);
    CHECK_INT(flat_install_due(0, 1, FLAT_OK), 0);                 /* installed: never twice */
    CHECK_INT(flat_install_due(FLAT_HELD_MARKER, 0, FLAT_ERR_NOCLASS), 0);   /* a kill switch stays inert */
    CHECK_INT(flat_install_due(0, 0, FLAT_ERR_DOUBLE), 0);         /* a real swap failure is final */
    CHECK_INT(flat_install_due(0, 0, -101), 0);                    /* out of memory is final */
}

/* S32-CLONE-CLIP: the depth clone on 3.2 (LeftEdge -1, marker pens): the glyph at its stock columns 5-19, the right bevel
 * (image column 23) never a glyph line at column 22, the back window's BACKGROUND inside a hole, not solid */
static void test_clone_clip_depth_marker(void)
{
    flat_state st;
    int x, y, glyph = 0;
    glyph_state(&st);
    st.enabled = 1;
    CHECK_INT(flat_clone_offset(-1), 1);
    CHECK_INT(flat_clone_offset(0), 0);
    cv_clear();
    g_render_calls = 0;
    g_clip_le = -1;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', 'T', 99, -1, clip_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            char m = g_os32_depth_marker[y][CLIP_PAD - 1 + x];
            int want = x >= 5 && x <= 19 && y >= 2 && y <= 8 && (m == '5' || m == '4');
            CHECK(cv[y][x] == (want ? 'T' : 'B'));
            glyph += cv[y][x] == 'T';
        }
    for (y = 0; y < 11; y++)
        CHECK(cv[y][22] == 'B' && cv[y][23] == 'B');                            /* no bevel line on the right */
    CHECK(cv[3][7] == 'B' && cv[4][8] == 'B' && cv[5][8] == 'B');             /* the back window's inside: a hole */
    CHECK_INT(glyph, 74);                                            /* = the real-pen fixture g_os32_depth */
}

static void test_clone_band_rows_strip(void)
{
    flat_state st;
    int x, y, edge = 0;
    CHECK_INT(flat_clone_band_rows(0, 11, 11), 10);
    CHECK_INT(flat_clone_band_rows(2, 11, 11), 8);
    CHECK_INT(flat_clone_band_rows(0, 11, 30), 11);
    CHECK_INT(flat_clone_band_rows(12, 11, 11), 0);
    glyph_state(&st);
    st.enabled = 1;
    cv_clear();
    g_render_calls = 0;
    g_clip_le = -1;
    /* S32-TITLE-STRIP: row 10 is the title bar's bottom edge (background), not band */
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', 'T', 10, 'E', clip_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            if (y == 10) {
                CHECK(cv[y][x] == 'E' || cv[y][x] == 'T');
                edge += cv[y][x] == 'E';
            } else
                CHECK(cv[y][x] == 'B' || cv[y][x] == 'T');
        }
    CHECK(edge > 0);
}

static void test_clone_no_dividers(void)
{
    flat_state st;
    int x, y, glyph;
    glyph_state(&st);
    st.enabled = 1;
    /* close 20 x 11: glyph = the 5 x 5 square at x 7-11, y 3-7; every other pixel the band pen */
    cv_clear();
    g_render_calls = 0;
    g_grid_rows = g_os32_close;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 20, 11, 'B', 'T', 99, -1, grid_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    CHECK_INT(g_render_calls, 1);
    glyph = 0;
    for (y = 0; y < 11; y++)
        for (x = 0; x < 20; x++) {
            int in = x >= 7 && x <= 11 && y >= 3 && y <= 7;
            CHECK(cv[y][x] == (in ? 'T' : 'B'));
            glyph += cv[y][x] == 'T';
        }
    CHECK_INT(glyph, 25);
    for (y = 1; y < 10; y++)
        CHECK(cv[y][18] == 'B' && cv[y][19] == 'B');                            /* the separator columns: band pen */
    /* depth 24 x 11: the left separator (columns 0-1) and the right edge gone, both overlapping boxes kept */
    cv_clear();
    g_grid_rows = g_os32_depth;
    CHECK_INT(flat_clone_draw(&st, FLAT_W_DEPTH, FLAT_IM_DRAW, 0, 0, 0, 24, 11, 'B', 'T', 99, -1, grid_render, 0, cv_fill, 0, g_scr), FLAT_CLONE_OK);
    glyph = 0;
    for (y = 0; y < 11; y++)
        for (x = 0; x < 24; x++) {
            int edge = x < 2 || x > 22 || y == 0 || y == 10;
            if (edge)
                CHECK(cv[y][x] == 'B');
            glyph += cv[y][x] == 'T';
        }
    CHECK_INT(glyph, 74);                                                       /* both boxes of the glyph, outline and fill */
    CHECK(cv[2][5] == 'T' && cv[5][10] == 'T' && cv[4][19] == 'T');
}

/* S32-GHOST-MENU: the 3.2 menu ghost (ROM caller, no Window layer, AreaPtSz 1, {0x4444, 0x1111}) takes the dim label pen; every
 * other fill, a Window layer, another pattern, a held or off block, a screen under 8 colours or a dim pen equal to the fill: stock. */
static void test_ghost_menu_pen(void)
{
    flat_state s;
    flat_init(&s);
    desk_flat_state(1, 1, &s);
    CHECK(s.pen_dimtext != 0);
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 1, 0x4444, 0x1111, 2), s.pen_dimtext);      /* item: Layer NULL */
    CHECK_INT(flat_gm_pen(&s, 3, 1, 0, 1, 0x4444, 0x1111, 2), s.pen_dimtext);      /* 8 colours */
    CHECK_INT(flat_gm_pen(&s, 2, 1, 0, 1, 0x4444, 0x1111, 2), -1);                 /* 4 colours: no dim tone */
    CHECK_INT(flat_gm_pen(&s, 8, 0, 0, 1, 0x4444, 0x1111, 2), -1);                 /* app caller */
    CHECK_INT(flat_gm_pen(&s, 8, 1, 1, 1, 0x4444, 0x1111, 2), -1);                 /* a Window layer: the button path */
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 0, 0x4444, 0x1111, 2), -1);
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 2, 0x4444, 0x1111, 2), -1);
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 1, 0x2222, 0x8888, 2), -1);                 /* another pattern */
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 1, 0x4444, 0x1111, s.pen_dimtext), -1);     /* dim = fill: nothing to show */
    s.held = 1;
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 1, 0x4444, 0x1111, 2), -1);
    s.held = 0; s.enabled = 0;
    CHECK_INT(flat_gm_pen(&s, 8, 1, 0, 1, 0x4444, 0x1111, 2), -1);
}

static int gm_px(uint8_t *const *pl, int depth, int bpr, int x, int y)
{
    int p, v = 0;
    for (p = 0; p < depth; p++)
        if (pl[p][y * bpr + (x >> 3)] & (0x80 >> (x & 7)))
            v |= 1 << p;
    return v;
}

static void gm_set(uint8_t *const *pl, int depth, int bpr, int x, int y, int pen)
{
    int p;
    for (p = 0; p < depth; p++) {
        if (pen & (1 << p)) pl[p][y * bpr + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
        else pl[p][y * bpr + (x >> 3)] &= (uint8_t)~(0x80 >> (x & 7));
    }
}

/* the recolour core on an interleaved 3-plane bitmap (rig layout: Planes[p] = base + p * row, BytesPerRow = depth * row): label
 * pixels (pen 1) and a checkmark pixel (pen 5) in the rect become the dim pen 6, background (fill pen 2) stays, pixels outside the
 * rect stay, a rect mostly not background is refused untouched. */
static void test_ghost_menu_recolour(void)
{
    enum { ROW = 4, D = 3, H = 6 };
    static uint8_t buf[ROW * D * H];
    uint8_t *pl[D];
    int bpr = ROW * D, p, x, y, bad = 0;
    for (p = 0; p < D; p++) pl[p] = buf + p * ROW;
    for (y = 0; y < H; y++) for (x = 0; x < ROW * 8; x++) gm_set(pl, D, bpr, x, y, 2);
    gm_set(pl, D, bpr, 5, 2, 1); gm_set(pl, D, bpr, 6, 2, 1); gm_set(pl, D, bpr, 9, 3, 5);
    gm_set(pl, D, bpr, 20, 2, 1);                                  /* outside the rect (x 4..15) */
    CHECK_INT(flat_gm_recolour(pl, D, bpr, ROW * 8, H, 4, 1, 15, 4, 2, 6), 3);
    CHECK_INT(gm_px(pl, D, bpr, 5, 2), 6);
    CHECK_INT(gm_px(pl, D, bpr, 6, 2), 6);
    CHECK_INT(gm_px(pl, D, bpr, 9, 3), 6);
    CHECK_INT(gm_px(pl, D, bpr, 20, 2), 1);
    for (y = 1; y <= 4; y++)                                       /* no period-4 holes, no background touched */
        for (x = 4; x <= 15; x++)
            if (!((x == 5 || x == 6) && y == 2) && !(x == 9 && y == 3) && gm_px(pl, D, bpr, x, y) != 2) bad++;
    CHECK_INT(bad, 0);
    CHECK_INT(flat_gm_recolour(pl, D, bpr, ROW * 8, H, 28, 4, 40, 9, 2, 6), 0);   /* clipped to the bitmap, all background */
    for (y = 0; y < H; y++) for (x = 0; x < 8; x++) gm_set(pl, D, bpr, x, y, 7);
    CHECK_INT(flat_gm_recolour(pl, D, bpr, ROW * 8, H, 0, 0, 7, 5, 2, 6), -1);     /* not a menu fill: refused */
    CHECK_INT(gm_px(pl, D, bpr, 3, 3), 7);
    CHECK_INT(flat_gm_recolour(pl, D, bpr, ROW * 8, H, 8, 0, 7, 5, 2, 6), -1);     /* empty rect */
}

void suite_flat_core(void)
{
    test_ghost_menu_pen();
    test_ghost_menu_recolour();
    test_frame_pen();
    test_inactive_pens();
    test_inbar_status_dump();
    test_stale_held_pens();
    test_arrow_glyph_pens();
    test_arrow_aspect();
    test_clone_arrows();
    test_glyph_clone();
    test_clone_private_slot();
    test_clone_no_dividers();
    test_clone_clip_depth_marker();
    test_clone_band_rows_strip();
    test_boot_before_workbench_retries_install();
    test_title_accent();
    test_scroller();
    test_knob_pen();
    test_desk_flat_state();
    test_kill_switches();
    test_which_from_gtype();
    test_layout();
    test_init_check();
    test_status();
    test_exit();
    test_swap();
    test_tramp();
    test_dopus_gate();
    test_disabled_button_dim_fill();
    test_dopus_swap();
    test_dopus_decide();
    test_dopus_path();
    test_glyph_goldens();
    test_glyph_solid_and_state_equal();
    test_glyph_states();
    test_glyph_frame_bg();
    test_glyph_route();
}
