/* The amarchy-flat state block (amarchy/flat_core.c; plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0). The block
 * is shared with Amarchy through the public semaphore, so its offsets are a contract: pinned here byte by byte. */
#include <stddef.h>
#include <string.h>
#include "../amarchy/flat_core.h"
#include "../amarchy/flat_glyph.h"
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
    CHECK_INT(offsetof(flat_state, reserved), 19);
    CHECK_STR(FLAT_SEM_NAME, "Amarchy.Flat");
    CHECK_INT(FLAT_VERSION, 1);
}

static void test_init_check(void)
{
    flat_state s;
    unsigned char raw[40];
    memset(&s, 0xAA, sizeof s);
    flat_init(&s);
    CHECK_INT(s.version, FLAT_VERSION);
    CHECK_INT(s.size, FLAT_STATE_SIZE);
    CHECK_INT(s.enabled | s.style | s.held | s.still_chained | s.calls | s.pen_text | s.pen_line | s.pen_back | s.reserved, 0);
    CHECK_INT(flat_check(&s, sizeof s), FLAT_OK);
    CHECK_INT(flat_check(&s, sizeof s + 20), FLAT_OK);
    CHECK_INT(flat_check(&s, FLAT_STATE_SIZE - 1), FLAT_ERR_SIZE);      /* fewer readable bytes than the layout */
    CHECK_INT(flat_check(&s, 3), FLAT_ERR_SIZE);                         /* not even the header */
    CHECK_INT(flat_check(0, 40), FLAT_ERR_SIZE);
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
    if (flat_glyph_draw(st, which, method, ids, ox, oy, w, h, cv_fill, 0))
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

static void test_glyph_states(void)
{
    flat_state st;
    flat_prim n[FLAT_GLYPH_MAX_PRIMS], s[FLAT_GLYPH_MAX_PRIMS], d[FLAT_GLYPH_MAX_PRIMS];
    int cn, cs, cd, i;
    glyph_state(&st);
    cn = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_NORMAL, FLAT_STYLE_LINE, n, FLAT_GLYPH_MAX_PRIMS);
    cs = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_SELECTED, FLAT_STYLE_LINE, s, FLAT_GLYPH_MAX_PRIMS);
    cd = flat_glyph_plan(FLAT_W_CLOSE, 14, 12, FLAT_G_INACTIVE, FLAT_STYLE_LINE, d, FLAT_GLYPH_MAX_PRIMS);
    CHECK_INT(cn, 5);
    CHECK_INT(cs, cn);
    CHECK_INT(cd, cn);
    CHECK_INT(n[0].role, FLAT_ROLE_BACK);                           /* first rect = the box in the background role */
    CHECK_INT(s[0].role, FLAT_ROLE_TEXT);                           /* selected inverts */
    CHECK_INT(d[0].role, FLAT_ROLE_BACK);
    for (i = 1; i < cn; i++) {
        CHECK_INT(n[i].role, FLAT_ROLE_TEXT);
        CHECK_INT(s[i].role, FLAT_ROLE_BACK);
        CHECK_INT(d[i].role, FLAT_ROLE_LINE);                       /* inactive: the thin outline pen */
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
    /* the block style (OD-F4 b) fills where the line style leaves the inside empty */
    cv_clear();
    st.style = FLAT_STYLE_BLOCK;
    fake_dispatch(&st, FLAT_W_CLOSE, FLAT_IM_DRAW, 0, 0, 0, 14, 12);
    CHECK_INT(cv[5][6], '#');
    CHECK_INT(cv[0][0], '.');
}

static void test_glyph_route(void)
{
    flat_state st;
    static const unsigned long unknown[] = { 0x04, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x05, 0x1B, 0x2A, 0xFFFF };
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

struct frame_rec_t { int n, r[8][5]; };
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
        fs.enabled = 1; fs.pen_line = 1; fs.pen_back = 0;
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 1, frame_rec, &f), 1);
        CHECK_INT(f.n, 5);
        CHECK_INT(f.r[0][0] == 10 && f.r[0][1] == 20 && f.r[0][2] == 17 && f.r[0][3] == 25 && f.r[0][4] == 0, 1);
        CHECK_INT(f.r[1][0] == 10 && f.r[1][1] == 20 && f.r[1][2] == 17 && f.r[1][3] == 20 && f.r[1][4] == 1, 1);
        CHECK_INT(f.r[2][1] == 25 && f.r[2][3] == 25 && f.r[3][2] == 10 && f.r[4][0] == 17 && f.r[4][4] == 1, 1);
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 8, 6, 0, frame_rec, &f), 1);
        CHECK_INT(f.n, 4);
        memset(&f, 0, sizeof f);
        CHECK_INT(flat_frame_draw(&fs, 1, 10, 20, 8, 6, 1, frame_rec, &f), 1);
        CHECK_INT(f.r[0][4], 1);                                               /* selected: pen_line fill */
        CHECK_INT(flat_frame_draw(&fs, 0, 10, 20, 2, 6, 1, frame_rec, &f), 0);   /* too small: stock */
        CHECK_INT(flat_frame_draw(0, 0, 10, 20, 8, 6, 1, frame_rec, &f), 0);
    }
    CHECK_INT((int)flat_which_alias(5), FLAT_W_DEPTH);
    CHECK_INT((int)flat_which_alias(3), FLAT_W_CLOSE);
}

void suite_flat_core(void)
{
    test_which_from_gtype();
    test_layout();
    test_init_check();
    test_status();
    test_exit();
    test_swap();
    test_tramp();
    test_dopus_gate();
    test_dopus_swap();
    test_dopus_decide();
    test_dopus_path();
    test_glyph_goldens();
    test_glyph_states();
    test_glyph_route();
}
