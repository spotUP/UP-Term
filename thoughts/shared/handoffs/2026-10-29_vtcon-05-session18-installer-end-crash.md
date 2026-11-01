---
date: 2026-10-29
session: vtcon-05 session 18
topic: INSTALLER-END-CRASH root cause and fix
status: fix landed, follow-ups open
---

# Handoff: vtcon-05 session 18

## Done
- 50877ec3 / pushed: tests/amiga/crashme.c reproduces the 060 reset loop (ROM ColdReboot after a guru).
- df860227 / pushed: rig watchdog restarts a rig stuck in the Kickstart 80000006 reset loop.
- 6fd0c83a on origin/main (local 2d04e3bf): INSTALLER-END-CRASH fixed. The last page's #n reached 16-17
  (wasabi branch kept #adrows after moving the note), P_LEAD's substr got length -1 and the Installer
  zero-filled memory over rtg.library's vblank server. Fix in dist/Install.installer + dist/common/procs.installer,
  test LastPageLineCount in tests/test_dist_installer.py. Verified: fresh-all on rig 9 reaches the end page and
  the reboot offer, no crash.
- Evidence: thoughts/spot/term-findings/rig9-installer-loop.md "### BREAK 1", "### Run 2", "### Run 3".

## Open
- FRESH-ALL-AFTER-END (ledger, new): 4 fails after the end page, never reached before: Amarchy hotkey window
  after the reboot (32), Uninstall leaves VTCX:UP-Term (38), Uninstall does not restore ixemul/ixnet (39/40).
- Local main has the session's commits as originals; origin/main has cherry-picked copies. Rebase local main
  once other sessions' dirty files are committed (do not stash them).
- RIG-SLOW-BOOT open decision: skip per-boot shadow copies on warm reboots (owner call).
- make dist-check fails WasabiDaemonOption licence/source test: dist/wasabi/SOURCE.txt and dist/LICENSES.txt
  still name wasabi be4c0559 while tools/nas/wasabi.rev is 562672a8 (vtcon-32 session item).
- Debug FS-UAE patch uncommitted in /Users/spot/Code/Emulators/fs-uae-debug (newcpu.cpp fault PC break,
  uaemisc.cpp fflush). stubs.cpp and rpc.cpp there are other people's edits.
