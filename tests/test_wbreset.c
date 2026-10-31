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
        0x2F, 0x3A, 0x00, 0x0C, 0x66, 0x06, 0x58, 0x8F, 0x2F, 0x3A, 0x00, 0x08, 0x4E, 0x75,
        0x00, 0x12, 0x34, 0x56, 0x00, 0xF8, 0x1A, 0x2C
    };
    unsigned char g[WBR_GATE_SIZE];
    memset(g, 0xEE, sizeof g);
    wbr_gate_build(g, 0x00123456UL, 0x00F81A2CUL);
    CHECK_INT(memcmp(g, want, sizeof g), 0);
    /* PC-relative operands: the extension word sits at 2 / 10 and PC there is the extension word's address */
    CHECK_INT(2 + ((g[2] << 8) | g[3]), WBR_GATE_HOOK);
    CHECK_INT(10 + ((g[10] << 8) | g[11]), WBR_GATE_SAVED);
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

void suite_wbreset(void)
{
    test_external_reset();
    test_failed_close();
    test_self_and_idle();
    test_table();
    test_gate();
    test_hand_patch_finish();
    test_wb_window_flag();
}
