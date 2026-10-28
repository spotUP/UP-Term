---
date: 2026-10-28
session: vtcon-05 session 17
topic: INSTALLER-END-CRASH (installer dies at the Proceed on the outer 100% "Reboot once" page)
status: open
---

# Session 17 handoff: INSTALLER-END-CRASH

## Done
- d63a5cd5: installer drive answers P_END's "Reboot now?" only when the text says "Reboot now"; the
  "Reboot once: then LIBS: has AmiSSL" message page before it gets Proceed. Tests in
  tests/test_installer_rig_ask.py and tests/test_installer_drive_loop.py (fail with the fix reverted).
- 5a3d62b0: `VTC:inhandlers ADDR` (name, node, is_Code, is_Data, read inside Forbid) and
  `INSTALLER_CHAIN=1` on installer_rig.py (chain dump logged before each Proceed).
- Static check: IND_ADDHANDLER/IND_REMHANDLER pairing is correct in amarchy/cx.c,
  device/upcon_input.c, handler/vtcon_handler.c (winch_remove), proc/top_amiga.c.

## Evidence
- Run 3 (d63a5cd5): Proceed on the "Reboot once" page, then wasabi TimeoutError: guest hung.
- Snoop probe: last Installer events are the P_ADFIND scan (Execute T:UPTADDLST, Delete ...) then
  FindTask x3; then the page and the click; "machine is frozen". bsdsocket ruled out. No Guru.
  thoughts/spot/term-findings/rig9-installer-loop.md "## End crash probe s17".
- Chain dump run: every dump = commodities.library, intuition.library, console.device + inhandlers.
  No stale node. This run CRASHED instead: "Illegal instruction: 1050 at 104E8438 -> 00F80AD2",
  reset loop. Same file, "## Chain dump s17".

## Open lead
- A CxCustom object (Amarchy gate, DOpus, others) runs inside the commodities.library chain node,
  so the input.device dump cannot show it. A broker whose program exited without DeleteCxObjAll
  leaves a dangling custom function there.
- PC 104E8438 is not inside any inhandlers code; its owner is unknown.

## Next steps
1. Before the hanging click, dump commodities.library's broker list (names, custom object
   functions) and the seglist ranges of every process and library; find who owns 104E8438.
   One rig (9), one sonnet agent, cost estimate first.
2. If a broker is stale: fix the program that leaves it, regression test, rerun fresh-all.
3. Then INSTALLER-CLICK-SPEED step 3 timing; LAYOUT bottom-strip (y 204..205) on the outer 100% page;
   nvim 5-run recheck (NVIM-HEAP-CORRUPT).

## Constraints
Rigs 9/10/11 only, one at a time (Mac runs hot); never rigs 1,2,3,4,7,8. Leave other sessions'
uncommitted files alone (see the s16 handoff list).
