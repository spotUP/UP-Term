/* WBRESET-EXT (amarchy/wbreset_core.c): an external CloseWorkBench (IPrefs on `Palette ... USE`) must find no
 * Amarchy window on the Workbench screen. Pins the phase machine the patched CloseWorkBench / OpenWorkBench and
 * the bar / amarchy-gui / amarchy-menu clients drive, and the bytes of the gate stub in the library slot. */
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

void suite_wbreset(void)
{
    test_external_reset();
    test_failed_close();
    test_self_and_idle();
    test_table();
    test_gate();
}
