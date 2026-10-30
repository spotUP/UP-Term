/* The amarchy-flat state block (amarchy/flat_core.c; plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0). The block
 * is shared with Amarchy through the public semaphore, so its offsets are a contract: pinned here byte by byte. */
#include <stddef.h>
#include <string.h>
#include "../amarchy/flat_core.h"
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
    /* the built-in table knows 73.8 but has no measured sum yet: a real library stays inert */
    CHECK(flat_dopus_table(&n) != 0 && n >= 1);
    sums[0] = sums[1] = 1;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 73, 8, sums, &why), 0);
    CHECK_INT(why, FLAT_GATE_UNMEASURED);
    /* sum and jump parsing */
    CHECK_INT(flat_sum(fn, 8), 3);
    CHECK_INT(flat_sum(fn, 3), 1);                                      /* never 0 */
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
    flat_dopus_status_line(0, 0, 0, 0, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=absent");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNKNOWN, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unknown");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNMEASURED, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unmeasured");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_OK, 3, 1, 12, 9, line, sizeof line);
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

void suite_flat_core(void)
{
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
}
