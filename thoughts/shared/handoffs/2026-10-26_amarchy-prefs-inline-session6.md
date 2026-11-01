---
date: 2026-10-26
session: vtcon-a1 (Amarchy only)
topic: Amarchy prefs-inline, session 6
status: in progress
---

# Handoff: Amarchy prefs-inline, session 6

Plan: thoughts/shared/plans/2026-10-26-amarchy-prefs-inline.md (untracked in the main checkout).
Ledger: thoughts/spot/todos.md (rows AM-PREFS-*, AM-THEME-SAVE-RIG).
Worktree: .claude/worktrees/prefs-a1, branch prefs-inline (origin at 94153060 after M6).

## State
- Done: T1-T4, M1-M6, C1. M5 e1c1f8c3, M6 94153060, theme-save rig driver 34299a4f (5/5 OK on rig 7).
- Open: M6b, M7, M8, M9, M10, R1, R2.
- Running when written: M7 agent (prefs-a1, tests/test_amconf.c) and M9 agent
  (worktree .claude/worktrees/prefs-m9, branch prefs-m9, dist/keymap + tests/test_keymap.c).

## Next steps, in order
1. M9 reports: merge origin/prefs-m9 into prefs-inline (after M7 has pushed, so the two do not race on
   the branch), run suites amconf + keymap, push, tick M9, remove worktree prefs-m9 and branch
   prefs-m9 (local and remote). It is this session's worktree.
2. M8 (colour chips, text-menu hex) and M10 (amarchy-menu links prefs/amconf; make amiga) as the next
   two agents. M6b (fallback font row, requester screen, mode name label) after them, small brief.
3. R1: one sonnet agent writes the driver, a second run step. Sentinel design is in the plan under R1
   (blink sampling before/after/toggle-back; ENV+ENVARC md5 and cursor-blink value; assert the picked
   theme name; also exercise the M6 requester rows). Reuse own_prefs_rig.py check(),
   amarchy_menu_rig.py (text menu via Left Amiga + Space), amarchy_theme_save_rig.py (wait_stable, step).
   Rig 7 only (never 1, 2, 8): `UPTERM_RIG=7 tools/rig/rig.py start --max --purpose "vtcon-a1: R1 prefs inline"`.
   Budget ~35 calls per agent; the last rig agent overran at 71.
4. R2: A1200 deploy with .bak backups, md5 drift 0, owner test steps.

## Known follow-ups (not in the plan checklist)
- AM-PREFS-M3-FOLLOWUP: amarchy-menu has no Right key; "Label: Value" clipping.
- C1-FOLLOWUP: profile labels not refreshed.
- Empty answer cancels ask_value, so a string row cannot be cleared from the menu.

## Gotchas
- prefs-a1 has an untracked `vendor` symlink and an untracked research file that is not ours: never commit them.
- Rig 7 had no Themes drawer; the theme-save driver stages dist/amarchy-themes itself.
- Disk is tight (about 2.7 GiB free); build/rig7 is kept for R1.

## Update (later in session 6)
- M9 done at 6c0fa6f6 on origin/prefs-m9, not merged. Merge into prefs-inline after M7 pushes, run ONLY=keymap and amconf, push, tick M9, remove the prefs-m9 worktree and branch.
- M9 used `settings.amarchysettings` ("Amarchy settings") because `settings.amarchy` already holds the old rows. Owner chose one entry instead: new plan item M9b (nest the page rows in the old group).
- M8 agent running in worktree .claude/worktrees/prefs-m8, branch prefs-m8 (pushes there only; merge after M7).
- Roadmap decisions 11 (auto-tile off until toggled) and 12 (roadmap starts after prefs R2) written into the roadmap plan.

## State at session end (prefs-inline c0d6d486 on origin)
- Done and merged: M7 (6a2a2a52), M8 (b6399cc4), M9 (6c0fa6f6), M10 (7069a3d3). Merge commit c0d6d486. amconf 337/0, keymap 11185/0, wbrun + amarchy-menu + amarchy-gui build. prefs-m8/m9/m10 worktrees and branches removed (local and remote).
- Plan: 15 of 19 done, 4 open: M9b, M6b, R1, R2.
- Next: two sonnet agents in parallel, each in its own worktree from origin/prefs-inline, pushing their own branch; main thread merges.
  - M9b (dist/keymap, dist/menu-icons, amarchy/menu_tree.c mn_children, tests/test_keymap.c). Also give tests/amiga/wbrun.c `__stack` and the `$STACK` cookie (it has neither).
  - M6b (leaf_core.c fallback font row, prefs_asl requester screen, screen-mode label name).
  - Then R1 on rig 7 (driver agent + run agent), then R2 on the A1200.
- Watch: M8 chip pens use vr_pen_obtain/ReleasePen per row draw, no cache; check on low-colour screens during R1.
- Roadmap waits for R2 (owner decision 12).
