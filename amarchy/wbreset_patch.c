/* WBRESET-EXT: the Amarchy commodity's patch of intuition CloseWorkBench (LVO -78) and OpenWorkBench (LVO -210).
 * Each JMP operand is swapped with the flat_swap model (flat_core.c, as amarchy-flat's Text patch) to a gate stub
 * (wbr_gate_build) that jumps to the hook below, or straight to the original once the hook is cleared. The close
 * hook signals every client with a window on the Workbench screen, waits (bounded) until none is left, then runs
 * the original; the open hook runs the original and signals the hidden clients to come back. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include <string.h>
#include "wbreset.h"
#include "flat_core.h"
#include "libpatch.h"

extern struct IntuitionBase *IntuitionBase;

typedef LONG (*wbfn)(__reg("a6") struct Library *);

static wbr_pub *g_pub;
static unsigned char *g_gc, *g_go;  /* gates of CloseWorkBench / OpenWorkBench */
static uintptr_t *g_slc, *g_slo;
static flat_swap g_swc, g_swo;

static ULONG gate_long(const unsigned char *g, int at)
{
    return ((ULONG)g[at] << 24) | ((ULONG)g[at + 1] << 16) | ((ULONG)g[at + 2] << 8) | g[at + 3];
}

static void signal_all(const wbr_sig *s, int n)
{
    while (n-- > 0)
        Signal((struct Task *)s[n].task, s[n].sig);
}

/* Diagnostic (WBRESET-EXT 3.1): the Workbench public screen's visitors and windows just before the original runs. */
static void diag_census(void)
{
    struct List *pl = LockPubScreenList();
    struct PubScreenNode *n;
    struct Window *w;
    int v = -1, k = 0;
    for (n = (struct PubScreenNode *)pl->lh_Head; n->psn_Node.ln_Succ; n = (struct PubScreenNode *)n->psn_Node.ln_Succ)
        if (n->psn_Node.ln_Name && !strcmp(n->psn_Node.ln_Name, "Workbench")) {
            v = n->psn_VisitorCount;
            if (n->psn_Screen)
                for (w = n->psn_Screen->FirstWindow; w; w = w->NextWindow)
                    k++;
        }
    UnlockPubScreenList();
    g_pub->rec.diag_vis = (int16_t)v;
    g_pub->rec.diag_wins = (int16_t)k;
}

/* Runs on the caller (IPrefs after a prefs USE, or any program). Never waits inside Forbid, never on its own task. */
static LONG close_hook(__reg("a6") struct Library *ib)
{
    wbr_sig s[WBR_MAX];
    struct Task *me = FindTask(0);
    int n, t, timed = 0;
    LONG ok;
    Forbid();
    g_pub->rec.busy++;
    n = wbr_close_begin(&g_pub->rec, me, s);
    signal_all(s, n);
    Permit();
    if (n && me->tc_Node.ln_Type == NT_PROCESS) {
        for (t = 0; t < WBR_WAIT_TICKS; t += 2) {
            Forbid();
            n = wbr_pending(&g_pub->rec, me);
            Permit();
            if (!n)
                break;
            Delay(2);
        }
        timed = n != 0;
    }
    diag_census();
    ok = ((wbfn)gate_long(g_gc, WBR_GATE_SAVED))(ib);
    g_pub->rec.diag_ok = ok;
    Forbid();
    n = wbr_close_end(&g_pub->rec, ok != 0, timed, s);
    signal_all(s, n);
    g_pub->rec.busy--;
    Permit();
    return ok;
}

static LONG open_hook(__reg("a6") struct Library *ib)
{
    wbr_sig s[WBR_MAX];
    LONG scr;
    int n;
    Forbid();
    g_pub->rec.busy++;
    Permit();
    scr = ((wbfn)gate_long(g_go, WBR_GATE_SAVED))(ib);
    Forbid();
    n = wbr_open_end(&g_pub->rec, scr != 0, s);
    signal_all(s, n);
    g_pub->rec.busy--;
    Permit();
    return scr;
}

int wbr_patch_install(void)
{
    unsigned char *ec, *eo;
    int rc1, rc2;
    if (!IntuitionBase)
        return -1;
    ec = (unsigned char *)IntuitionBase + WBR_LVO_CLOSEWB;
    eo = (unsigned char *)IntuitionBase + WBR_LVO_OPENWB;
    if (!flat_jmp_target(ec) || !flat_jmp_target(eo))
        return -2;
    Forbid();
    rc1 = FindSemaphore((STRPTR)WBR_SEM_NAME) != 0;
    Permit();
    if (rc1)
        return -3;                  /* another Amarchy holds the patch */
    g_pub = (wbr_pub *)AllocMem(sizeof *g_pub, MEMF_PUBLIC | MEMF_CLEAR);
    g_gc = (unsigned char *)AllocMem(WBR_GATE_SIZE, MEMF_PUBLIC);
    g_go = (unsigned char *)AllocMem(WBR_GATE_SIZE, MEMF_PUBLIC);
    if (!g_pub || !g_gc || !g_go) {
        if (g_pub)
            FreeMem(g_pub, sizeof *g_pub);
        if (g_gc)
            FreeMem(g_gc, WBR_GATE_SIZE);
        if (g_go)
            FreeMem(g_go, WBR_GATE_SIZE);
        g_pub = 0;
        g_gc = g_go = 0;
        return -4;
    }
    wbr_init(&g_pub->rec);
    strcpy(g_pub->name, WBR_SEM_NAME);
    g_pub->ss.ss_Link.ln_Name = g_pub->name;
    g_pub->ss.ss_Link.ln_Pri = 0;
    AddSemaphore(&g_pub->ss);
    flat_swap_init(&g_swc);
    flat_swap_init(&g_swo);
    Forbid();
    wbr_gate_build(g_gc, (uint32_t)close_hook, flat_jmp_target(ec));
    wbr_gate_build(g_go, (uint32_t)open_hook, flat_jmp_target(eo));
    CacheClearU();
    rc1 = flat_swap_install(&g_swc, (uintptr_t *)(ec + 2), (uintptr_t)g_gc);
    rc2 = flat_swap_install(&g_swo, (uintptr_t *)(eo + 2), (uintptr_t)g_go);
    g_slc = (uintptr_t *)(ec + 2);
    g_slo = (uintptr_t *)(eo + 2);
    libpatch_done((struct Library *)IntuitionBase);
    Permit();
    if (rc1 != FLAT_OK || rc2 != FLAT_OK) {
        wbr_patch_remove();
        return -5;
    }
    return 0;
}

void wbr_patch_remove(void)
{
    int rc1 = FLAT_NOT_INSTALLED, rc2 = FLAT_NOT_INSTALLED, i;
    if (!g_pub)
        return;
    Forbid();
    memset(g_gc + WBR_GATE_HOOK, 0, 4);     /* pass-through from now on, also for a gate left chained */
    memset(g_go + WBR_GATE_HOOK, 0, 4);
    CacheClearU();
    if (g_swc.installed)
        rc1 = flat_swap_remove(&g_swc, g_slc, 0);
    if (g_swo.installed)
        rc2 = flat_swap_remove(&g_swo, g_slo, 0);
    libpatch_done((struct Library *)IntuitionBase);
    RemSemaphore(&g_pub->ss);
    Permit();
    Delay(5);                               /* a caller past the gate but not yet counted gets in */
    for (i = 0; i < 2 * WBR_WAIT_TICKS && g_pub->rec.busy; i++)
        Delay(2);
    if (g_pub->rec.busy)
        return;                             /* a caller is still inside: leak rather than free under it */
    if (rc1 != FLAT_STILL_CHAINED)
        FreeMem(g_gc, WBR_GATE_SIZE);
    if (rc2 != FLAT_STILL_CHAINED)
        FreeMem(g_go, WBR_GATE_SIZE);
    FreeMem(g_pub, sizeof *g_pub);
    g_pub = 0;
    g_gc = g_go = 0;
}
