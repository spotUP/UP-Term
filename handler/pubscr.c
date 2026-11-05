/* pubscr: see pubscr.h. The guarded public screen lock, moved out of amarchy/iconpen_dos.c. */
#include <string.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include "pubscr.h"
#include "pubscr_core.h"

extern struct IntuitionBase *IntuitionBase;

#if PSNF_PRIVATE != IP_PSN_PRIVATE
#error IP_PSN_PRIVATE must equal PSNF_PRIVATE from intuition/screens.h
#endif

/* The node called `name` in the public screen list (the caller holds its lock). 0 when no such screen is listed.
 * Names compare with strcmp (case-sensitive), as Intuition's own public screen names are. */
struct PubScreenNode *pubscr_node_in(struct List *pl, const char *name)
{
    struct PubScreenNode *n;
    for (n = (struct PubScreenNode *)pl->lh_Head; n->psn_Node.ln_Succ; n = (struct PubScreenNode *)n->psn_Node.ln_Succ)
        if (n->psn_Node.ln_Name && !strcmp(n->psn_Node.ln_Name, name))
            return n;
    return 0;
}

/* BOOT-31-WB-DEADLOCK: LockPubScreen("Workbench") on a closed Workbench makes Intuition reopen it (OS 3.1), which deadlocks
 * against IPrefs closing it for the boot RTG switch. This is the only place that asks for a public screen lock by name
 * (tests/test_wb_lock_guard.py): it reads the node in the list first and calls LockPubScreen only when Intuition will find
 * it (the list lock is held, so it cannot change between).
 * S57-FREEZE-2IN9: "listed with a screen" is not enough. While LoadWB opens the Workbench the node is listed but private;
 * Intuition counts it as absent and takes the same reopen path, which waits for the opening task while that task waits
 * for the list lock held here (2 boots in 9 froze). ip_pub_lockable is Intuition's own test; a screen that is still
 * opening reads as "not open yet", and the callers ask again on their next tick. 0 for a null or empty name. */
struct Screen *ipd_lock_pub_screen(const char *name)
{
    struct List *pl;
    struct PubScreenNode *n;
    struct Screen *sc = 0;
    if (!name || !*name)
        return 0;
    pl = LockPubScreenList();
    n = pubscr_node_in(pl, name);
    if (ip_pub_lockable(n != 0, n && n->psn_Screen, n ? n->psn_Flags : 0))
        sc = LockPubScreen((STRPTR)name);
    UnlockPubScreenList();
    return sc;
}

