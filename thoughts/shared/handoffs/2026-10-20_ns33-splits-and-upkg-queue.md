---
date: 2026-10-20
topic: Native splits NS3.3 (last open item) + 3-pane freeze; queued upkg patches 0012/0013; owner's bash-first rule
ledger: vtcon thoughts/spot/todos.md, sections "2026-10-20 late: NS4.3" to the end
plan: vtcon-splits thoughts/shared/plans/2026-10-19-native-splits.md (18 of 19 ticked; NS3.3 open)
from: session up-term-45, handed to uppkg-addon-installer (owner: "we don't need two")
---

# Handoff: NS3.3 hunt, upkg 0012/0013 queue

Read the ledger sections named above first; trust it over this file if they differ.

## 1. Native splits (repo /Users/spot/Code/UP-Term/vtcon-splits, branch feature/native-splits, NOTHING pushed)
- HEAD 68ea2a87. Ahead of origin/main by 63, behind 19. Rig 4 ONLY, never rig 2.
- Open plan item: NS3.3 pane close (rare SIGWINCH node leak after a pane exits; release blocker). The owner's
  release rule: all 19 items ticked, NS3.3 leak fixed first.
- What is known (thoughts/spot/crash/ns33.md, paneleak.md, leakloop.md):
  - Invariant: a con's "vtcon SIGWINCH" node is on input.device's list only while c->w.win is a live window,
    and comes off before the window closes or the con is freed.
  - Ruled out by code: post_sizewindow ignoring winch_closing (nothing calls h_resized on that stretch; the
    retired host has w.win = 0; node removal precedes TM_UNREGISTER and CloseWindow). Test race ruled out too.
  - Still open, unproven: open_window re-opening a window on a con with w.win == 0 (READ/WRITE/DISK_INFO paths
    handler/vtcon_handler.c :7261/:7291/:7638 reset winch_closing at :2836); or the old probe itself.
  - Rate: 1 leak in ~31 earlier runs. This session: pane_close 40 new rounds (43 recorded), 0 leaks.
- Freeze (separate bug, not pane close): g5-g.log shows the rig stopped answering with 3 panes LIVE (left pane
  printing Date each second), at teardown's first window lookup, before any exit. Cause unknown, no taskdump yet.
- New rig tooling (commit 68ea2a87): tools/rig/capture.py; splits_rig.py writes fail-<case>-<n>.txt (inputlist with
  is_Data, taskdump, UITREE, UPTerm state) when a SIGWINCH node survives, hang-<case>-<n>.txt (taskdump, UITREE,
  serial.log tail) on a silent rig call, then restarts the rig. `--repeat N [--fresh-boot]`, resumable, stops at
  first FAIL/HANG. Output dir: /Users/spot/Code/UP-Term/vtcon/build/rig4/splits/ (paths.RIG), logs repeat-<case>.log.
  Known gaps: "UPTerm state" in a fail file reads "can't open RAM:cnt.out" (windows gone); the hang path is
  unexercised.
- LOOP STATE AT HANDOFF: pane_close done 43/43 PASS. pane_scroll running (round 18 of 42 PASS) as a background job
  of the old session; it may die with that session. Check:
    tail -3 /Users/spot/Code/UP-Term/vtcon/build/rig4/splits/repeat-pane_scroll.log
    ls /Users/spot/Code/UP-Term/vtcon/build/rig4/splits/ | grep -E '^(fail|hang)-'
    pgrep -fl splits_rig.py
  If it stopped short, resume (recorded rounds are skipped):
    cd /Users/spot/Code/UP-Term/vtcon-splits && python3 tools/rig/splits_rig.py 4 --only pane_scroll --repeat 42 > thoughts/spot/crash/loop-scroll.log 2>&1
- Next:
  1. A fail-/hang- file exists: read only that file; brief one agent (opus for cause) with it. Fix at the cause,
     regression proof that fails with the fix reverted, then tick NS3.3.
  2. No failure in 80 rounds: the leak does not show under plain loops. Next trigger to try: `--fresh-boot`
     rounds of pane_close (the only observed leak was the first case after a fresh boot), and a trace of
     open_window after host_retire. Freeze: more pane_scroll rounds until the hang capture fires.
  3. Do not tick NS3.3 without a reproduced-and-fixed leak or an owner exemption.

## 2. upkg (repo /Users/spot/Code/UP-Term/upterm-ports, branch test/upkg-width, HEAD b06d17d)
- HOLD: no main push, no dist/upload, no build/ changes until the owner's v1.2-test1 PASS/FAIL (A1200).
- Queued after the verdict, in order:
  - 0012: `upkg install` exits 0 after "Couldn't close //UP-Term/pkg/upkg/status: No space left on device";
    propagate the opkg_conf_write_status_files failure to a non-zero rc. Host test with a status file whose
    write/close fails (failing before the patch). Evidence: rig 2 report.md ~:759 (path in the ledger).
  - 0013: `?`, `-h`, `help` print the --help usage text and exit 0 (now: `?` rc 1 "unknown sub-command ?",
    `-h` rc 1 "invalid option -- h", `help` rc 1 "unknown sub-command help"). One regression test per form.
- Then commit both on test/upkg-width and record the hashes in the ledger.

## 3. Owner rule (2026-10-20, applies to vsh and every port)
"Full bash compatibility and as much Amiga compatibility as possible: a Unix for Amiga users, a hybrid.
Bash wins: if something in bash breaks Amiga, that may be fine; if Amiga breaks bash, that is not fine."
- So the vsh "lone `?` is always literal" change (shell/sh_expand.c) is REVERSED: do not land it; if committed,
  `git revert` it. vsh keeps bash globbing; a lone `?` with no match already reaches the program literally.
- Amiga forms belong in the programs (upkg accepting `?`), not in shell semantics.
- 0013 docs: `upkg ?` in AmigaShell; in vsh `upkg help`, `upkg -h` or `upkg '?'` (bare `?` works unless a
  one-character file name exists in the current directory).
- Any deliberate shell break from bash needs an owner decision, a dist/README.txt "Differences from bash" line and
  a tests/bash/divergences.txt entry.
