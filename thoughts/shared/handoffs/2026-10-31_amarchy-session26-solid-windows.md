---
date: 2026-10-31
session: amarchy s26
topic: flat UI solid windows, block glyphs, flat-state wiring gap
---
# Amarchy session 26 handoff

## Landed on origin/main
- b60dd2de block glyphs only (FLAT-SOLID-FILL b): line style, key flat-glyph-style and prefs row removed.
- 2ac8209e, 518da41e, 0b708b1b (FLAT-SOLID-FILL a): desk_pens_flat SHINE=SHADOW=FILL=DK_BG, FILLTEXT=DK_FG;
  c2_draw frame_pen BACKGROUNDPEN both states; desk_flat_glyph_pens (active DK_FG, inactive DK_ACCENT, back DK_BG).
  Host desk 1747/0, flat_core 445/0, amconf 745/0, fail-on-revert shown. Rig 8 3.2 shot ~/Desktop/flat-solid-32.png
  was taken BEFORE 518da41e (active title invisible there).
- Earlier s26: AM-BACKDROP-DOTS, FLAT-D2/D3, BAR-NET-CLICK, FLAT-M2 table, FLAT-P1-2/P1-3, FLAT-GADGET-BG.

## Next, in order (ledger thoughts/spot/todos.md, top of "## Open bugs and features")
1. FLAT-C9 WIRE (blocker for everything visible): the product never writes Amarchy.Flat flat_state enabled/pens; only
   tests/amiga/flatwin.c does. Row has the design: one pure desk_core function (prefs + theme -> flat_state via
   desk_flat_glyph_pens) called by desk apply AND amarchy-flat BOOT; reachability via DESK THEME in amarchy_desk_rig.py
   without flatwin. Add plan row C9 before X1. One sonnet implement agent, worktree from origin/main.
2. After C9: rig 8 measurement 3.2 + 3.1 through the real theme path: title text active/inactive, listview selected row,
   menu highlight, MX, cycle, prop knob (FILLPEN=BG may hide them). Inactive title is probably TEXTPEN (unmeasured): if
   so it cannot dim by pens -> owner question (AskUserQuestion).
3. FLAT-SCROLLER knob shade (propgclass hook), TAB-FILLPEN (UP-Term session row), FLAT-31-G1, M1/M2 residue, C6, D4, X1,
   AM-FLAT-WB, R2.16/R2.17 (A1200 last).

## Gotchas
- Solid agent hit rig 1 once for a read-only grab with UPTERM_RIG unset: every brief must say export UPTERM_RIG=8.
- make test ONLY=desk is ASAN, ~20 min: run in background.
- Agent budget: split rig runs from code work; the solid agent ran ~80 calls.
- Disk 9.4 GB free: no full make amiga.
- Pre-existing failure: tests/test_rig_a500_user_startup.py::test_start_strips_on_both_clone_paths.
