/* The patina-flat state block (render/flat_core.c; plan 2026-10-29-patina-flat-ui-patch.md FLAT-C0). The block
 * is shared with Patina through the public semaphore, so its offsets are a contract: pinned here byte by byte. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../render/flat_core.h"
#include "../render/flat_glyph.h"
#include "../render/desk_core.h"
#include "../render/iconpen_core.h"
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
    CHECK_INT(offsetof(flat_state, pen_sel), 29);
    CHECK_INT(offsetof(flat_state, sel_rgb), 30);
    CHECK_INT(offsetof(flat_state, side), 33);
    CHECK_INT(offsetof(flat_state, corner_radius), 36);
    CHECK_INT(offsetof(flat_state, border_width), 37);
    CHECK_INT(offsetof(flat_state, knob_shape), 38);
    CHECK_INT(offsetof(flat_state, knob_min), 39);
    CHECK_INT(offsetof(flat_state, glyph_set), 40);
    CHECK_INT(offsetof(flat_state, ghost_style), 41);
    CHECK_INT(offsetof(flat_state, hover), 42);
    CHECK_INT(offsetof(flat_state, knob_rgb), 44);
    CHECK_INT(FLAT_STATE_SIZE, 48);
    CHECK_STR(FLAT_SEM_NAME, "Patina.Flat");
    CHECK_INT(FLAT_VERSION, 5);
    {   /* a version-4 reader (the check as it was: version word 4 and a 36-byte block) refuses a v5 block before the rest */
        flat_state v5;
        flat_init(&v5);
        CHECK(v5.version != 4);
        CHECK(v5.size != 36);
        v5.version = 4;                              /* and the v5 reader refuses a v4 block the same way */
        v5.size = 36;
        CHECK_INT(flat_check(&v5, 36), FLAT_ERR_VERSION);
    }
}

static void test_init_check(void)
{
    flat_state s;
    unsigned char raw[FLAT_STATE_SIZE + 4];
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
    /* the bytes of a valid block as Patina writes them: big-endian on the Amiga, native on the host */
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
    /* FLAT-LOOP-BEAT: the main loop trace line */
    CHECK_INT(flat_loop_line(7, FLAT_STG_WAIT, FLAT_REF_NONE, 0, line, sizeof line) > 0, 1);
    CHECK_STR(line, "loop beat=7 stage=wait refused=none refusals=0");
    flat_loop_line(9, FLAT_STG_REAP_ICL, FLAT_REF_DOPUS, 2, line, sizeof line);
    CHECK_STR(line, "loop beat=9 stage=reap_icl refused=dopus refusals=2");
    flat_loop_line(1, FLAT_STG_FREE, FLAT_REF_RECORD, 1, line, sizeof line);
    CHECK_STR(line, "loop beat=1 stage=free refused=record refusals=1");
    flat_loop_line(3, FLAT_STG_CG_REMOVE, FLAT_REF_GADTOOLS, 1, line, sizeof line);     /* S74-TIME-ROW: the CreateGadgetA patch */
    CHECK_STR(line, "loop beat=3 stage=cg_remove refused=gadtools refusals=1");
    CHECK_STR(flat_trow_why_name(FLAT_TROW_OK), "ok");
    CHECK_STR(flat_trow_why_name(FLAT_TROW_NOBUTTON), "nobutton");
    CHECK_STR(flat_trow_why_name(FLAT_TROW_ROW), "row");
    CHECK_STR(flat_trow_why_name(FLAT_TROW_ROW + 1), "bad");
    CHECK_STR(flat_trow_why_name(-1), "bad");
    flat_loop_line(1, FLAT_STG_COUNT, FLAT_REF_COUNT, 0, line, sizeof line);
    CHECK_STR(line, "loop beat=1 stage=bad refused=bad refusals=0");
    CHECK_INT(flat_loop_line(1, 0, 0, 0, line, 63), 0);                  /* a buffer under 64 bytes is refused */
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
    flat_dopus_build tab[2] = { { 73, 8, { 0x1111, 0x2222 }, 0 }, { 73, 9, { 0, 0 }, 0 } };
    uint32_t sums[FLAT_EL_COUNT] = { 0x1111, 0x2222 };
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
    /* Directory Opus 5.82 (Magellan II, dopus5.library 68.16), sums measured on rig 95: the bevel patch must engage on it */
    sums[0] = 0x05a329c5u; sums[1] = 0xaa2c535eu;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 68, 16, sums, &why), 3);
    CHECK_INT(why, FLAT_GATE_OK);
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 68, 15, sums, &why), 0);   /* another 5.82 revision stays inert */
    CHECK_INT(why, FLAT_GATE_UNKNOWN);
    sums[0] ^= 1; sums[1] ^= 1;
    CHECK_INT(flat_dopus_gate(flat_dopus_table(&n), n, 68, 16, sums, &why), 0);   /* same version, other routine bytes: inert */
    CHECK_INT(why, FLAT_GATE_SUM);
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

/* s66 0c buttons: dopusbuttongclass (Use / Save / Cancel) and dopusiclass frames draw through dopus5's internal static
 * bevel 0x166d8 (no LVO, absolute jsr from 0x15d7a, 0x19012, 0x19370, 0x1a9da), not DrawBox / FieldBox, so neither D4
 * element saw them and they stayed pen 0 under the flat theme. The real 5.104 bytes: the bevel body is found at
 * DrawBox body - 0xde34, its 8-byte prologue is relocatable, and its sum gates it like the other two elements. */
static void test_dopus_bevel_hook(void)
{
    static const unsigned char bevel[64] = {
        0x48,0xe7,0x38,0x32,0x41,0xef,0x00,0x20,0x24,0x18,0x26,0x58,0x24,0x58,0x28,0x18,0x36,0x28,0x00,0x02,0x30,0x7c,
        0x00,0x08,0x70,0x01,0xb0,0x84,0x67,0x04,0x30,0x7c,0x00,0x06,0x30,0x33,0x88,0x00,0x22,0x42,0x2c,0x79,0x00,0x00,
        0x00,0x08,0x4e,0xae,0xfe,0xaa,0x30,0x12,0xd0,0x6a,0x00,0x04,0x4a,0x43,0x56,0xc1,0x48,0x81,0x54,0x41};
    static const unsigned char box[8] = {0x48,0xe7,0x30,0x12,0x24,0x08,0x26,0x49};
    unsigned char b2[64], t[FLAT_BODY_TRAMP_MAX], orig[FLAT_PRO_MAX];
    uint32_t sums[FLAT_EL_COUNT] = { 0x780fd46bu, 0xba2cf172u, 0 };
    const flat_dopus_build *tab;
    flat_swap w;
    char line[160];
    int n, why = -1;
    tab = flat_dopus_table(&n);
    CHECK_INT(flat_bevel_body(tab, n, 73, 8, 0x2450c), 0x166d8);       /* the jsr target of the button render */
    CHECK_INT(flat_bevel_body(tab, n, 73, 8, 0x0102450c), 0x010166d8); /* position independent */
    CHECK_INT(flat_bevel_body(tab, n, 68, 16, 0x2450c), 0);            /* 5.82: offset not measured, never hooked */
    CHECK_INT(flat_bevel_body(tab, n, 73, 9, 0x2450c), 0);
    CHECK_INT(flat_bevel_body(tab, n, 73, 8, 0), 0);
    CHECK_INT(flat_bevel_body(tab, n, 73, 8, 0x100), 0);               /* would wrap below 0 */
    CHECK_INT(flat_body_prologue(FLAT_EL_BEVEL, bevel), 8);
    CHECK_INT(flat_body_prologue(FLAT_EL_BEVEL, box), 0);              /* each element only its own prologue */
    CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, bevel), 0);
    memcpy(b2, bevel, 64); b2[7] = 0x1c;                               /* lea 28(sp): another frame, no hook */
    CHECK_INT(flat_body_prologue(FLAT_EL_BEVEL, b2), 0);
    CHECK_INT(flat_sum_reloc(bevel, FLAT_FUNC_BYTES), 0xbdaa116au);    /* the table sum is these bytes */
    memcpy(b2, bevel, 64); b2[42] = 0x12; b2[45] = 0x34;               /* GfxBase relocated elsewhere: same sum */
    CHECK_INT(flat_sum_reloc(b2, FLAT_FUNC_BYTES), 0xbdaa116au);
    CHECK_INT(flat_dopus_gate(tab, n, 73, 8, sums, &why), 3);           /* bevel sum missing: DrawBox + FieldBox only */
    sums[FLAT_EL_BEVEL] = flat_sum_reloc(bevel, FLAT_FUNC_BYTES);
    CHECK_INT(flat_dopus_gate(tab, n, 73, 8, sums, &why), 7);           /* all three elements */
    CHECK_INT(why, FLAT_GATE_OK);
    /* the hook itself: JMP over the 8-byte prologue, trampoline = prologue + JMP body+8, removal restores the bytes */
    memcpy(b2, bevel, 64);
    flat_swap_init(&w);
    CHECK_INT(flat_body_install(&w, b2, 8, t, orig, 0x00c0ffee), FLAT_OK);
    CHECK_INT(flat_jmp_target(b2), 0x00c0ffee);
    CHECK(memcmp(t, bevel, 8) == 0 && flat_jmp_target(t + 8) == (uint32_t)(uintptr_t)(b2 + 8));
    CHECK_INT(flat_body_remove(&w, b2, orig, 8, 0), FLAT_REMOVED);
    CHECK(memcmp(b2, bevel, 64) == 0);
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_OK, 7, 1, 4, 4, sums, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 patched=7 calls=4 flat=4 sums=780fd46b,ba2cf172,bdaa116a,00000000");
}

/* 0c: the body hook's pure parts, on the real dopus5.library 73.8 (DOpus 5.104 OS3) bytes: .text 0x245c4 (the -702 stub),
 * 0x2450c (DrawBox body), 0x245de (FieldBox body). */
static void test_dopus_body_hook(void)
{
    static const unsigned char stub[18] = {0x2f,0x0a,0x2f,0x39,0x00,0x00,0x00,0x08,0x2f,0x0a,0x2f,0x09,0x2f,0x08,0x4e,0xba,0x00,0x0a};
    static const unsigned char box[8] = {0x48,0xe7,0x30,0x12,0x24,0x08,0x26,0x49};
    static const unsigned char field[10] = {0x48,0xe7,0x20,0x32,0x41,0xef,0x00,0x14,0x24,0x18};
    unsigned char s2[18], b2[10], t[FLAT_BODY_TRAMP_MAX], j[6];
    CHECK_INT(flat_fieldbox_body(stub, 0x245c4), 0x245de);             /* the bsr target the disassembly found */
    CHECK_INT(flat_fieldbox_body(stub, 0x1000000), 0x100001a);        /* position independent */
    memcpy(s2, stub, 18); s2[4] = 0x12; s2[7] = 0x34;                  /* another GfxBase: still the stub */
    CHECK_INT(flat_fieldbox_body(s2, 0x245c4), 0x245de);
    s2[16] = 0xff; s2[17] = 0xf0;                                      /* a backward bsr */
    CHECK_INT(flat_fieldbox_body(s2, 0x245c4), 0x245c4 + 16 - 16);
    memcpy(s2, stub, 18); s2[13] = 0x09;                               /* other code: no body */
    CHECK_INT(flat_fieldbox_body(s2, 0x245c4), 0);
    CHECK_INT(flat_fieldbox_body(0, 0x245c4), 0);
    {   /* DOpus 5.82 FieldBox stub and body, read from rig 7 (stub 0x10270876, body 0x1027089a): the requester string
         * field stayed stock because neither matched and FieldBox fell back to the LVO swap dopusstrgclass never calls */
        static const unsigned char st82[32] = {0x59, 0x4f, 0x2f, 0x0c, 0x49, 0xee, 0x00, 0x30, 0x2f, 0x2c, 0x06, 0x70,
                                               0x2f, 0x0a, 0x2f, 0x09, 0x2f, 0x08, 0x2f, 0x4e, 0x00, 0x14, 0x61, 0x00,
                                               0x00, 0x0c, 0x4f, 0xef, 0x00, 0x10, 0x28, 0x5f};
        static const unsigned char bd82[8] = {0x59, 0x4f, 0x48, 0xe7, 0x00, 0x36, 0x24, 0x6f};
        unsigned char bad[32];
        CHECK_INT(flat_fieldbox_body(st82, 0x10270876), 0x1027089a);
        memcpy(bad, st82, 32); bad[22] = 0x4e;                          /* not a bsr.w: refused */
        CHECK_INT(flat_fieldbox_body(bad, 0x10270876), 0);
        CHECK_INT(flat_body_prologue(FLAT_EL_FIELDBOX, bd82), 6);
        CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, bd82), 0);
        CHECK_INT(flat_body_prologue(FLAT_EL_BEVEL, bd82), 0);
    }
    CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, box), 6);
    CHECK_INT(flat_body_prologue(FLAT_EL_FIELDBOX, field), 8);
    CHECK_INT(flat_body_prologue(FLAT_EL_FIELDBOX, box), 0);           /* each element only its own prologue */
    CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, field), 0);
    memcpy(b2, box, 8); b2[5] = 0x09;                                  /* move.l a1,d2: another build, keep the LVO swap */
    CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, b2), 0);
    memcpy(b2, field, 10); b2[7] = 0x18;                               /* lea 24(sp): another frame */
    CHECK_INT(flat_body_prologue(FLAT_EL_FIELDBOX, b2), 0);
    b2[0] = 0x4e; b2[1] = 0xf9;                                        /* already hooked (ours or foreign): no second hook */
    CHECK_INT(flat_body_prologue(FLAT_EL_FIELDBOX, b2), 0);
    CHECK_INT(flat_body_prologue(FLAT_EL_DRAWBOX, 0), 0);
    flat_jmp_build(j, 0x00c01234);
    CHECK(j[0] == 0x4e && j[1] == 0xf9 && j[2] == 0x00 && j[3] == 0xc0 && j[4] == 0x12 && j[5] == 0x34);
    CHECK_INT(flat_jmp_target(j), 0xc01234);
    CHECK_INT(flat_body_tramp_build(t, field, 8, 0x245de + 8), 14);
    CHECK(memcmp(t, field, 8) == 0);
    CHECK_INT(flat_jmp_target(t + 8), 0x245e6);
    CHECK_INT(flat_body_tramp_build(t, box, 6, 0x2450c + 6), 12);
    CHECK(memcmp(t, box, 6) == 0);
    CHECK_INT(flat_jmp_target(t + 6), 0x24512);
    CHECK_INT(flat_body_tramp_build(t, box, 4, 0x24510), 0);          /* shorter than the JMP it is replaced by */
    CHECK_INT(flat_body_tramp_build(t, box, 10, 0x24516), 0);         /* longer than the trampoline holds */
    CHECK_INT(flat_body_tramp_build(t, box, 7, 0x24513), 0);          /* odd: not an instruction boundary */
}

/* 0c: install / remove of the body hook on a fake body (the real DrawBox and FieldBox prologues). */
static void test_dopus_body_swap(void)
{
    static const unsigned char field[10] = {0x48,0xe7,0x20,0x32,0x41,0xef,0x00,0x14,0x24,0x18};
    unsigned char body[10], orig[FLAT_PRO_MAX], tramp[FLAT_BODY_TRAMP_MAX];
    flat_swap w;
    flat_state st;
    flat_init(&st);
    flat_swap_init(&w);
    memcpy(body, field, 10);
    CHECK_INT(flat_body_install(&w, body, 8, tramp, orig, 0x7000), FLAT_OK);
    CHECK_INT(flat_jmp_target(body), 0x7000);                          /* the body entry now jumps to ours */
    CHECK(memcmp(tramp, field, 8) == 0);                               /* saved path: the relocated prologue ... */
    CHECK_INT(flat_jmp_target(tramp + 8), (uint32_t)((uintptr_t)body + 8));   /* ... and on into the body */
    CHECK_INT(w.saved, (uintptr_t)tramp);
    CHECK_INT(flat_body_install(&w, body, 8, tramp, orig, 0x7100), FLAT_ERR_DOUBLE);
    body[2] = 0x00; body[5] = 0x99;                                    /* a foreign patch on top */
    CHECK_INT(flat_body_remove(&w, body, orig, 8, &st), FLAT_STILL_CHAINED);
    CHECK_INT(st.still_chained, 1);
    flat_jmp_build(body, 0x7000);
    CHECK_INT(flat_body_remove(&w, body, orig, 8, &st), FLAT_REMOVED);
    CHECK(memcmp(body, field, 10) == 0);                               /* the body is byte for byte as found */
    CHECK_INT(st.still_chained, 0);
    CHECK_INT(flat_body_install(&w, body, 4, tramp, orig, 0x7000), FLAT_ERR_ARG);   /* shorter than the JMP: refused */
    CHECK(memcmp(body, field, 10) == 0);
}

static void test_dopus_swap(void)
{
    uintptr_t slot[FLAT_EL_COUNT] = { 0x5000, 0x6000 }, snap[FLAT_EL_COUNT];
    flat_swap w[FLAT_EL_COUNT];
    flat_state st;
    int el;
    flat_init(&st);
    memcpy(snap, slot, sizeof slot);
    for (el = 0; el < FLAT_EL_BEVEL; el++) {             /* the two LVO elements; the bevel has no slot */
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
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0, -1, -1), FLAT_DRAW_STOCK);       /* flat off: stock */
    s.enabled = 1;
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0, -1, -1), FLAT_DRAW_SKIP);        /* flat on, flat pens: no shadow L */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 1, 0, 0, -1, -1), FLAT_DRAW_SKIP);        /* recessed too */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 0, 0, 0, -1, -1), FLAT_DRAW_SKIP);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 2, 0, -1, -1), FLAT_DRAW_STOCK);       /* stock pens (shine != back): stock bevel */
    /* B10 (a), owner 2026-11-07: the colours decide, not the pen numbers. Two pens of one colour are flat; a stock-look
     * clone (SHINEPEN 2 white, BACKGROUNDPEN 0 grey) keeps its bevel; equal pens are flat whatever the colour map says. */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 2, 0, 0x1A1B26, 0x1A1B26), FLAT_DRAW_SKIP);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 0, 5, 0, 0x1A1B26, 0x1A1B26), FLAT_DRAW_SKIP);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 2, 0, 0xFFFFFF, 0xAAAAAA), FLAT_DRAW_STOCK);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 2, 0, 0x1A1B26, -1), FLAT_DRAW_STOCK);   /* one colour unknown: pens decide */
    /* FLAT-INPUT-NOT-FLAT: an input field under flat on is flat with stock pens too (rig 7 Workbench: SHINE a9b1d6, BACK 1a1b26) */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 0, 2, 0, 0xA9B1D6, 0x1A1B26), FLAT_DRAW_SKIP);
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 1, 2, 0, -1, -1), FLAT_DRAW_SKIP);
    s.held = 1;
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 0, 0, -1, -1), FLAT_DRAW_STOCK);       /* kill switch */
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_FIELDBOX, 0, 2, 0, -1, -1), FLAT_DRAW_STOCK);      /* kill switch holds for fields too */
    s.held = 0;
    CHECK_INT(flat_dopus_decide(&s, 5, 0, 0, 0, -1, -1), FLAT_DRAW_STOCK);
    CHECK_INT(flat_dopus_decide(0, FLAT_EL_DRAWBOX, 0, 0, 0, -1, -1), FLAT_DRAW_STOCK);
    flat_dopus_status_line(0, 0, 0, 0, 0, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=absent");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNKNOWN, 0, 0, 0, 0, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unknown");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_UNMEASURED, 0, 0, 0, 0, sm, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 inert=unmeasured sums=0001abcd,deadbeef,00000000,00000000");
    flat_dopus_status_line(1, 73, 8, FLAT_GATE_OK, 3, 1, 12, 9, 0, line, sizeof line);
    CHECK_STR(line, "dopus=73.8 patched=3 calls=12 flat=9");
}

/* The whole path as flat.c runs it: gate -> install over a fake jump table -> a patched call decides per call -> remove. */
static unsigned long g_skipped, g_stock;
static void patched_call(const flat_state *s, int el, int shine, int back)
{
    if (flat_dopus_decide(s, el, 0, shine, back, -1, -1) == FLAT_DRAW_SKIP)
        g_skipped++;
    else
        g_stock++;
}

static void test_dopus_path(void)
{
    flat_dopus_build tab[1] = { { 73, 8, { 0xAB, 0xCD }, 0 } };
    uint32_t sums[FLAT_EL_COUNT] = { 0xAB, 0xCD };
    uintptr_t slot[FLAT_EL_COUNT] = { 0x5000, 0x6000 };
    flat_swap w[FLAT_EL_COUNT];
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

/* FLAT-MENU-PANEL: in a 144 x 112 menu layer (rig 92 measure) the fill and erases (pen 0) and the 2 px side / 1 px top and bottom
 * outline strips (pen 2) take the menu pen 4; a short strip or a mid-panel fill in another pen (a separator) stays stock. */
static void test_menu_panel(void)
{
    CHECK_INT(flat_menu_fill_pen(4, 0, 0, 0, 0, 143, 111, 144, 112), 4);   /* panel fill */
    CHECK_INT(flat_menu_fill_pen(4, 0, 0, 10, 20, 60, 28, 144, 112), 4);   /* text erase */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 0, 0, 1, 111, 144, 112), 4);     /* left outline */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 142, 0, 143, 111, 144, 112), 4); /* right outline */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 0, 0, 143, 0, 144, 112), 4);     /* top outline */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 0, 111, 143, 111, 144, 112), 4); /* bottom outline */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 4, 50, 139, 50, 144, 112), -1);  /* separator line inside: stock */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 0, 0, 1, 20, 144, 112), -1);     /* short edge piece: stock */
    CHECK_INT(flat_menu_fill_pen(4, 2, 0, 0, 0, 2, 111, 144, 112), -1);    /* 3 px wide: not an outline */
    CHECK_INT(flat_menu_fill_pen(-1, 0, 0, 0, 0, 143, 111, 144, 112), -1); /* no selection pen held */
    CHECK_INT(flat_menu_fill_pen(4, 4, 0, 0, 0, 143, 111, 144, 112), -1);  /* already the menu pen */
    CHECK_INT(flat_menu_fill_pen(4, 0, 0, 5, 0, 4, 9, 144, 112), -1);      /* empty rect */
}

/* OWNER-2026-11-10 INPUT-GROUND: the input ground is the held selection pen only; a string gadget is told by type, StringExtend flag
 * and presence; a frame is a string border only when every edge sits within 4 px of the gadget box (a group box is not). */
static void test_input_ground(void)
{
    flat_state s;
    memset(&s, 0, sizeof s);
    s.pen_sel = 5;
    CHECK_INT(flat_input_pen(&s, 4), 5);
    CHECK_INT(flat_input_pen(&s, 2), -1);                 /* depth < 3: no selection pen */
    CHECK_INT(flat_input_pen(&s, 8), 5);
    s.pen_sel = 9;
    CHECK_INT(flat_input_pen(&s, 3), -1);                 /* pen outside the depth */
    s.pen_sel = 0;
    CHECK_INT(flat_input_pen(&s, 4), -1);                 /* none held */
    CHECK_INT(flat_input_pen(0, 4), -1);
    /* INPUT-GROUND: the strgclass dispatcher writes the ground pens on GM_RENDER only */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x2000 | 1, 1, 5), 1);
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x2000, 1, 0), 1);          /* pen 0 is a pen */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER + 1, 1, 0, 0x2000, 1, 5), 0);      /* GM_GOACTIVE */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 0, 0, 0x2000, 1, 5), 0);          /* no SpecialInfo */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 1, 1, 5), 0);               /* no GACT_STRINGEXTEND */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x2000, 0, 5), 0);          /* no Extension */
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x2000, 1, -1), 0);         /* no pen on this screen */
    /* INPUT-TEXT: DOpus path field has Pens[0] = 1 (414868 on the flat palette); the screen TEXTPEN replaces it */
    CHECK_INT(flat_input_text_pen(2, 4), 2);
    CHECK_INT(flat_input_text_pen(2, 2), -1);              /* depth < 3 */
    CHECK_INT(flat_input_text_pen(16, 4), -1);             /* pen past the colours */
    CHECK_INT(flat_input_text_pen(-1, 8), -1);
    CHECK_INT(flat_string_ext(4, 0, 0x2000 | 1, 1, 1, 0), 1);
    CHECK_INT(flat_string_ext(4, 0, 1, 1, 1, 0), 0);            /* no GACT_STRINGEXTEND */
    CHECK_INT(flat_string_ext(1, 0, 0x2000, 1, 1, 0), 0);       /* boolean gadget */
    CHECK_INT(flat_string_ext(4, 0, 0x2000, 0, 1, 0), 0);
    CHECK_INT(flat_string_ext(4, 0, 0x2000, 1, 0, 0), 0);
    CHECK_INT(flat_string_ext(5, 0, 0x2000 | 1, 1, 1, 1), 1);      /* INPUT-GROUND: boopsi dopusstrgclass chain holds strgclass */
    CHECK_INT(flat_string_ext(5, 0, 0x2000 | 1, 1, 1, 0), 0);      /* custom gadget without strgclass */
    CHECK_INT(flat_string_ext(5, 0, 1, 1, 1, 1), 0);               /* no GACT_STRINGEXTEND */
    CHECK_INT(flat_string_ext(5, 0, 0x2000, 1, 0, 1), 0);          /* no extension */
    /* extension flag: GFLG_STRINGEXTEND 0x0400 in Flags OR GACT_STRINGEXTEND 0x2000 in Activation; 0x0800 is GACT_LONGINT (the old wrong bit) */
    CHECK_INT(flat_string_has_extend(0x0400, 0), 1);
    CHECK_INT(flat_string_has_extend(0, 0x2000), 1);
    CHECK_INT(flat_string_has_extend(0, 0x0800), 0);
    CHECK_INT(flat_string_ext(4, 0x0400, 0, 1, 1, 0), 1);
    CHECK_INT(flat_string_ext(4, 0, 0x2000, 1, 1, 0), 1);
    CHECK_INT(flat_string_ext(4, 0, 0x0800, 1, 1, 0), 0);
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0x0400, 0, 1, 5), 1);
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x2000, 1, 5), 1);
    CHECK_INT(flat_string_render_write(FLAT_GM_RENDER, 1, 0, 0x0800, 1, 5), 0);
    /* INSTALLER-STRING-FILL: the Installer's hand-built GTYP_STRGADGET (no gadtools, no frameiclass frame, StringExtend unknown) is
     * grounded when a fill lies inside its box (gadget 92 at 164,331 472x12): with or without StringExtend, never a custom gadget */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 472, 12, 164, 331, 635, 342), 1);   /* whole box erase */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 472, 12, 200, 331, 635, 342), 1);   /* erase after the text */
    CHECK_INT(flat_plain_string_fill(4 | 0x10, 1, 164, 331, 472, 12, 164, 331, 635, 342), 1);   /* type bits above the mask */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 472, 12, 160, 331, 635, 342), 0);   /* rect leaves the box (window erase) */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 472, 12, 164, 331, 635, 343), 0);
    CHECK_INT(flat_plain_string_fill(4, 0, 164, 331, 472, 12, 164, 331, 635, 342), 0);   /* no StringInfo */
    CHECK_INT(flat_plain_string_fill(1, 1, 164, 331, 472, 12, 164, 331, 635, 342), 0);   /* boolean gadget */
    CHECK_INT(flat_plain_string_fill(5, 1, 164, 331, 472, 12, 164, 331, 635, 342), 0);   /* boopsi: strgclass path (g9_ground) */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 0, 12, 164, 331, 164, 342), 0);     /* empty box */
    CHECK_INT(flat_plain_string_fill(4, 1, 164, 331, 472, 12, 300, 331, 299, 342), 0);   /* empty rect */
    CHECK_INT(flat_string_frame(10, 10, 104, 18, 12, 12, 100, 14), 1);    /* bevel 2 px out */
    CHECK_INT(flat_string_frame(12, 12, 100, 14, 12, 12, 100, 14), 1);
    CHECK_INT(flat_string_frame(54, 32, 53, 14, 60, 35, 41, 8), 1);      /* GadTools bevel: 6 px out horizontally, 3 vertically (Prefs/Time year) */
    CHECK_INT(flat_string_frame(54, 32, 53, 14, 60, 35, 41, 2), 0);      /* a frame 12 px taller than the gadget is no input border */
    CHECK_INT(flat_string_frame(2, 2, 120, 34, 12, 12, 100, 14), 0);     /* 10 px out: a group box, not the input border */
    CHECK_INT(flat_string_frame(2, 2, 300, 120, 12, 12, 100, 14), 0);     /* group box around the gadget */
    CHECK_INT(flat_string_frame(10, 10, 104, 18, 12, 12, 0, 14), 0);      /* empty gadget */
}

/* FLAT-TITLE-GADGETS (S1.3): DOpus's iconify / lock / hijacked zoom are not system gadgets; the band must still end before the
 * leftmost of them. Reachability: the fold below is exactly tx_span (flat.c) over the lister's gadget list. Revert check: a
 * SYSGADGET-only filter leaves x1 at 420. The lock (0x8141) and iconify (0x8041) Activation words are the rig's: a filter that
 * reads 0x10 as TOPBORDER (it is RIGHTBORDER) drops both and the band covers them (x1 397). */
static void test_title_gadget_filter(void)
{
    flat_gadrect rect = { 0, 0, 24, 14, 0, 0, 0 }, low = { 0, 20, 24, 14, 0, 0, 0 };
    flat_gadrect g[6];
    int i, x0, x1, ng;
    unsigned type[6], act[6];
    CHECK_INT(flat_title_gadget(0x0005, 0x40, 0, &rect, 14), 1);          /* GACT_TOPBORDER */
    CHECK_INT(flat_title_gadget(0x0005, 0x40, 1, &rect, 14), 1);          /* GACT_TOPBORDER in a GIMMEZEROZERO window */
    CHECK_INT(flat_title_gadget(0x0005, 0, 0, &rect, 14), 1);             /* geometry branch */
    CHECK_INT(flat_title_gadget(0x0005, 0, 1, &rect, 14), 0);             /* GIMMEZEROZERO: inner-relative */
    CHECK_INT(flat_title_gadget(0x0005, 0x10, 0, &rect, 14), 0);          /* GACT_RIGHTBORDER */
    CHECK_INT(flat_title_gadget(0x0005, 0x20, 0, &rect, 14), 0);          /* GACT_LEFTBORDER */
    CHECK_INT(flat_title_gadget(0x0005, 0x80, 0, &rect, 14), 0);          /* GACT_BOTTOMBORDER */
    CHECK_INT(flat_title_gadget(0x0005, 0, 0, &low, 14), 0);              /* content gadget */
    CHECK_INT(flat_title_gadget(0x8040, 0, 0, &low, 14), 1);              /* system gadget anywhere */
    /* DOpus lister, win_w 444, bar_h 14: close, depth, zoom, iconify, lock, drag (LeftEdges 349/373/398/421) */
    {
        flat_gadrect d[6] = { { 0, 0, 20, 14, 0, 0, 0 }, { -23, 0, 24, 14, 1, 0, 0 }, { -46, 0, 24, 14, 1, 0, 0 },
                              { -71, 0, 24, 14, 1, 0, 0 }, { -95, 0, 24, 14, 1, 0, 0 }, { 0, 0, -20, 14, 0, 1, 1 } };
        unsigned t[6] = { 0x8001, 0x8001, 0x0005, 0x0005, 0x0005, 0x8020 };
        unsigned a[6] = { 0, 0, 0x0001, 0x8041, 0x8141, 0 };            /* zoom, iconify, lock: Activation read on rig 68 */
        for (i = 0; i < 6; i++) { g[i] = d[i]; type[i] = t[i]; act[i] = a[i]; }
    }
    x0 = 0; x1 = 443; ng = 0;
    for (i = 0; i < 6; i++)
        if (flat_title_gadget(type[i], act[i], 0, &g[i], 14)) { flat_title_span_add(&g[i], 444, 14, &x0, &x1); ng++; }
    CHECK_INT(ng, 6);
    CHECK_INT(x0, 20);
    CHECK_INT(x1, 348);
    x0 = 0; x1 = 443;                                                     /* the old filter: system gadgets only */
    for (i = 0; i < 6; i++)
        if (type[i] & 0x8000u) flat_title_span_add(&g[i], 444, 14, &x0, &x1);
    CHECK_INT(x1, 420);
}

static void glyph_state(flat_state *st)
{
    flat_init(st);
    st->enabled = 1;
    st->pen_text = '#';
    st->pen_line = '+';
    st->pen_back = '.';
}

/* FLAT-C4: the kill switches. A table of fake hooks / task names / buttons / marker ages drives the same
 * functions flat.c runs at start. */
static void test_kill_switches(void)
{
    static const struct { const char *name; int want; } tasks[] = {
        { "VisualPrefs", FLAT_HELD_VISUALPREFS }, { "visualprefs handler", FLAT_HELD_VISUALPREFS },
        { "Birdie", FLAT_HELD_BIRDIE }, { "BirdieD", FLAT_HELD_BIRDIE }, { "BIRDIE", FLAT_HELD_BIRDIE },
        { "Workbench", FLAT_HELD_NONE }, { "Visual", FLAT_HELD_NONE }, { "", FLAT_HELD_NONE },
        { "Patina", FLAT_HELD_NONE }, { "bird", FLAT_HELD_NONE },
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
    CHECK_INT(flat_start_decide(2, 0, 0, 0), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(2, 1, 1, FLAT_HELD_BIRDIE), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(0, 1, 1, 0), FLAT_HELD_GESTURE);
    CHECK_INT(flat_start_decide(0, 1, 0, 0), FLAT_HELD_NONE);                                /* one button is not the gesture */
    CHECK_INT(flat_start_decide(0, 0, 1, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(0, 1, 1, FLAT_HELD_HOOK), FLAT_HELD_GESTURE);
    CHECK_INT(flat_start_decide(0, 0, 0, FLAT_HELD_VISUALPREFS), FLAT_HELD_VISUALPREFS);

    /* two-strike crash guard: one early death (count 1) still starts, two in a row hold; a damaged marker is one strike */
    CHECK_INT(flat_start_decide(1, 0, 0, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(flat_marker_parse(0, 0), 0, 0, 0), FLAT_HELD_NONE);                  /* no text read */
    CHECK_INT(flat_start_decide(flat_marker_parse("flat-starting 1\n", 16), 0, 0, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(flat_marker_parse("flat-starting 2\n", 16), 0, 0, 0), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(flat_marker_parse("flat-starting 9\n", 16), 0, 0, 0), FLAT_HELD_MARKER);
    CHECK_INT(flat_start_decide(flat_marker_parse("garbage!garbage!!", 17), 0, 0, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(flat_marker_parse("flat-starting 0\n", 16), 0, 0, 0), FLAT_HELD_NONE);
    CHECK_INT(flat_start_decide(flat_marker_parse("flat-starting ", 14), 0, 0, 0), FLAT_HELD_NONE);
    {
        char mk[24];
        CHECK_INT(flat_marker_format(1, mk), 16);
        CHECK_STR(mk, "flat-starting 1\n");
        flat_marker_format(2, mk);
        CHECK_INT(flat_marker_parse(mk, 16), 2);
        flat_marker_format(3, mk);                                                                  /* capped */
        CHECK_STR(mk, "flat-starting 2\n");
    }

    /* marker -> inert with an injected clock: written at t, alive for FLAT_MARKER_SECS, then gone */
    CHECK_INT(flat_marker_remaining(1000, 1000), FLAT_MARKER_SECS);
    CHECK_INT(flat_marker_remaining(1000, 1059), 1);
    CHECK_INT(flat_marker_remaining(1000, 1060), 0);
    CHECK_INT(flat_marker_remaining(1000, 5000), 0);
    CHECK_INT(flat_marker_remaining(0xFFFFFFF0ul, 5), 60 - 21);                              /* the clock wrapped */

    /* a held block is inert: kill switch beats the switch, and STATUS names the patch */
    flat_init(&s);
    s.enabled = 1;
    s.held = (uint16_t)flat_start_decide(2, 0, 0, 0);
    flat_status_line(&s, sizeof s, 1, line, sizeof line);
    CHECK_STR(line, "flat=held calls=0 chained=0 why=marker");
    CHECK_INT(flat_dopus_decide(&s, FLAT_EL_DRAWBOX, 0, 1, 1, -1, -1), FLAT_DRAW_STOCK);
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
    CHECK_STR(FLAT_MARKER_PATH, "ENVARC:Patina/flat-starting");
}

/* FLAT-C9: the prefs -> flat_state mapping both writers (desk apply, patina-flat BOOT) call. The sentinel proves the function ran;
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
            desk_flat_state(f, g, 1, &s);
            CHECK_INT(desk_flat_state_calls, before + 1);                      /* sentinel: the mapping ran */
            CHECK_INT(s.enabled, f || g);                                     /* either switch enables; both off = 0 */
            CHECK_INT(s.pen_text, t);
            CHECK_INT(s.pen_line, l);
            CHECK_INT(s.pen_back, b);
            CHECK_INT(s.pen_button, DK_SELECTION);                            /* FLAT-BUTTON-FILL */
            CHECK_INT(s.pen_ptext, DK_BG);
            CHECK_INT(s.pen_outline, DK_ACCENT);                              /* the theme accent pen, never 0 */
            CHECK_INT(s.aa, 1);                                               /* ui-antialias on */
            desk_flat_state(f, g, 0, &s);
            CHECK_INT(s.aa, 0);                                               /* ui-antialias off */
            CHECK(s.pen_outline != 0);
            CHECK_INT(s.version, FLAT_VERSION);
            CHECK_INT(s.size, FLAT_STATE_SIZE);
            CHECK_INT(s.held, 2);
            CHECK_INT(s.still_chained, 1);
            CHECK_INT(s.calls, 77);
            CHECK_INT(flat_check(&s, sizeof s), FLAT_OK);
        }
}

/* S32-KNOB-NONE-FRESH: which gadgets scroller_pen treats as prop gadgets (XCON's knob is a propgclass object, type 0x0005). */
static void test_gadget_is_prop(void)
{
    CHECK_INT(flat_gadget_is_prop(0x0003, 0, 1), 1);                    /* Workbench drawer prop */
    CHECK_INT(flat_gadget_is_prop(0x0005, 1, 1), 1);                    /* custom gadget, propgclass or subclass */
    CHECK_INT(flat_gadget_is_prop(0x0005, 0, 1), 0);                    /* custom, other class (strgclass: StringInfo) */
    CHECK_INT(flat_gadget_is_prop(0x0003, 0, 0), 0);                    /* no SpecialInfo */
    CHECK_INT(flat_gadget_is_prop(0x0005, 1, 0), 0);
    CHECK_INT(flat_gadget_is_prop(0x0001, 1, 1), 0);                    /* bool gadget */
}

/* FLAT-BOOT-THEME-FIELDS: desk_flat_full is the one mapping; on the default theme it gives the selection colour the input ground needs */
static long ftheme_none(void *ctx, const char *path, char *buf, long cap)
{
    (void)ctx; (void)path; (void)buf; (void)cap;
    return -1;                                                           /* no theme file: gt_load falls back to the built-in theme */
}
static void test_desk_flat_full(void)
{
    gtheme th;
    flat_state full, parts;
    gt_load(ftheme_none, 0, GT_DEFAULT, &th);
    flat_init(&full);
    flat_init(&parts);
    desk_flat_full(1, 1, 1, &th, &full);
    desk_flat_state(1, 1, 1, &parts);
    desk_flat_inbar(&th, &parts);
    desk_flat_look(&th, &parts);
    CHECK_INT(memcmp(&full, &parts, sizeof full), 0);                    /* the three calls and nothing else */
    CHECK_INT((full.sel_rgb[0] | full.sel_rgb[1] | full.sel_rgb[2]) != 0, 1);   /* a boot resident holds a selection colour, not zero */
}

/* KNOB-PEN: the knob has its own pen from 16 colours, a colour between the surface and the muted text on every theme; at 8 colours
 * or fewer (no free pen) it is DK_MUTED as before. */
static void test_knob_pen(void)
{
    flat_state s;
    flat_init(&s);
    desk_flat_state(1, 1, 1, &s);
    s.pen_outline = 0; s.pen_inbar = 0;                                  /* the knob pen is the fallback below the S86-ACTIVE-SCROLL-GREY inactive grey */
    s.pen_knob = 9;
    CHECK_INT(flat_knob_pen(&s, 4), 9);
    { int dt = s.pen_dimtext; s.pen_dimtext = 0;   /* no inactive grey known: the knob pen */
    CHECK_INT(flat_scroller_pen(&s, 4, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);   /* reachable through the scroller entry */
    CHECK_INT(flat_scroller_pen(&s, 8, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);
      s.pen_dimtext = dt; }
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
    s.enabled = 1; s.pen_back = 0; s.pen_text = 2; s.pen_line = 3; s.pen_button = 4; s.pen_sel = 4; s.pen_dim = 6; s.pen_dimtext = 5;
    flat_gh_frame(&g, &a, &c1, 0, 1, 10, 20, 49, 39);
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
    flat_gh_frame(&g, &a, &c1, 1, 1, 10, 20, 49, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 10, 20, 49, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 0, 1, 60, 20, 99, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 60, 20, 99, 39) != 0, 1);   /* OWNER-2026-11-08: a secondary button is a fill too: its disabled dim applies */
    flat_gh_frame(&g, &a, &c1, 0, 0, 60, 20, 99, 39);
    CHECK_INT(flat_gh_match(&s, &g, &a, &c1, 2, 60, 20, 99, 39) == 0, 1);
    flat_gh_frame(&g, &a, &c1, 0, 1, 10, 20, 49, 39);
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
        {   /* S62-FLAT-RF-TX-BUDGET: the Text hook folds one gadget at a time; the fold equals the array form for every prefix and subset */
            int n, k, f0, f1, a0, a1;
            for (n = 0; n <= 4; n++)
                for (k = 0; k + n <= 4; k++) {
                    f0 = 0; f1 = 199;
                    for (i = k; i < k + n; i++)
                        flat_title_span_add(&g[i], 200, 11, &f0, &f1);
                    flat_title_span(g + k, n, 200, 11, &a0, &a1);
                    CHECK_INT(f0 == a0 && f1 == a1, 1);
                }
        }
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
    FILE *f = fopen("patina/flat.c", "rb");
    CHECK(f != 0);
    if (!f)
        return;
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "\" inbar=\");  put_num(line, &n, copy.pen_inbar)") != 0);
    CHECK(strstr(src, "flat_pens_refresh_on(sl") != 0);                      /* the pen the dump reports is really obtained */
    CHECK(strstr(src, "flat_pens_follow_on(sl") != 0);                       /* and follows the screen on every 1 s watch */
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
    {   /* S57-LOCALE-LIST-NO-SELECT: FILLPEN is the accent; the ROM's side and bottom border fills in it take the surface */
        int o[4];
        s.pen_back = 2;
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 1, 11, 2, 97, o), 2);      /* left side */
        CHECK_INT(o[0], 1); CHECK_INT(o[1], 11); CHECK_INT(o[2], 2); CHECK_INT(o[3], 97);
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 0, 0, 3, 99, o), 2);       /* full-height strip: title rows stay */
        CHECK_INT(o[1], 11); CHECK_INT(o[3], 99);
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 196, 11, 199, 97, o), 2);  /* right side */
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 0, 98, 199, 99, o), 2);    /* bottom */
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 0, 0, 199, 10, o), -1);    /* title band stays the accent */
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 4, 20, 100, 30, o), -1);   /* interior: a selected row */
        CHECK_INT(flat_side_fill_pen(&s, 3, 200, 100, 4, 11, 4, 2, 0, 11, 3, 97, o), -1);     /* another pen */
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 0, 11, 3, 100, o), -1);    /* past the window: untouched */
        s.held = 1;
        CHECK_INT(flat_side_fill_pen(&s, 5, 200, 100, 4, 11, 4, 2, 0, 11, 3, 97, o), -1);
        s.held = 0;
    }
    s.pen_inbar = 9;
    CHECK_INT(flat_inactive_pen(&s, 4), 9);
    CHECK_INT(flat_inactive_pen(&s, 3), -1);
    s.pen_dimtext = 6;
    CHECK_INT(flat_inactive_pen(&s, 3), 6);          /* under 16 colours: the muted pen */
    CHECK_INT(flat_inbar_colour(0x000000UL, 0x646464UL), 0x2d2d2dUL);   /* 45 percent */
    CHECK_INT(flat_scroller_pen(&s, 4, 0, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);   /* inactive knob */
    CHECK_INT(flat_scroller_pen(&s, 4, 1, 10, 20, 16, 100, 0, 1, 0, 0, 0, 0x8000, 10, 20, 25, 69), 9);   /* S86-ACTIVE-SCROLL-GREY: active knob equals inactive */
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

/* S33-PREFS-SKIN-FAIL: the button fill and the knob fallback are a pen flat holds, never a role number a palette load can recolour */
static void test_button_pen_held(void)
{
    wbscr w;
    flat_pens p;
    flat_pen_ops o;
    flat_state s;
    int scr, i, n, live[4], d, bp, kp, sel;
    desk_flat_state(1, 1, 1, &s);
    CHECK_INT(s.pen_button, DK_SELECTION);                          /* the role number Patina writes */
    s.sel_rgb[0] = 0x28; s.sel_rgb[1] = 0x2c; s.sel_rgb[2] = 0x34;
    s.pen_knob = 0; s.pen_dimtext = 0;                              /* the knob falls back to the button pen */
    flat_pens_init(&p);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    flat_pens_refresh(&p, &o, &s, &scr, 1, 8);
    sel = s.pen_sel;
    CHECK_INT(sel >= 100, 1);
    CHECK_INT(w.col[sel], 0x282c34UL);
    n = desk_live_role_pens(8, live);
    CHECK_INT(n > 0, 1);
    bp = flat_button_pen(&s, 8);
    kp = flat_knob_pen(&s, 8);
    CHECK_INT(bp, sel);
    CHECK_INT(kp, sel);
    for (i = 0; i < n; i++) {
        CHECK_INT(bp != live[i], 1);                                /* never a pen a palette load or program recolours */
        CHECK_INT(kp != live[i], 1);
    }
    for (d = 3; d <= 8; d++)
        CHECK_INT(flat_button_pen(&s, d) == (sel < (1 << d) ? sel : s.pen_back), 1);
    CHECK_INT(flat_button_pen(&s, 2), s.pen_line);                  /* 4 colours: the accent line pen, no obtain */
    flat_pens_release(&p, &o, &s, &scr, 1);
    CHECK_INT(s.pen_sel, 0);
    CHECK_INT(flat_button_pen(&s, 8), s.pen_back);                  /* nothing held: the background, never pen 4 */
    CHECK_INT(flat_knob_pen(&s, 8), s.pen_back);
    CHECK_INT(w.bad_releases, 0);
    flat_pens_refresh(&p, &o, &s, &scr, 2, 2);                       /* 4 colours: no selection pen is obtained */
    CHECK_INT(s.pen_sel, 0);
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
    s.sel_rgb[0] = 0x28; s.sel_rgb[1] = 0x2c; s.sel_rgb[2] = 0x34;
    s.pen_dimtext = 6;
    flat_pens_init(&p);
    o.obtain = wb_obtain;
    o.release = wb_release;
    o.rgb = wb_rgb;
    wb_screen(&a, 249);
    o.ctx = &a;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 1);
    CHECK_INT(a.obtains, 4);
    CHECK_INT(s.pen_inbar, 249);
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 0);       /* same screen, same epoch: nothing */
    CHECK_INT(a.obtains, 4);
    /* Workbench reopened as screen B: other clients own 249..251 there, ours would be 240.. */
    wb_screen(&b, 240);
    o.ctx = &b;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_b, 2, 8), 1);
    CHECK_INT(b.releases, 0);                       /* the old pens were never obtained on B */
    CHECK_INT(b.obtains, 4);
    CHECK_INT(b.col[s.pen_inbar], 0x5A5E75UL);      /* flat_inbar_colour of B's pen 0 and pen 2 */
    CHECK_INT(s.pen_inbar, 240);
    CHECK_INT(s.pen_knob != 0 && s.pen_dim != 0, 1);
    /* the same address with a new epoch is a reopened screen too */
    wb_screen(&c, 100);
    o.ctx = &c;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_b, 3, 8), 1);
    CHECK_INT(c.releases, 0);
    CHECK_INT(c.obtains, 4);
    /* Ctrl-D on the identity they were obtained on releases normally; on another identity it only forgets */
    flat_pens_release(&p, &o, &s, &screen_b, 4);
    CHECK_INT(c.releases, 0);
    CHECK_INT(s.pen_inbar, 0);
    flat_pens_refresh(&p, &o, &s, &screen_b, 4, 8);
    flat_pens_release(&p, &o, &s, &screen_b, 4);
    CHECK_INT(c.releases, 4);
    CHECK_INT(c.bad_releases, 0);
    /* the screen going away forgets */
    flat_pens_refresh(&p, &o, &s, &screen_b, 5, 8);
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, 0, 5, 0), 0);
    CHECK_INT(c.bad_releases, 0);
    CHECK_INT(s.pen_dim, 0);
}

/* S57-ICL-PINK-SHADE: Workbench reopened at the SAME screen address before the epoch moved: the held pens read free on the new
 * ColorMap. flat_pens_verify forgets them without ReleasePen; the follow step then re-obtains. */
static int wb_is_free(void *c, int pen)
{
    return !((wbscr *)c)->mine[pen];
}

static void test_pens_verify_same_address_reopen(void)
{
    wbscr a;
    flat_pens p;
    flat_pen_ops o;
    flat_state s;
    int screen_a, rel, held;
    flat_init(&s);
    s.pen_back = 0; s.pen_text = 2; s.pen_button = 5; s.pen_dimtext = 6;
    flat_pens_init(&p);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&a, 249);
    o.ctx = &a;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 1);
    held = a.obtains;
    CHECK(held > 0);
    CHECK_INT(flat_pens_verify(&p, &s, wb_is_free, &a, &screen_a, 1), 0);   /* pens still held: kept */
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 0);
    a.mine[s.pen_dim] = 0;                                                    /* reopen: same address, same epoch, pen free now */
    rel = a.releases;
    CHECK_INT(flat_pens_verify(&p, &s, wb_is_free, &a, &screen_a, 1), 1);
    CHECK_INT(a.releases, rel);                                               /* forgotten, no ReleasePen */
    CHECK_INT(s.pen_dim, 0);
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 8), 1);          /* obtained again */
    CHECK_INT(a.obtains, 2 * held);
    CHECK_INT(a.bad_releases, 0);
}

/* FLAT-PENS-FOLLOW-PALETTE: on a full 16 colour Workbench every pen is shared, so ObtainBestPen only answers the nearest pen and
 * writes nothing. Started before the theme palette is loaded, the selection pen is the nearest stock colour; when the palette load
 * then puts the wanted colour on another pen, the 1 s step must pick again (rig 68: pen 10 = 7f007f kept, pen 12 = 3e4254 unused). */
static int near_obtain(void *c, unsigned long rgb)
{
    wbscr *w = c;
    long best = -1;
    int i, pen = 0;
    for (i = 0; i < 16; i++) {
        long dr = (long)((w->col[i] >> 16) & 255) - (long)((rgb >> 16) & 255);
        long dg = (long)((w->col[i] >> 8) & 255) - (long)((rgb >> 8) & 255);
        long db = (long)(w->col[i] & 255) - (long)(rgb & 255);
        long d = dr * dr + dg * dg + db * db;
        if (best < 0 || d < best) { best = d; pen = i; }
    }
    w->mine[pen]++;
    w->obtains++;
    return pen;
}

static void near_release(void *c, int pen)
{
    wbscr *w = c;
    w->releases++;
    if (w->mine[pen] <= 0)
        w->bad_releases++;
    else
        w->mine[pen]--;
}

static void test_pens_follow_palette_load(void)
{
    wbscr a;
    flat_pens p;
    flat_pen_ops o;
    flat_state s;
    int screen_a, i, obt, held = 0;
    flat_init(&s);
    s.pen_back = 0; s.pen_text = 2; s.pen_button = 4; s.pen_dimtext = 6;
    s.sel_rgb[0] = 0x3e; s.sel_rgb[1] = 0x42; s.sel_rgb[2] = 0x54;
    flat_pens_init(&p);
    memset(&a, 0, sizeof a);
    for (i = 0; i < 16; i++)
        a.col[i] = 0xff0000UL;                                               /* stock palette: nothing near the theme */
    a.col[0] = 0x1a1b26UL; a.col[2] = 0xa9b1d6UL; a.col[4] = 0x00ff9fUL; a.col[10] = 0x7f007fUL;
    o.obtain = near_obtain; o.release = near_release; o.rgb = wb_rgb; o.ctx = &a;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 4), 1);
    CHECK(s.pen_sel != 12);                                                   /* the start before the palette load */
    obt = a.obtains;
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 4), 0);          /* palette unchanged: nothing, no churn */
    CHECK_INT(a.obtains, obt);
    a.col[12] = 0x3e4254UL;                                                   /* IPrefs loads the theme palette */
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 4), 1);          /* same screen, same epoch: picked again */
    CHECK_INT(s.pen_sel, 12);
    CHECK_INT(flat_button_pen(&s, 4), 12);                                    /* and the faces follow */
    CHECK_INT((int)a.col[flat_button_pen(&s, 4)], 0x3e4254);
    CHECK_INT(a.bad_releases, 0);
    for (i = 0; i < 16; i++)
        held += a.mine[i];
    CHECK_INT(held, a.obtains - a.releases);                                  /* every dropped pen was released on its screen */
    CHECK_INT(flat_pens_follow(&p, &o, &s, 1, &screen_a, 1, 4), 0);          /* settled */
    CHECK_INT(flat_pens_palsig(&o, 0), 0);
}
/* S32-CLONE-CLIP: the private depth image as rig 23 (3.2, intuition 47.53) drew it with the MARKER pens, dumped 32 wide with
 * the draw origin at column 4; LeftEdge is -1, so image column 0 is dump column 3 (s32-evidence/clone-clip-hunt.md).
 * Marker 5 = SHADOW (right bevel at image column 23), 8 = BACKGROUND (inside the back window). */
#define CLIP_PAD 4

/* S32-START-LOOK: started before LoadWB, the first install finds no class; the 1 s watch retries it once the screen is up */
static void test_boot_before_workbench_retries_install(void)
{
    CHECK_INT(flat_install_due(0, 0, FLAT_ERR_NOCLASS), 1);
    CHECK_INT(flat_install_due(0, 1, FLAT_OK), 0);                 /* installed: never twice */
    CHECK_INT(flat_install_due(FLAT_HELD_MARKER, 0, FLAT_ERR_NOCLASS), 0);   /* a kill switch stays inert */
    CHECK_INT(flat_install_due(0, 0, FLAT_ERR_DOUBLE), 0);         /* a real swap failure is final */
    CHECK_INT(flat_install_due(0, 0, -101), 0);                    /* out of memory is final */
}

/* DOPUS-LATE-START: boot starts patina-flat before DOpus opens dopus5.library; the 1 s watch rescans until it is found */
static void test_dopus_late_start_rescans(void)
{
    CHECK_INT(flat_dopus_rescan_due(0, 0, 0), 1);                  /* library absent at the last scan */
    CHECK_INT(flat_dopus_rescan_due(0, 1, 0), 0);                  /* found (even if the gate refused): final */
    CHECK_INT(flat_dopus_rescan_due(0, 0, 1), 0);                  /* already hooked */
    CHECK_INT(flat_dopus_rescan_due(FLAT_HELD_MARKER, 0, 0), 0);   /* a kill switch stays inert */
}

/* S32-GHOST-MENU: the 3.2 menu ghost (ROM caller, no Window layer, AreaPtSz 1, {0x4444, 0x1111}) takes the dim label pen; every
 * other fill, a Window layer, another pattern, a held or off block, a screen under 8 colours or a dim pen equal to the fill: stock. */
static void test_ghost_menu_pen(void)
{
    flat_state s;
    flat_init(&s);
    desk_flat_state(1, 1, 1, &s);
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

/* ---- TE-10.6a: pens per screen slot ---- */
typedef char flat_slots_equal_ip_screens[(FLAT_PENS_SLOTS == IP_SCREENS) ? 1 : -1];

static int tab_is_open(void *ctx, const void *screen, unsigned epoch)
{
    (void)epoch;
    return screen == ctx;
}

static void tab_obtain_on(flat_pens_tab *t, const flat_pen_ops *o, const flat_state *look, const void *scr, unsigned epoch)
{
    flat_pens *p = flat_pens_tab_for(t, scr, epoch);
    if (p && p->screen != scr)
        flat_pens_refresh_on(p, o, look, 0, scr, epoch, 8);
}

static int wb_is_free_none(void *ctx, int pen)
{
    (void)ctx; (void)pen;
    return 0;
}

static void test_pens_tab_never_cross_screens(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state s;
    int scr_a, scr_b;
    const flat_pens *pa, *pb;
    desk_flat_state(1, 1, 1, &s);
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(&t, &o, &s, &scr_a, 1);
    tab_obtain_on(&t, &o, &s, &scr_b, 1);
    pa = flat_pens_tab_find(&t, &scr_a, 1);
    pb = flat_pens_tab_find(&t, &scr_b, 1);
    CHECK(pa != 0 && pb != 0 && pa != pb);
    CHECK(pa->dim != pb->dim);                                       /* A's pens are never returned for B */
    CHECK(flat_pens_tab_find(&t, &w, 1) == 0);                        /* an unknown screen holds nothing */
}

static void test_pens_tab_switch_releases_nothing(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state s;
    int scr_a, scr_b, obtains, dim_a;
    desk_flat_state(1, 1, 1, &s);
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(&t, &o, &s, &scr_a, 1);
    dim_a = flat_pens_tab_find(&t, &scr_a, 1)->dim;
    obtains = w.obtains;
    CHECK(obtains > 0);
    tab_obtain_on(&t, &o, &s, &scr_b, 1);                            /* a window on B */
    tab_obtain_on(&t, &o, &s, &scr_a, 1);                            /* and back on A */
    CHECK_INT(w.releases, 0);
    CHECK_INT(w.obtains, 2 * obtains);                               /* B obtained once, A not again */
    CHECK_INT(flat_pens_tab_find(&t, &scr_a, 1)->dim, dim_a);
}

static void test_pens_tab_new_epoch_not_old_pens(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state s;
    int scr_a;
    desk_flat_state(1, 1, 1, &s);
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(&t, &o, &s, &scr_a, 1);
    CHECK(flat_pens_tab_find(&t, &scr_a, 2) == 0);                   /* same address, reopened: the old pens are not handed out */
    CHECK(flat_pens_tab_for(&t, &scr_a, 2) != flat_pens_tab_find(&t, &scr_a, 1));
    CHECK_INT(w.releases, 0);                                        /* nothing released for the old identity */
    CHECK(flat_pens_tab_find(&t, &scr_a, 1) != 0);                   /* and it stays until the reap */
}

static void test_pens_tab_full_obtains_nothing(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state s;
    static int scr[FLAT_PENS_SLOTS + 1];
    int i, obtains;
    desk_flat_state(1, 1, 1, &s);
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    for (i = 0; i < FLAT_PENS_SLOTS; i++) {
        flat_pens *p = flat_pens_tab_for(&t, &scr[i], 1);
        CHECK(p != 0);
        p->screen = &scr[i];
        p->epoch = 1;
    }
    obtains = w.obtains;
    CHECK(flat_pens_tab_for(&t, &scr[FLAT_PENS_SLOTS], 1) == 0);
    tab_obtain_on(&t, &o, &s, &scr[FLAT_PENS_SLOTS], 1);
    CHECK_INT(w.obtains, obtains);                                   /* a full table obtains nothing */
    CHECK(flat_pens_tab_for(&t, &scr[3], 1) != 0);                   /* a held identity is still served */
    CHECK(flat_pens_tab_for(&t, 0, 0) == 0);
}

static void test_pens_tab_reap_forgets_closed_only(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state s;
    int scr_a, scr_b, scr_c;
    desk_flat_state(1, 1, 1, &s);
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(&t, &o, &s, &scr_a, 1);
    tab_obtain_on(&t, &o, &s, &scr_b, 1);
    CHECK_INT(flat_pens_tab_reap(&t, tab_is_open, &scr_a), 1);        /* B closed */
    CHECK_INT(w.releases, 0);
    CHECK(flat_pens_tab_find(&t, &scr_a, 1) != 0);
    CHECK(flat_pens_tab_find(&t, &scr_b, 1) == 0);
    CHECK_INT(flat_pens_tab_reap(&t, tab_is_open, &scr_a), 0);
    tab_obtain_on(&t, &o, &s, &scr_c, 1);                            /* the freed slot is reusable */
    CHECK(flat_pens_tab_find(&t, &scr_c, 1) != 0);
    CHECK_INT(w.releases, 0);
}

static void test_pens_non_workbench_slot_keeps_flat_state(void)
{
    wbscr w;
    flat_pens p;
    flat_pen_ops o;
    flat_state s;
    int scr;
    desk_flat_state(1, 1, 1, &s);
    s.sel_rgb[0] = 0x28; s.sel_rgb[1] = 0x2c; s.sel_rgb[2] = 0x34;
    s.pen_dim = 11; s.pen_knob = 12; s.pen_inbar = 13; s.pen_sel = 14;     /* the Workbench's numbers */
    flat_pens_init(&p);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    flat_pens_refresh_on(&p, &o, &s, 0, &scr, 1, 8);
    CHECK(p.dim >= 100);
    CHECK_INT(s.pen_dim, 11);
    CHECK_INT(s.pen_knob, 12);
    CHECK_INT(s.pen_inbar, 13);
    CHECK_INT(s.pen_sel, 14);
    CHECK_INT(flat_pens_follow_on(&p, &o, &s, 0, 1, &scr, 1, 8), 0);   /* same identity: nothing */
    flat_pens_release(&p, &o, 0, &scr, 1);
    CHECK_INT(w.releases > 0, 1);
    CHECK_INT(s.pen_dim, 11);                                        /* release with no store leaves them too */
    flat_pens_refresh_on(&p, &o, &s, 0, &scr, 2, 8);
    CHECK_INT(flat_pens_verify(&p, 0, wb_is_free_none, 0, &scr, 2), 0);
    CHECK_INT(s.pen_sel, 14);
    /* the Workbench path still stores */
    flat_pens_refresh(&p, &o, &s, &scr, 3, 8);
    CHECK(s.pen_dim >= 100);
}

/* TE-10.6b: what the draw hook reads for a window: the global look plus the pens of the window's own screen slot */
static void test_state_for_screen_pens(void)
{
    wbscr w;
    flat_pens_tab t;
    flat_pen_ops o;
    flat_state look, a, b, c;
    int scr_a, scr_b, scr_c;
    desk_flat_state(1, 1, 1, &look);
    look.sel_rgb[0] = 0x28; look.sel_rgb[1] = 0x2c; look.sel_rgb[2] = 0x34;
    look.pen_dim = 77; look.pen_knob = 78; look.pen_inbar = 79; look.pen_sel = 80;   /* stale numbers in the look block */
    flat_pens_tab_init(&t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(&t, &o, &look, &scr_a, 1);
    tab_obtain_on(&t, &o, &look, &scr_b, 1);
    CHECK_INT(flat_state_for(&look, &t, &scr_a, 1, &a), 1);
    CHECK_INT(flat_state_for(&look, &t, &scr_b, 1, &b), 1);
    CHECK(a.pen_dim >= 100 && b.pen_dim >= 100 && a.pen_dim != b.pen_dim);           /* B's pens are never A's */
    CHECK_INT(a.pen_dim, flat_pens_tab_find(&t, &scr_a, 1)->dim);
    CHECK_INT(b.pen_sel, flat_pens_tab_find(&t, &scr_b, 1)->sel);
    CHECK_INT(a.pen_back, look.pen_back);                                           /* the look stays global */
    CHECK_INT(memcmp(a.sel_rgb, look.sel_rgb, 3), 0);
    CHECK_INT(flat_state_for(&look, &t, &scr_c, 1, &c), 0);                          /* no slot: no pens at all */
    CHECK_INT(c.pen_dim, 0); CHECK_INT(c.pen_knob, 0); CHECK_INT(c.pen_inbar, 0); CHECK_INT(c.pen_sel, 0);
    CHECK_INT(c.pen_back, look.pen_back);
    CHECK_INT(flat_state_for(&look, &t, &scr_a, 2, &c), 0);                          /* another epoch of A: the old pens are gone */
    CHECK_INT(c.pen_dim, 0);
}

/* TE-10.6b budget fix: the draw table the main task keeps and the hooks only look up (no copy, no iconpen call in a hook) */
static flat_draw_tab g_dt;

static void draw_setup(flat_state *look, flat_pens_tab *t, int *sa, int *sb)
{
    wbscr w;
    flat_pen_ops o;
    desk_flat_state(1, 1, 1, look);
    look->pen_dim = 77; look->pen_knob = 78; look->pen_inbar = 79; look->pen_sel = 80;
    flat_pens_tab_init(t);
    o.obtain = wb_obtain; o.release = wb_release; o.rgb = wb_rgb;
    wb_screen(&w, 100);
    o.ctx = &w;
    tab_obtain_on(t, &o, look, sa, 1);
    (void)sb;
}

static void test_draw_state_lookup(void)
{
    flat_state look, live, st;
    flat_pens_tab t;
    flat_draw_scr l[2];
    int wb, sa, sb, un;
    const flat_state *p;
    draw_setup(&look, &t, &sa, &sb);
    live = look; live.pen_dim = 5;
    flat_draw_init(&g_dt);
    l[0].screen = &sa; l[0].epoch = 1;
    l[1].screen = &sb; l[1].epoch = 1;                                   /* listed, no pens slot */
    flat_draw_sync(&g_dt, &look, &t, l, 2, &wb, 9);
    CHECK(flat_draw_state(&g_dt, &live, 0) == &live);                   /* 1: no screen = live */
    CHECK(flat_draw_state(&g_dt, &live, &wb) == &live);                 /* 2: the Workbench = live */
    p = flat_draw_state(&g_dt, &live, &sa);                              /* 3 */
    flat_state_for(&look, &t, &sa, 1, &st);
    CHECK(p != &live && p->pen_dim == st.pen_dim && p->pen_dim >= 100);
    CHECK_INT(p->pen_knob, st.pen_knob); CHECK_INT(p->pen_inbar, st.pen_inbar); CHECK_INT(p->pen_sel, st.pen_sel);
    CHECK_INT(p->pen_back, look.pen_back); CHECK_INT(memcmp(p->sel_rgb, look.sel_rgb, 3), 0);
    p = flat_draw_state(&g_dt, &live, &sb);                              /* 4 */
    CHECK(p != &live);
    CHECK_INT(p->pen_dim, 0); CHECK_INT(p->pen_knob, 0); CHECK_INT(p->pen_inbar, 0); CHECK_INT(p->pen_sel, 0);
    CHECK_INT(p->pen_back, look.pen_back);
    CHECK(flat_draw_state(&g_dt, &live, &un) == &g_dt.plain);            /* 5 */
    CHECK_INT(g_dt.plain.pen_dim, 0); CHECK_INT(g_dt.plain.pen_back, look.pen_back);
}

static void test_draw_sync_look_and_drop(void)
{
    flat_state look, live;
    flat_pens_tab t;
    flat_draw_scr l[2];
    int wb, sa, sb, un;
    draw_setup(&look, &t, &sa, &sb);
    live = look;
    flat_draw_init(&g_dt);
    l[0].screen = &sa; l[0].epoch = 1;
    l[1].screen = &sb; l[1].epoch = 1;
    flat_draw_sync(&g_dt, &look, &t, l, 2, &wb, 9);
    look.pen_back = (uint8_t)(look.pen_back + 3);                        /* 6: a look change reaches every slot and plain */
    flat_draw_sync(&g_dt, &look, &t, l, 2, &wb, 9);
    CHECK_INT(flat_draw_state(&g_dt, &live, &sa)->pen_back, look.pen_back);
    CHECK_INT(flat_draw_state(&g_dt, &live, &sb)->pen_back, look.pen_back);
    CHECK_INT(flat_draw_state(&g_dt, &live, &un)->pen_back, look.pen_back);
    flat_draw_sync(&g_dt, &look, &t, &l[1], 1, &wb, 9);                  /* 7: A no longer listed reads plain */
    CHECK(flat_draw_state(&g_dt, &live, &sa) == &g_dt.plain);
    CHECK(flat_draw_state(&g_dt, &live, &sb) != &g_dt.plain);
}

static void test_draw_slot_keeps_address(void)
{
    flat_state look, live;
    flat_pens_tab t;
    flat_draw_scr l[3];
    int wb, sa, sb, sc, i;
    const flat_state *pa, *pb;
    draw_setup(&look, &t, &sa, &sb);
    live = look;
    flat_draw_init(&g_dt);
    l[0].screen = &sa; l[0].epoch = 1;
    l[1].screen = &sb; l[1].epoch = 1;
    flat_draw_sync(&g_dt, &look, &t, l, 2, &wb, 9);
    pa = flat_draw_state(&g_dt, &live, &sa);
    pb = flat_draw_state(&g_dt, &live, &sb);
    l[0].screen = &sc; l[0].epoch = 1;                                   /* 8: B and C come and go around A */
    l[1].screen = &sa; l[1].epoch = 1;
    flat_draw_sync(&g_dt, &look, &t, l, 2, &wb, 9);
    CHECK(flat_draw_state(&g_dt, &live, &sa) == pa);
    CHECK(flat_draw_state(&g_dt, &live, &sb) == &g_dt.plain);
    CHECK(flat_draw_state(&g_dt, &live, &sc) != &g_dt.plain);
    for (i = 0; i < 3; i++) {
        l[0].screen = &sb; l[0].epoch = 1;
        l[1].screen = &sa; l[1].epoch = 1;
        l[2].screen = &sc; l[2].epoch = 1;
        flat_draw_sync(&g_dt, &look, &t, l, 3, &wb, 9);
        CHECK(flat_draw_state(&g_dt, &live, &sa) == pa);
    }
    (void)pb;
}

static void test_draw_epoch(void)
{
    flat_state look;
    flat_pens_tab t;
    flat_draw_scr l[1];
    int wb, sa, sb, un;
    draw_setup(&look, &t, &sa, &sb);
    flat_draw_init(&g_dt);
    l[0].screen = &sa; l[0].epoch = 4;
    flat_draw_sync(&g_dt, &look, &t, l, 1, &wb, 9);
    CHECK_INT(flat_draw_epoch(&g_dt, &wb), 9);                           /* 9 */
    CHECK_INT(flat_draw_epoch(&g_dt, &sa), 4);
    CHECK_INT(flat_draw_epoch(&g_dt, &un), 0);
    CHECK_INT(flat_draw_epoch(&g_dt, 0), 0);
}

/* S74-TIME-ROW: one CreateGadgetA event */
static flat_trow_ev trow_ev(int kind, int l, int t, int w, int h, int id)
{
    flat_trow_ev e;
    e.kind = kind; e.left = l; e.top = t; e.width = w; e.height = h; e.id = id;
    return e;
}

/* Feed the time 39.1 buttons Save / Use / Cancel (window coords of the measured layout, window at 0,14: Save 12,198 87x14 on
 * screen); returns how many calls answered "changed" (must be 0). */
static int trow_buttons(flat_trow_tab *t, const void *task, const char *prog, int top, int h)
{
    flat_trow_ev e;
    int nl = -1, nw = -1, n = 0;
    e = trow_ev(FLAT_GT_BUTTON, 12, top, 87, h, 2);
    n += flat_trow_feed(t, task, prog, 1, 0, &e, &nl, &nw);
    e = trow_ev(FLAT_GT_BUTTON, 122, top, 87, h, 8);
    n += flat_trow_feed(t, task, prog, 1, 0, &e, &nl, &nw);
    e = trow_ev(FLAT_GT_BUTTON, 233, top, 87, h, 9);
    n += flat_trow_feed(t, task, prog, 1, 0, &e, &nl, &nw);
    return n;
}

/* The visible ground columns between two boxes side by side after FLAT-SPACING (flat_inset) */
static int trow_ground(int side, int al, int at, int ar, int ab, int bl, int bt, int br, int bb)
{
    int a[4], b[4];
    flat_inset(side, al, at, ar, ab, a);
    flat_inset(side, bl, bt, br, bb, b);
    return b[0] - a[2] - 1;
}

static void test_trow_name(void)
{
    CHECK_INT(flat_trow_name("Time"), 1);
    CHECK_INT(!strcmp(flat_mui_map("Cycle.mui"), "UP-Cycle.mcc"), 1);   /* MUI-FLAT */
    CHECK_INT(flat_mui_map("cycle.mui") == 0 && flat_mui_map("Group.mui") == 0 && flat_mui_map(0) == 0, 1);
    CHECK_INT(flat_trow_name("time"), 1);
    CHECK_INT(flat_trow_name("SYS:Prefs/TIME"), 1);
    CHECK_INT(flat_trow_name("Work:Time"), 1);
    CHECK_INT(flat_trow_name("Timer"), 0);
    CHECK_INT(flat_trow_name("Time.info"), 0);
    CHECK_INT(flat_trow_name("SYS:Time/Clock"), 0);
    CHECK_INT(flat_trow_name("Tim"), 0);
    CHECK_INT(flat_trow_name(""), 0);
    CHECK_INT(flat_trow_name(0), 0);
}

/* The measured time 39.1 layout: row left = Save left 12; year 54..106 (frame box, NewGadget assumed to carry it, frame_out 0) lands
 * at 12 keeping width 53; month 142..319 grows left to 65, width 255, right edge 319 unchanged; ground between = 2 * side. */
static void test_trow_time39(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, nl = -1, nw = -1, r;
    flat_trow_init(&t);
    CHECK_INT(trow_buttons(&t, &a, "SYS:Prefs/Time", 184, 14), 0);
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    r = flat_trow_feed(&t, &a, "SYS:Prefs/Time", 1, 0, &e, &nl, &nw);
    CHECK_INT(r, 1);
    CHECK_INT(nl, 12);                                                   /* year box left = row left (Save) */
    CHECK_INT(nw, 53);                                                   /* keeps its width */
    nl = nw = -1;
    e = trow_ev(FLAT_GT_CYCLE, 142, 18, 178, 14, 15);
    r = flat_trow_feed(&t, &a, "SYS:Prefs/Time", 1, 0, &e, &nl, &nw);
    CHECK_INT(r, 1);
    CHECK_INT(nl, 65);
    CHECK_INT(nw, 255);
    CHECK_INT(nl + nw - 1, 142 + 178 - 1);                               /* month right edge stays */
    CHECK_INT(trow_ground(1, 12, 18, 64, 31, nl, 18, nl + nw - 1, 31), 2);   /* FLAT-SPACING gap, square pixels */
    CHECK_INT(t.rows, 1);
    CHECK_INT(t.why, FLAT_TROW_OK);
    CHECK_INT(t.slot[0].task == 0 && t.slot[1].task == 0, 1);           /* sequence ended: slot freed */
    e = trow_ev(11, 126, 140, 194, 11, 13);         /* the sliders after: unchanged */
    CHECK_INT(flat_trow_feed(&t, &a, "SYS:Prefs/Time", 1, 0, &e, &nl, &nw), 0);
}

/* Same layout if the NewGadget carries the inner string box (60,21 41x8) and GadTools draws the frame 6 columns outside: the frame
 * lands at 12 (box at 18), the month the same as above. */
static void test_trow_inner_box(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, nl = -1, nw = -1;
    flat_trow_init(&t);
    trow_buttons(&t, &a, "Time", 184, 14);
    e = trow_ev(FLAT_GT_INTEGER, 60, 21, 41, 8, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 6, &e, &nl, &nw), 1);
    CHECK_INT(nl, 18);
    CHECK_INT(nl - 6, 12);
    CHECK_INT(nw, 41);
    e = trow_ev(FLAT_GT_CYCLE, 142, 18, 178, 14, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 6, &e, &nl, &nw), 1);
    CHECK_INT(nl, 65);
    CHECK_INT(nw, 255);
}

/* Non-square pixels (side 2, PAL hires): the program boxes still touch, the ground between is 4 columns = 2 * side; a row too small
 * for flat_inset gets the gap as program space instead. */
static void test_trow_aspect(void)
{
    int nl = -1, nw = -1;
    CHECK_INT(flat_trow_month(2, 0, 12, 53, 18, 14, 106, 142, 18, 178, 14, &nl, &nw), 1);
    CHECK_INT(nl, 65);
    CHECK_INT(nw, 255);
    CHECK_INT(trow_ground(2, 12, 18, 64, 31, nl, 18, nl + nw - 1, 31), 4);
    CHECK_INT(flat_trow_month(1, 0, 12, 53, 18, 6, 106, 142, 18, 178, 6, &nl, &nw), 1);   /* 6 rows: no inset on either box */
    CHECK_INT(nl, 67);
    CHECK_INT(nl - 64 - 1, 2);
    CHECK_INT(nl + nw - 1, 319);
    CHECK_INT(flat_trow_month(2, 0, 12, 53, 18, 6, 106, 142, 18, 178, 6, &nl, &nw), 1);
    CHECK_INT(nl - 64 - 1, 4);
}

/* A larger font: every size from the NewGadget values, no font constant (year 70,24 69x20, month 186,24 240x20). */
static void test_trow_big_font(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, nl = -1, nw = -1;
    flat_trow_init(&t);
    trow_buttons(&t, &a, "Time", 250, 20);
    e = trow_ev(FLAT_GT_INTEGER, 70, 24, 69, 20, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);
    CHECK_INT(nl, 12);
    CHECK_INT(nw, 69);
    e = trow_ev(FLAT_GT_CYCLE, 186, 24, 240, 20, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);
    CHECK_INT(nl, 81);
    CHECK_INT(nw, 345);
    CHECK_INT(nl + nw - 1, 425);
    CHECK_INT(trow_ground(1, 12, 24, 80, 43, nl, 24, 425, 43), 2);
}

/* Every mismatch: the gadget stays stock and the reason is kept for STATUS. */
static void test_trow_mismatch(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, nl = -1, nw = -1;
    flat_trow_init(&t);
    trow_buttons(&t, &a, "Clock", 184, 14);                                  /* another program: no slot, never changed */
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Clock", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.slot[0].task == 0, 1);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);         /* INTEGER before any BUTTON */
    CHECK_INT(t.why, FLAT_TROW_NOBUTTON);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* another kind before the INTEGER */
    e = trow_ev(12, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ORDER);
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);         /* the sequence restarted: no BUTTON since */
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* a program named Time with other ids */
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 3);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ID);
    trow_buttons(&t, &a, "Time", 18, 14);                                    /* a BUTTON on the INTEGER's row */
    e = trow_ev(FLAT_GT_INTEGER, 154, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ORDER);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* INTEGER already at the row left */
    e = trow_ev(FLAT_GT_INTEGER, 12, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_GEOM);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* degenerate width */
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 0, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_GEOM);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* INTEGER moved, next is no CYCLE */
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);         /* decided when it arrives: it moves */
    nl = nw = -1;
    e = trow_ev(11, 142, 18, 178, 14, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(nl == -1 && nw == -1, 1);
    CHECK_INT(t.why, FLAT_TROW_NOCYCLE);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* CYCLE on another row */
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw);
    e = trow_ev(FLAT_GT_CYCLE, 142, 60, 178, 14, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ROW);
    trow_buttons(&t, &a, "Time", 184, 14);                                   /* CYCLE left of the INTEGER */
    e = trow_ev(FLAT_GT_INTEGER, 154, 18, 53, 14, 14);
    flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw);
    e = trow_ev(FLAT_GT_CYCLE, 100, 18, 40, 14, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ROW);
    CHECK_INT(flat_trow_month(1, 0, 12, 53, 18, 14, 106, 142, 18, -5, 14, &nl, &nw), 0);   /* negative month width */
    CHECK_INT(flat_trow_month(1, 0, 12, 53, 18, 14, 106, 107, 18, 178, 14, &nl, &nw), 1);  /* touching: grows */
    CHECK_INT(flat_trow_month(1, 0, 60, 53, 18, 14, 106, 107, 18, 178, 14, &nl, &nw), 0);  /* would not grow */
    CHECK_INT(flat_trow_year(-1, 0, 54, 53, &nl), 0);
    CHECK_INT(t.rows, 0);
}

/* Two Time tasks creating at once keep independent states; a dropped task forgets its sequence. */
/* The exact call order rig 68 traced from Time 39.1 (depth.kind.id): gadtools builds each kind by calling CreateGadgetA again through
 * its own vector, so every program gadget is followed by nested GENERIC (and, for the INTEGER, STRING) calls of the same id:
 * 0.0.0 0.1.2 1.0.2 0.1.8 1.0.8 0.1.9 1.0.9 0.3.14 1.12.14 2.0.14 0.7.15 1.0.15 0.11.13 1.0.13 1.0.13 0.11.12 1.0.12 1.0.12.
 * Nested calls carry the box gadtools passes down (the moved one after a change). Only the INTEGER and CYCLE change. */
static void test_trow_nested_trace(void)
{
    static const int seq[18][6] = {
        { 0, 0, 0, 0, 0, 0 },                                               /* CreateContext */
        { 1, 12, 184, 87, 14, 2 }, { 0, 12, 184, 87, 14, 2 },
        { 1, 122, 184, 87, 14, 8 }, { 0, 122, 184, 87, 14, 8 },
        { 1, 233, 184, 87, 14, 9 }, { 0, 233, 184, 87, 14, 9 },
        { 3, 54, 18, 53, 14, 14 }, { 12, 12, 18, 53, 14, 14 }, { 0, 12, 18, 53, 14, 14 },
        { 7, 142, 18, 178, 14, 15 }, { 0, 65, 18, 255, 14, 15 },
        { 11, 126, 140, 194, 11, 13 }, { 0, 126, 140, 194, 11, 13 }, { 0, 126, 140, 194, 11, 13 },
        { 11, 126, 160, 194, 11, 12 }, { 0, 126, 160, 194, 11, 12 }, { 0, 126, 160, 194, 11, 12 }
    };
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, i, r, nl, nw, changed = 0;
    flat_trow_init(&t);
    for (i = 0; i < 18; i++) {
        nl = nw = -1;
        e = trow_ev(seq[i][0], seq[i][1], seq[i][2], seq[i][3], seq[i][4], seq[i][5]);
        r = flat_trow_feed(&t, &a, "SYS:Prefs/Time", 1, 0, &e, &nl, &nw);
        changed += r;
        if (i == 7) {
            CHECK_INT(r, 1);
            CHECK_INT(nl, 12);
            CHECK_INT(nw, 53);
        } else if (i == 10) {
            CHECK_INT(r, 1);
            CHECK_INT(nl, 65);
            CHECK_INT(nw, 255);
        } else {
            CHECK_INT(r, 0);
            CHECK_INT(nl, -1);                                               /* untouched: nested values stay as gadtools made them */
        }
        if (i == 9)
            CHECK_INT(t.slot[0].phase == 1 || t.slot[1].phase == 1 || t.slot[2].phase == 1 || t.slot[3].phase == 1, 1);
    }
    CHECK_INT(changed, 2);
    CHECK_INT(t.rows, 1);
    CHECK_INT(t.why, FLAT_TROW_OK);                                          /* not overwritten by the nested calls after the row */
}

/* A nested-looking call neither starts nor ends a sequence: GENERIC or STRING id 14 alone take no slot and leave the reason; a
 * GENERIC between the buttons and the INTEGER does not end the sequence. */
static void test_trow_nested_inert(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, nl = -1, nw = -1;
    flat_trow_init(&t);
    t.why = FLAT_TROW_ROW;
    e = trow_ev(FLAT_GT_GENERIC, 12, 184, 87, 14, 2);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    e = trow_ev(FLAT_GT_STRING, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.slot[0].task == 0 && t.slot[1].task == 0 && t.slot[2].task == 0 && t.slot[3].task == 0, 1);
    CHECK_INT(t.why, FLAT_TROW_ROW);
    CHECK_INT(t.clock, 0);
    trow_buttons(&t, &a, "Time", 184, 14);
    e = trow_ev(FLAT_GT_GENERIC, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
    CHECK_INT(t.why, FLAT_TROW_ROW);
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);
    CHECK_INT(nl, 12);
}

static void test_trow_tasks(void)
{
    static flat_trow_tab t;
    flat_trow_ev e;
    int a = 0, b = 0, nl = -1, nw = -1;
    flat_trow_init(&t);
    trow_buttons(&t, &a, "Time", 184, 14);
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &b, "Time", 1, 0, &e, &nl, &nw), 0);        /* b has no BUTTON yet */
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);
    trow_buttons(&t, &b, "Time", 250, 20);
    e = trow_ev(FLAT_GT_INTEGER, 70, 24, 69, 20, 14);
    CHECK_INT(flat_trow_feed(&t, &b, "Time", 1, 0, &e, &nl, &nw), 1);
    e = trow_ev(FLAT_GT_CYCLE, 142, 18, 178, 14, 15);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 1);
    CHECK_INT(nl == 65 && nw == 255, 1);
    e = trow_ev(FLAT_GT_CYCLE, 186, 24, 240, 20, 15);
    CHECK_INT(flat_trow_feed(&t, &b, "Time", 1, 0, &e, &nl, &nw), 1);
    CHECK_INT(nl == 81 && nw == 345, 1);
    CHECK_INT(t.rows, 2);
    trow_buttons(&t, &a, "Time", 184, 14);
    flat_trow_drop(&t, &a);
    e = trow_ev(FLAT_GT_INTEGER, 54, 18, 53, 14, 14);
    CHECK_INT(flat_trow_feed(&t, &a, "Time", 1, 0, &e, &nl, &nw), 0);
}

void suite_flat_core(void)
{
    test_draw_state_lookup();
    test_draw_sync_look_and_drop();
    test_draw_slot_keeps_address();
    test_draw_epoch();
    test_state_for_screen_pens();
    test_pens_tab_never_cross_screens();
    test_pens_tab_switch_releases_nothing();
    test_pens_tab_new_epoch_not_old_pens();
    test_pens_tab_full_obtains_nothing();
    test_pens_tab_reap_forgets_closed_only();
    test_pens_non_workbench_slot_keeps_flat_state();
    test_ghost_menu_pen();
    test_ghost_menu_recolour();
    test_frame_pen();
    test_inactive_pens();
    test_inbar_status_dump();
    test_stale_held_pens();
    test_pens_verify_same_address_reopen();
    test_pens_follow_palette_load();
    test_button_pen_held();
    test_boot_before_workbench_retries_install();
    test_dopus_late_start_rescans();
    test_title_accent();
    test_gadget_is_prop();
    test_knob_pen();
    test_desk_flat_state();
    test_kill_switches();
    test_layout();
    test_init_check();
    test_status();
    test_exit();
    test_swap();
    test_tramp();
    test_dopus_gate();
    test_disabled_button_dim_fill();
    test_dopus_body_hook();
    test_dopus_bevel_hook();
    test_dopus_body_swap();
    test_dopus_swap();
    test_dopus_decide();
    test_input_ground();
    test_menu_panel();
    test_desk_flat_full();
    test_title_gadget_filter();
    test_dopus_path();
    test_trow_name();
    test_trow_time39();
    test_trow_inner_box();
    test_trow_aspect();
    test_trow_big_font();
    test_trow_mismatch();
    test_trow_tasks();
    test_trow_nested_trace();
    test_trow_nested_inert();
}
