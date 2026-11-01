# Phase 4: upkg browse rig (rig 4), 2026-10-21

Script: tools/rig/upkg_browse_rig.py (flow: `upkg browse`, Down to up-vim, Return opens the info box, Return on Install,
programbox, Return (OK), Return on up-vim, Return on Remove, y, programbox OK, q).

## Run 2 (clean pass, 12 of 12 checks)
- step 0 update: PASS; before (not installed, no sentinel): PASS
- install via info box + programbox: sentinel UP-Term:pkg/up-vim/Uninstall appeared: PASS; list-installed has up-vim: PASS
- remove via info box + Yes + programbox: sentinel gone: PASS; list-installed without up-vim: PASS
- colour: green pixels in the window area 0 (before) -> 2030 (installed) -> 0 (removed): PASS
- no Shell text (window text via Select all + Copy on the list before, after install, after remove): PASS
- q returns to the shell prompt: PASS (echo ran)

## Colour method
No live dump of window cell attributes exists in tools/rig (vtdump replays a byte stream; snipprobe copies text only; cudump is a
ConUnit struct probe). Used colour_check.pixels on a rig screenshot instead. upkg-ui draws the SELECTED row in the base attribute
(no \Z colour, chrome "A selected row keeps base"), so the check moves the cursor Up one row, shoots, and moves Down again.
A real cell-attribute dump of a live window would need a new probe (not built).

## Later runs (not product bugs, unverified cause)
- Run 3: the install did not finish in 240 s (programbox still showed "Wait: Command running"; host load average 20-30 from other agents);
  the timeout is now 420 s.
- Runs 4-7: `upkg update` fails with curl 23 (write error). Rig 4 info showed DH0: System 100% full (2 blocks free) after the hung
  run 3 left an install half done; RAM: also reported full. Rig-state damage, not the script. Not investigated further (budget).
  Next run needs a clean rig 4 disk (rig.py start --fresh or equivalent) before re-verifying the 420 s change.
- The 420 s timeout and the window-text notes in report.md were added after run 2; run 3 exercised the notes, nothing has run
  the 420 s value to completion.

## Fresh rig 4 run (2026-10-22)

Result: `UPTERM_RIG=4 python3 tools/rig/upkg_browse_rig.py` -> Verdict: PASS (12 of 12 checks), report build/rig4/upkg-browse/report.md.
Code: upterm-ports 8206576, vtcon 38c8d21c (plus uncommitted work tree).
Steps: moved build/rig4/sys.hdf aside, `rig.py setup` (copied the 1.5 GB system disk anew), booted, deleted the old copy.
`make kit-stage` did not rebuild upkg-ui (binary 23:12 older than source 23:47): ran `make upkg-ui` in upterm-ports first (302892 bytes, 00:07), then `make kit-stage` (about 10 min), then `make dist && make dist-ipk` in vtcon. upkg is built by opkg's recipe (no `make upkg` target); its binary was newer than the recipe.
Checks: update rc 0; install sentinel appears; list-installed present/absent; green pixels 0 -> 2030 -> 0; no shell text in the three list screens.
Rig 4 stopped at the end by the script. No source files changed.
