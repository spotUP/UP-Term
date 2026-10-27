---
date: 2026-10-27
session: vtcon-05 session 9 (UP-Term side)
status: handoff
ledger: thoughts/spot/todos.md (rows U8 DONE, UHC-PATH-BEFORE-REBOOT, TSV-LAUNCH, UPKG-AMARCHY-API, RIG-NO-DRIVE-SOUNDS)
---

# vtcon-05 session 9: UHC repo U8 done, upgrade questions PR, drive sounds

## Landed
- UP-Term PR 3 (feat/upkg-ask-answered, 472e45d9): dist/upkg-ask honours UPKG_ASK_STATE / UPKG_ASK_UNATTENDED; test tests/test_upkg_ask.py::test_an_upgrade_asks_only_new_questions. Merge after ports PR 14.
- Ports PR 14 (2d44c71): its patch renamed 0042 -> 0043. tools/patches.sh numbers by git format-patch order of the opkg tree, so a regen reverts a hand rename; the real renumber is a rebase of each branch's opkg tree onto the merged stack as PRs merge. Targets: 0040 browse-launch, 0041 install-all, 0042 EINTR (PR 11), 0043 upgrade-answers (PR 14), feat/uhc-repo 0044-0048.
- feat/uhc-repo 66abeb7 (pushed, no PR): uhc_install runs `/bin/sh -c "Execute <path>"` (C:Execute only redirects the caller's CLI input). Needs vsh from 18 Oct or later. Test pkgs/opkg/check/uhc_run_test.c via uhc_run.sh.
- U8 on rig 8: all checks PASS (install, real split, list-installed, remove, browse install of uhc:pipe, ipk postinst). Evidence: thoughts/spot/term-findings/uhc-u8-rig.md.
- Rig drive sounds: origin/main ecd88675 (vtcon-a1) sets floppy_drive_volume = 0 and floppy_drive_volume_empty = 0. Main checkout's uncommitted rig.py (another session) carries the same two settings at line 667.

## Next, in order
1. UHC-PATH-BEFORE-REBOOT + TSV-LAUNCH on feat/uhc-repo (worktree ../upterm-ports-uhc). Rules in ledger row TSV-LAUNCH. Reply to vtcon-a1 with hash, m68k md5, field 7 text.
2. feat/uhc-repo PR after the renumber rebase.
3. Rest of UPKG-AMARCHY-API: question channel, progress line format, stable exit codes.
4. PR 9 rerun items (up-iaask answer kept; Shift+U "Update all 4" where 2 expected; Shift+I --only 4).
5. Rig 8: PR 10 even-row height, PR 11 mid-install resize.
6. ixemul mkdir (ixemul-vtcon PR 3, worktree ixemul-mkdir; A1200 install needs approval; clean build295 and docker image ixemul-gcc295).
7. L7 feed rebuild after PR 8; NVIM-PERF; CI rolling feed F1-F13; creep CP6; upgrade-path rig run for PR 14 / PR 3; `make host-opkg` in upterm-ports-allinst before next use.

## Blocked on owner
- A1200 off the network since `ami.py reboot` after running up-uhc 1.3 install.dos (thoughts/spot/term-findings/uhc-pr2-a1200-reboot.md). Owner looks at the screen; then rerun GetEnv UHC/AMINETMIRROR, GetEnv UHC/UHCBIN, List ENV:UHC.
- Merge ports PRs 8-14 in order, then UP-Term PR 3; PR 2 after the A1200 check.
- Patrik letter (~/Desktop/uhc-message-to-patrik.md) needs a name, then send.
- Decisions: up-uhc requires vsh from 18 Oct or later? Full remove of up-uhc deletes UHC-installed packages? Release note for PR 2 (a 1.0 upgrade wipes ExtBin/Installed).

## Constraints
- Rig 8 only; `make wasabi`, then `UPTERM_RIG=8 tools/rig/rig.py start --max --purpose "vtcon-05: <task>"`; stop at the end.
- Main vtcon checkout is dirty and behind origin: never pull, stash, reset or commit there; commit through temporary worktrees.
- Ledger is shared with vtcon-a1: edit by content anchor with python.
- Worktrees: upterm-ports-right, upterm-ports-uhc, upterm-ports-newq, upterm-ports-allinst.
