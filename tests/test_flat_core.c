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

void suite_flat_core(void)
{
    test_layout();
    test_init_check();
    test_status();
    test_exit();
    test_swap();
    test_tramp();
}
