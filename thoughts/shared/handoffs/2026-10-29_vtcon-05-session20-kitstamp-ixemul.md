---
date: 2026-10-29
session: vtcon-05 session 20
status: handoff
---
# vtcon-05 session 20: D10 68000, ixemul merge + release, rig kit stamp

## Done
- D10 68000 add-on fix pushed (47f51b27, c3bece54, plan 49b03330); rig 9 17 of 17.
- ixemul-vtcon: feature/ixemul-80 merged into main as 8b0b4da; release v80.1-vtcon-1 with the FPU and soft-float
  68020 ixemul.library and ixnet.library (https://github.com/spotUP/ixemul-vtcon/releases/tag/v80.1-vtcon-1).
- upterm-ports patch 0060: PR https://github.com/spotUP/upterm-ports/pull/25 (not merged; owner asked push + PR only).
- RIG-SLOW-BOOT: warm reboots skip the kit shadow pass while VTC:kit-shadowed matches VTC:kit-hash
  (origin/main 0ac35afa, tools/rig/kitstamp.py, tests/test_rig_kit_stamp.py). Rig 9 live check passed.

## Open, in order
1. Local main has diverged (37 local-only, 117 behind origin). Owner decision: wait until vtcon-32 commits
   thoughts/shared/plans/2026-10-28-omarchy-ai.md, then make a backup branch and rebase local main onto origin/main.
   Never stash that file.
2. tests/test_rig_kit_shadow_keymap.py test_install_stages_dist_keymap_and_refreshes_apps_bin fails on HEAD:
   build_drift lists Apps/bin/md5sum twice.
3. tests/test_amarchy_docs fails on origin/main (vtcon-32 area): install.dos misses the Copy Amarchy line.
4. Ledger items still open: ColdReboot loop after INSTALLER-END-CRASH (todos ~3263), KILL-BUS-ERROR, rig 11 flakes,
   stale curl on rigs 3/4/9, IPrefs System Request at --fresh boot (text unread), ir.Before() base rununinstall on 68000,
   rig "A1200" ROM is the 40.63 A500 ROM (note at todos ~1460).
5. Uncommitted debug FS-UAE patch in /Users/spot/Code/Emulators/fs-uae-debug.

## Gotchas
- Do not commit other sessions' dirty files (see git status: omarchy-ai plan, session10 handoff, job-view research,
  session13/14 handoffs, untracked "-", scratchpad_info, scratchpad_w.log).
- Push vtcon via a detached worktree on origin/main + cherry-pick while local main diverges.
