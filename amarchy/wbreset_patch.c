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
#include "ipatch.h"
#include "sentinel_dos.h"

extern struct IntuitionBase *IntuitionBase;

typedef LONG (*wbfn)(__reg("a6") struct Library *);

static wbr_pub *g_pub;
static ipatch g_pc, g_po;       /* CloseWorkBench / OpenWorkBench */

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
    ok = ((wbfn)ipatch_saved(&g_pc))(ib);
    g_pub->rec.diag_ok = ok;
    Forbid();
    n = wbr_close_end(&g_pub->rec, ok != 0, timed, s);
    signal_all(s, n);
    Permit();
    ipatch_leave(&g_pc);
    return ok;
}

static LONG open_hook(__reg("a6") struct Library *ib)
{
    wbr_sig s[WBR_MAX];
    LONG scr;
    int n;
    scr = ((wbfn)ipatch_saved(&g_po))(ib);
    Forbid();
    n = wbr_open_end(&g_pub->rec, scr != 0, s);
    signal_all(s, n);
    Permit();
    ipatch_leave(&g_po);
    return scr;
}

int wbr_patch_install(void)
{
    int rc1, rc2;
    if (!IntuitionBase)
        return -1;
    rc1 = ipatch_alloc(&g_pc, WBR_LVO_CLOSEWB);
    if (rc1)
        return rc1;
    rc1 = ipatch_alloc(&g_po, WBR_LVO_OPENWB);
    if (rc1) {
        ipatch_forget(&g_pc);
        return rc1;
    }
    Forbid();
    rc1 = FindSemaphore((STRPTR)WBR_SEM_NAME) != 0;
    Permit();
    if (rc1) {
        ipatch_forget(&g_pc);
        ipatch_forget(&g_po);
        return -3;                  /* another Amarchy holds the patch */
    }
    g_pub = (wbr_pub *)AllocMem(sizeof *g_pub, MEMF_PUBLIC | MEMF_CLEAR);
    if (!g_pub) {
        ipatch_forget(&g_pc);
        ipatch_forget(&g_po);
        return -4;
    }
    wbr_init(&g_pub->rec);
    strcpy(g_pub->name, WBR_SEM_NAME);
    g_pub->ss.ss_Link.ln_Name = g_pub->name;
    g_pub->ss.ss_Link.ln_Pri = 0;
    AddSemaphore(&g_pub->ss);
    Forbid();
    rc1 = ipatch_arm(&g_pc, (void *)close_hook);
    rc2 = ipatch_arm(&g_po, (void *)open_hook);
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
    int rc1, rc2, i;
    if (!g_pub)
        return;
    Forbid();
    ipatch_passthru(&g_pc);                 /* pass-through from now on, also for a gate left chained */
    ipatch_passthru(&g_po);
    rc1 = ipatch_unarm(&g_pc);
    rc2 = ipatch_unarm(&g_po);
    libpatch_done((struct Library *)IntuitionBase);
    RemSemaphore(&g_pub->ss);
    Permit();
    Delay(5);
    for (i = 0; ipatch_busy(&g_pc) + ipatch_busy(&g_po); i++) {
        if (i && i % 50 == 0)          /* ~10 s a round: a hook that never returns stays visible on the rig */
            am_sentinel("AMARCHY_WBRESET_BUSY_WAIT", i / 5);
        Delay(10);                      /* a caller is inside: stay resident, a Guru under a live hook is worse */
    }
    ipatch_forget(&g_pc);
    ipatch_forget(&g_po);
    FreeMem(g_pub, sizeof *g_pub);
    g_pub = 0;
}
