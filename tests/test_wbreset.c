/* WBRESET-EXT (amarchy/wbreset_core.c): an external CloseWorkBench (IPrefs on `Palette ... USE`) must find no
 * Amarchy window on the Workbench screen. Pins the phase machine the patched CloseWorkBench / OpenWorkBench and
 * the bar / amarchy-gui / amarchy-menu clients drive, and the bytes of the gate stub in the library slot. */
#include <stdio.h>
#include <string.h>
#include "../amarchy/wbreset_core.h"
#include "harness.h"

static char bar_t, gui_t, menu_t, iprefs_t;
#define BAR ((void *)&bar_t)
#define GUI ((void *)&gui_t)
#define MENU ((void *)&menu_t)
#define IPREFS ((void *)&iprefs_t)
#define SIG_F 0x8000UL
#define SIG_C 0x1000UL

/* The reported defect: bar and menus up, IPrefs closes the Workbench. Every window owner is signalled first,
 * the patch waits until none holds a window, then the original runs; OpenWorkBench brings the HIDE ones back. */
static void test_external_reset(void)
{
    wbr_rec r;
    wbr_sig s[WBR_MAX];
    int n;
    wbr_init(&r);
    CHECK_INT(wbr_set(&r, BAR, SIG_F, WBR_HIDE, 1) >= 0, 1);
    CHECK_INT(wbr_set(&r, GUI, SIG_F, WBR_HIDE, 1) >= 0, 1);
    CHECK_INT(wbr_set(&r, MENU, SIG_C, WBR_DISMISS, 1) >= 0, 1);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_NONE);       /* no reset going: a stray signal does nothing */

    n = wbr_close_begin(&r, IPREFS, s);
    CHECK_INT(n, 3);
    CHECK_INT(r.phase, WBR_CLOSING);
    CHECK_INT(wbr_pending(&r, IPREFS), 3);

    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_HIDE);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 0);
    CHECK_INT(wbr_client_step(&r, MENU), WBR_DO_HIDE);
    wbr_set(&r, MENU, SIG_C, WBR_DISMISS, 0);
    CHECK_INT(wbr_pending(&r, IPREFS), 1);
    CHECK_INT(wbr_client_step(&r, GUI), WBR_DO_HIDE);
    wbr_set(&r, GUI, SIG_F, WBR_HIDE, 0);
    CHECK_INT(wbr_pending(&r, IPREFS), 0);                  /* nothing of ours on the screen: close can succeed */

    CHECK_INT(wbr_close_end(&r, 1, 0, s), 0);
    CHECK_INT(r.phase, WBR_CLOSED);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_NONE);       /* closed and waiting: no reopen yet */

    n = wbr_open_end(&r, 1, s);
    CHECK_INT(n, 2);                                        /* bar + gui come back, the dismissed menu does not */
    CHECK_INT(s[0].task == BAR || s[1].task == BAR, 1);
    CHECK_INT(s[0].task == MENU || s[1].task == MENU, 0);
    CHECK_INT(r.phase, WBR_OPEN);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_SHOW);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_NONE);
    CHECK_INT(wbr_client_step(&r, GUI), WBR_DO_SHOW);
    CHECK_INT(wbr_client_step(&r, MENU), WBR_DO_NONE);
    CHECK_INT(r.closes, 1);
    CHECK_INT(r.opens, 1);
}

/* Another program's window kept the Workbench: ours reopen at once, the phase is OPEN again. */
static void test_failed_close(void)
{
    wbr_rec r;
    wbr_sig s[WBR_MAX];
    wbr_init(&r);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 1);
    wbr_close_begin(&r, IPREFS, s);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_HIDE);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 0);
    CHECK_INT(wbr_close_end(&r, 0, 0, s), 1);
    CHECK_INT(s[0].task == BAR, 1);
    CHECK_INT(s[0].sig, SIG_F);
    CHECK_INT(r.phase, WBR_OPEN);
    CHECK_INT(r.failed, 1);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_SHOW);
}

/* The caller is never waited on (it is inside the patch); a client without a window is not signalled. */
/* 3.1 with Directory Opus: IPrefs' first CloseWorkBench fails while DOpus closes its windows, the bar starts to
 * reopen, and IPrefs retries at once. The reopening bar already holds the screen lock, so the retry must signal it
 * and wait, not run the original into the lock ("Intuition is attempting to reset the Workbench screen"). */
static void test_retry_while_reopening(void)
{
    wbr_rec r;
    wbr_sig s[WBR_MAX];
    wbr_init(&r);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 1);
    CHECK_INT(wbr_close_begin(&r, IPREFS, s), 1);
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_HIDE);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 0);                   /* the bar closed and reported */
    CHECK_INT(wbr_close_end(&r, 0, 0, s), 1);               /* DOpus still had its windows: the bar comes back */
    CHECK_INT(wbr_client_step(&r, BAR), WBR_DO_SHOW);       /* the bar starts to open: lock taken, no report yet */
    CHECK_INT(wbr_close_begin(&r, IPREFS, s), 1);           /* IPrefs retries: the opening bar is signalled */
    CHECK_INT(wbr_pending(&r, IPREFS), 1);                  /* and waited for */
}

static void test_self_and_idle(void)
{
    wbr_rec r;
    wbr_sig s[WBR_MAX];
    wbr_init(&r);
    wbr_set(&r, BAR, SIG_F, WBR_HIDE, 1);
    wbr_set(&r, GUI, SIG_F, WBR_HIDE, 0);
    CHECK_INT(wbr_close_begin(&r, BAR, s), 0);
    CHECK_INT(wbr_pending(&r, BAR), 0);
    CHECK_INT(wbr_client_step(&r, GUI), WBR_DO_NONE);
    CHECK_INT(wbr_close_end(&r, 1, 1, s), 0);
    CHECK_INT(r.timeouts, 1);
    CHECK_INT(wbr_open_end(&r, 0, s), 0);                   /* failed open: phase stays CLOSED */
    CHECK_INT(r.phase, WBR_CLOSED);
}

static void test_table(void)
{
    wbr_rec r;
    static char t[WBR_MAX + 1];
    int i;
    wbr_init(&r);
    CHECK_INT(wbr_set(&r, 0, SIG_F, WBR_HIDE, 1), -1);
    for (i = 0; i < WBR_MAX; i++)
        CHECK_INT(wbr_set(&r, &t[i], SIG_F, WBR_HIDE, 0), i);
    CHECK_INT(wbr_set(&r, &t[WBR_MAX], SIG_F, WBR_HIDE, 0), -1);
    CHECK_INT(wbr_set(&r, &t[3], SIG_C, WBR_DISMISS, 1), 3);  /* update keeps the slot */
    wbr_drop(&r, &t[3]);
    CHECK_INT(wbr_set(&r, &t[WBR_MAX], SIG_F, WBR_HIDE, 0), 3);
    CHECK_STR(WBR_SEM_NAME, "Amarchy.WBReset");
    CHECK_INT(WBR_LVO_CLOSEWB, -78);
    CHECK_INT(WBR_LVO_OPENWB, -210);
}

static void test_gate(void)
{
    static const unsigned char want[WBR_GATE_SIZE] = {
        0x52, 0xB9, 0x00, 0x40, 0x00, 0x22,                 /* addq.l #1,($400022).l */
        0x2F, 0x3A, 0x00, 0x12, 0x66, 0x0C,                 /* move.l hook(pc),-(sp); bne.s go */
        0x53, 0xB9, 0x00, 0x40, 0x00, 0x22,                 /* subq.l #1,($400022).l */
        0x58, 0x8F, 0x2F, 0x3A, 0x00, 0x08, 0x4E, 0x75,     /* addq.l #4,sp; move.l saved(pc),-(sp); go: rts */
        0x00, 0x12, 0x34, 0x56, 0x00, 0xF8, 0x1A, 0x2C,     /* hook, saved */
        0x00, 0x00, 0x00, 0x00                              /* busy */
    };
    unsigned char g[WBR_GATE_SIZE];
    memset(g, 0xEE, sizeof g);
    wbr_gate_build(g, 0x00123456UL, 0x00F81A2CUL, 0x00400000UL);
    CHECK_INT(memcmp(g, want, sizeof g), 0);
    /* PC-relative operands: the extension word sits at 8 / 22 and PC there is the extension word's address */
    CHECK_INT(8 + ((g[8] << 8) | g[9]), WBR_GATE_HOOK);
    CHECK_INT(22 + ((g[22] << 8) | g[23]), WBR_GATE_SAVED);
    CHECK_INT(WBR_GATE_BUSY + 4, WBR_GATE_SIZE);
}

/* Tiny 68000 stepper for exactly the gate's six opcodes. mem[] is the gate at SELF; stack is one slot (the gate
 * pushes once and rts pops it). Z follows the last move/addq/subq, as the CPU's does. */
#define SELF 0x00400000UL
#define HOOK_T 0x00123456UL
#define SAVED_T 0x00F81A2CUL
typedef struct { unsigned char m[WBR_GATE_SIZE]; unsigned pc; int z, done; uint32_t sp, target; } gstep;

static uint32_t rd32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static unsigned rd16(const unsigned char *p) { return (p[0] << 8) | p[1]; }
static void wr32(unsigned char *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void gstep_one(gstep *g)
{
    unsigned op = rd16(g->m + g->pc);
    if (op == 0x52B9 || op == 0x53B9) {         /* addq.l / subq.l #1,abs.l: one indivisible step */
        unsigned char *busy = g->m + (rd32(g->m + g->pc + 2) - SELF);
        wr32(busy, rd32(busy) + (op == 0x52B9 ? 1 : (uint32_t)-1));
        g->z = rd32(busy) == 0;
        g->pc += 6;
    } else if (op == 0x2F3A) {                  /* move.l d16(pc),-(sp) */
        int d = (short)rd16(g->m + g->pc + 2);
        g->sp = rd32(g->m + g->pc + 2 + d);
        g->z = g->sp == 0;
        g->pc += 4;
    } else if ((op >> 8) == 0x66) {             /* bne.s */
        g->pc = g->z ? g->pc + 2 : g->pc + 2 + (op & 0xFF);
    } else if (op == 0x588F) {                  /* addq.l #4,sp: drops the hook word */
        g->sp = 0;
        g->pc += 2;
    } else if (op == 0x4E75) {                  /* rts: the jump target is what is on the stack */
        g->target = g->sp;
        g->done = 1;
    } else {
        CHECK(!"gate holds an opcode the stepper does not know");
        g->done = 1;
    }
}

/* AMARCHY-IPATCH-GATE: a caller pre-empted at ANY instruction boundary of the gate, with the remover then zeroing the
 * hook word and reading the counter, is either counted (the remover waits for it) or goes to saved (never into the
 * hook). The old gate read the hook word first and counted in the C hook: a caller between the two was invisible. */
static void test_gate_counts_before_it_reads_the_hook(void)
{
    int k, n = 0;
    gstep full;
    memset(&full, 0, sizeof full);
    wbr_gate_build(full.m, HOOK_T, SAVED_T, SELF);
    while (!full.done && n < 20) { gstep_one(&full); n++; }
    CHECK_INT(full.target, HOOK_T);
    CHECK_INT(rd32(full.m + WBR_GATE_BUSY), 1);             /* hook path: counted, the hook uncounts on exit */
    for (k = 0; k <= n; k++) {
        gstep g;
        int i;
        uint32_t seen;
        memset(&g, 0, sizeof g);
        wbr_gate_build(g.m, HOOK_T, SAVED_T, SELF);
        for (i = 0; i < k && !g.done; i++)
            gstep_one(&g);
        wr32(g.m + WBR_GATE_HOOK, 0);                       /* the remover: passthru, then read busy */
        seen = rd32(g.m + WBR_GATE_BUSY);
        for (i = 0; i < 20 && !g.done; i++)
            gstep_one(&g);
        if (!(seen > 0 || g.target == SAVED_T))
            printf("  boundary %d: hook entered with busy 0\n", k);
        CHECK(seen > 0 || g.target == SAVED_T);
    }
    memset(&full, 0, sizeof full);
    wbr_gate_build(full.m, 0, SAVED_T, SELF);               /* pass-through run */
    for (n = 0; !full.done && n < 20; n++)
        gstep_one(&full);
    CHECK_INT(full.target, SAVED_T);
    CHECK_INT(rd32(full.m + WBR_GATE_BUSY), 0);             /* uncounted itself */
}

/* Source lint: the host cannot run Intuition, so the two rules that keep the reset working are pinned on the text
 * of the Amiga-side files (run from the repo root, as `make test` does). */
static int count_in(const char *path, const char *needle)
{
    static char buf[400000];
    FILE *f = fopen(path, "rb");
    size_t n;
    int c = 0;
    const char *p;
    if (!f)
        return -1;
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    for (p = buf; (p = strstr(p, needle)) != 0; p += strlen(needle))
        c++;
    return c;
}

/* A hand-written jump-table change finishes with libpatch_done (LIBF_CHANGED + CacheClearU + SumLibrary, the steps
 * SetFunction takes); a bare SumLibrary leaves the checksum stale: RECOVERABLE ALERT #01000003 at boot. */
static void test_hand_patch_finish(void)
{
    static const char *const files[] = { "amarchy/flat.c", "amarchy/wbreset_patch.c" };
    unsigned i;
    CHECK_INT(count_in("amarchy/libpatch.h", "lib_Flags |= LIBF_CHANGED"), 1);
    CHECK_INT(count_in("amarchy/libpatch.h", "SumLibrary(lib)"), 1);
    CHECK_INT(count_in("amarchy/libpatch.h", "CacheClearU()") >= 1, 1);
    for (i = 0; i < sizeof files / sizeof *files; i++) {
        CHECK_INT(count_in(files[i], "SumLibrary("), 0);
        CHECK_INT(count_in(files[i], "libpatch_done(") >= 1, 1);
    }
}

/* IPrefs 40.7 counts the windows on the Workbench screen that lack WFLG_WBENCHWINDOW and shows "Please close all
 * windows, except drawers" without ever calling CloseWorkBench, so the gate never runs. Every window Amarchy opens
 * on the Workbench screen (the status bar via tw_open, the drawn menu via gui window_open) carries WA_WBenchWindow.
 * shot.c's full-screen overlay is on the front screen for one drag and is exempt. */
static void test_wb_window_flag(void)
{
    static const char *const files[] = { "amarchy/gui.c", "amarchy/themewin.c" };
    unsigned i;
    for (i = 0; i < sizeof files / sizeof *files; i++) {
        int opens = count_in(files[i], "OpenWindowTag");
        CHECK_INT(opens >= 1, 1);
        CHECK_INT(count_in(files[i], "WA_WBenchWindow"), opens);
    }
    CHECK_INT(count_in("amarchy/bar.c", "OpenWindow"), 0);      /* the bar opens only through tw_open */
}

/* TE-7: reset-bound only for the fields of the plan's section 5 table; every live field leaves the mask 0. */
static void test_reset_bound_table(void)
{
    gt_model a, b;
    memset(&a, 0, sizeof a);
    b = a;
    CHECK_INT((int)wbr_bound_mask(&a, &b), 0);
    CHECK_INT((int)wbr_bound_mask(0, &b), 0);
    strcpy(b.look.font[GT_FONT_SCREEN], "helvetica.font/13");
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_SCREEN_FONT);
    b = a;
    strcpy(b.look.font[GT_FONT_TITLE], "topaz.font/9");
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_TITLE_FONT);
    b = a; b.look.bar_height = 20;
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_BAR_HEIGHT);
    b = a; b.look.gadget_size = 18;
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_GADGET_SIZE);
    b = a; b.look.gadget_pad = 3;
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_GADGET_PAD);
    b = a; b.look.gadget_order = 1;
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_GADGET_ORDER);
    b = a; b.look.scroll_width = 16;
    CHECK_INT((int)wbr_bound_mask(&a, &b), WBR_B_SCROLL_WIDTH);
    /* the live fields of the table: colours, radius, border, knob, glyphs, ghost, hover, menu / icon / shell font, pointer */
    b = a;
    b.look.corner_radius = 9; b.look.border_width = 2; b.look.knob_min = 8; b.look.knob_shape = 1; b.look.glyph_set = 1;
    b.look.ghost_style = 1; b.look.hover_on = 1; b.look.inactive_pct = 50; b.look.pointer[0] = 0x123456u;
    strcpy(b.look.font[GT_FONT_MENU], "m.font/9");
    strcpy(b.look.font[GT_FONT_ICON], "i.font/9");
    strcpy(b.look.font[GT_FONT_SHELL], "s.font/9");
    b.swatch[0] = 0xabcdefu;
    CHECK_INT((int)wbr_bound_mask(&a, &b), 0);
}

void suite_wbreset(void)
{
    test_reset_bound_table();
    test_external_reset();
    test_failed_close();
    test_retry_while_reopening();
    test_self_and_idle();
    test_table();
    test_gate();
    test_gate_counts_before_it_reads_the_hook();
    test_hand_patch_finish();
    test_wb_window_flag();
}
