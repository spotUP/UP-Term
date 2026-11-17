---
date: 2026-11-13
session: s77
topic: Patina launch fix (e563c555) and v1.1 republish
status: release done; owner A1200 feed test pending
---

# s77 handoff: Patina launch fix and v1.1 republish

## Done
- e563c555 "Open Patina's new UP-Term windows through the handler's tab launch path", pushed to spotUP/UP-Term main.
  - open:uterm / rich-UI UP-Term row now opens vsh (FROM script T:Patina-uterm.NN), not a bare AmigaDOS Shell.
  - A program row (Claude, pi, Demo, tutorial, agent) with no terminal open opens ONE window running that program.
  - Evidence: thoughts/shared/s77-evidence/launch-fix-rig.md (rig 9, text checks). Owner eye check on rig 9: both PASS.
- Rig 7 and rig 9 stopped; rig 7 saved files deleted (40 MB, disk pressure).
- Stale untracked dist/Amarchy/Icons/*.dat deleted (owner OK).

## Release
- Cut gotcha found: dist-full packs build/up-term.lha as it lies. The first cut reused a 13:20 base archive with pre-fix Patina.
  Correct order: make amiga, make dist, make dist-check, rm -rf build/dist-full, make dist-full, make dist-boot (memory vtcon-release-recipe updated).
- Recut 4 logs: session scratchpad dist4/distcheck4/distfull4/distboot4.log.
- Upload: DONE, 41 assets, v1.1 Latest, feed HTTP 200, live up-term.lha matches local. Evidence file: thoughts/shared/release/v1.1-republish-2026-11-13-s77.md.
- images/nyhd2-pre-patina-2026-11-13.hdf was already gone at delete time.

## Owner to test (LIVE, A1200, feed https://github.com/spotUP/vtcon/releases/latest/download)
1. Upgrade UP-Term from the feed. Patina UP-Term row: PASS = vsh banner and `... bin %` prompt; FAIL = `1.SYS:>`.
2. Close all terminals, Agents > Launch the agent: PASS = one Claude window, no tab bar; FAIL = vsh tab plus Claude tab.

## Next session (owner decisions, ledger thoughts/spot/todos.md "S77 owner decisions" and "answers 2")
- opkg fix 5fa49b1 (branch opkg-remove-reason).
- opkg Replaces/Conflicts for renamed packages (up-hexedit/up-picocom/up-sqlite3 -> up-hexed/up-picom/up-sqlite).
- MENU_TAB_NEW with no terminal opens a vsh window (uterm_line path).
- port/htop: rebuild build/kit/userland.
- IPREFS-REQ-ON-THEME-SWITCH (desk_dos.c:863): research, then options to owner.
- Old-name worktrees vtcon-dopus-env-0c, vtcon-s48-amiberry-default, vtcon-te6c-3: check owners and unmerged work, remove merged ones.
- Dedicated test session: test_tutorial_wiring p1 bump-point failure, claude_repl timing flake, 21 old make test rows.
- terminal-unix.png 600x200: leave as is.
