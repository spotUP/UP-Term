---
date: 2026-11-15
session: S82 (up-term-e8)
status: handoff
---
# S82 handoff: DOpus reset deadlock fixed, Installer field fill

## Landed on main (pushed, ad5dc81c)
- Installer string field fill (c4fb4a6d, d66ead4a): flat_plain_string_fill, rf_strfill before rf_gbox.
- DOpus IPrefs literal kit patch (0fdd2372): tools/dopus_iprefs_patch.py runs in `dist-dopus` after the md5 check.
  Cause and rig evidence: thoughts/shared/research/2026-11-15-dopus-theme-reopen.md.
- Upstream PR BlitterStudio/dopus5#252 (spotUP fork, branch fix-latin1-task-names), Testing section has the rig result.

## Owed owner eye checks (LOCAL rig, main build, flat on)
1. Theme pick with a DOpus lister open: PASS = no "reset the Workbench screen" requester, DOpus closes, reopens in the new pens. Pens were not measured by the agent.
2. Installer "Install DOpus5", "Where to install" page: PASS = grey input-pen ground behind "SYS:".
3. DT-11: narrow lister so the toolbar overflows: PASS = flat "<<" at the far right.

## Open
- Boot-time IPrefs reset (desk_dos.c:1505-1522 start apply) is now harmless (no requester at boot on rig 10); trace only if it costs boot time.
- PLAN=4 (DESK_START_PALETTE) persists every boot.
- s81-blockpen conflict in patina/desk_dos.c: drop seen/probe lines, keep dk_wb_windows(int splash_only).
- Rest: thoughts/spot/todos.md S82 lines and the gate list (Bug 0, 0c, W8b, A5, super+t).

## Do not touch
- thoughts/shared/research/test-register.md (other session), .claude/worktrees/agent-a81292c12d8643813, rig 81.
