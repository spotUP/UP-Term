/* pubscr_core -- the pure decision of the guarded public screen lock (handler/pubscr.c), host-tested (tests/test_iconpen.c). */
#ifndef PUBSCR_CORE_H
#define PUBSCR_CORE_H

/* S57-FREEZE-2IN9: the psn_Flags bit of a public screen node that is not public (yet): intuition's PSNF_PRIVATE. */
#define IP_PSN_PRIVATE 0x0001
/* Whether LockPubScreen(name) will FIND the public screen (any name, the Workbench included), from its node read under the public screen list lock:
 * the node is listed, has a screen and is not private. While LoadWB opens the Workbench the node is listed but still
 * private; Intuition then treats it as absent and takes its "reopen the Workbench" path, which waits for the task
 * that is opening it. A caller that holds the list lock must not make that call: the opening task needs the list
 * lock to make the screen public, so both wait for ever (evidence: s57-evidence/freeze-9boot-rig145.md). */
int ip_pub_lockable(int listed, int has_screen, unsigned flags);

#endif
