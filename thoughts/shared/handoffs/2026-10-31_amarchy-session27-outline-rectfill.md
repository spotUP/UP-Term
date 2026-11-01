---
date: 2026-10-31
session: amarchy s27
topic: flat C9 desk apply, text grey, window outline via RectFill patch, rounded corners request
---
# Amarchy session 27 handoff

Ledger: thoughts/spot/todos.md, s27 rows at the end (grep "s27"). Evidence copied to thoughts/shared/handoffs/s27-evidence/
(border-hunt.md = who draws window edges on 3.1/3.2; bprobe/ = RectFill/Move/Draw logging probe source).

## Landed on origin/main
- cf78c357 + 79f2d11a FLAT-C9: desk apply writes flat_state from prefs and refreshes frames (win_refresh.c). C9 rig case NOT run yet.
- 5e55c7fe THEME-TEXT-GREY: dark theme fg #ffffff loads as #BEBEBE. Not seen on a rig yet (expect 190,190,190).
- 963d47ed FLAT-OUTER-EDGE-RECTFILL: Intuition repaints the 4 outer border edges LAST in every redraw as four 1 px
  RectFill calls (3.1 V40.85 and 3.2 V47.53, measured). graphics.library RectFill (LVO -306) patch, stub amarchy/flat_rf.s,
  recolours only exact outer strips of bordered windows when the caller is in Intuition ROM (FindResident range).
  Rig 8 3.2: 857/857 and 677/677 ring pixels, 0 with flat off. STATUS line "rectfill installed= calls= hits=".

## Open, in order
1. 3.1 rig check of 963d47ed (same steps; rig start [REFRESH] copies main checkout build/amiga/amarchy-flat over VTC:,
   so either rebuild in a worktree and run as RAM:af-rf, or check what the main build holds). Also resize, depth change,
   simple-refresh damage cases, and tests/test_amarchy_stack.py (needs build/amiga/amarchy-menu).
2. ROUND-CORNERS research (owner chose): one opus agent, 30 calls, no code: any 3.2 layers shape/cliprect support, cost
   of a layers patch leaving corner regions to the layer below on 3.1. Answer feasible yes/no + size, then park and ask owner.
   Owner REJECTED fake corners painted in backdrop pen (1 or 2 px ring) and keeping square.
3. C9 rig case on rig 8: tools/rig/amarchy_gui_rig.py --setup; UPTERM_RIG=8 tools/rig/amarchy_desk_rig.py --flat.
   Then remove worktree .claude/worktrees/c9 (clean, merged).
4. FLAT-BUTTON-FILL (owner spec in ledger). Deferred: FLAT-TITLE-ACCENT + inactive title dim.
5. Ask owner: delete local branch flat-outline-last (6e3f5e1f, unmerged, superseded by 963d47ed).

## Gotchas
- 3.2 boot flag is `--os 32`. Start rigs from a foreground Bash call (background shell kills FS-UAE).
- Never ReadPixel or other heavy calls under LockIBase: deadlocks 3.2.
- Rig 8 after restart came up on stock theme (backdrop 170), not vantablack; pen 3 = 102,136,187 there.
- Disk 8.3 GB free; space is used outside this repo (worktrees < 1 GB). Single make targets only.
- Agents overran budgets (61 calls on 30). Keep briefs tight.
