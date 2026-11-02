---
title: s30 FILENAME-PEN implementation notes
date: 2026-11-02
branch: s30-filename-pen
---

# Invariant and root level (written before the first edit)

Violated invariant: Workbench draws list-view file names with the fixed DrawInfo pen 1. Pen 1 must therefore carry the theme muted
text colour so names follow every theme. Today pen 1 = DK_DARKER (222222 on vantablack), so names are near-invisible.

Levels considered:
- Pen/role map (the enum that fixes which role sits on which palette pen). Chosen: root. Every builder use of a role follows the enum.
- Recolour pen 1 at runtime only. Band-aid: BLOCKPEN, BARTRIMPEN and the font back pen would still reference pen 1 and turn muted.
- Patch Workbench drawing. Not possible, pen 1 is fixed by Workbench.

Change: enum order is now BG, MUTED(1), FG, ACCENT, SELECTION, DARKER(5), GREEN, RED. Amarchy's dark uses move to pen 5 by name.

# Pen-1 (old DK_DARKER) users, counted from the builders

desk_pens(): 3 slots (DK_BLOCK, DK_SHADOW, DK_BARTRIM) map to DK_DARKER; desk_pens_flat(): 2 (SHADOW is overridden to DK_BG).
Counted by tests/test_desk.c test_desk_filename_pen. Each now uses pen 5 by enum name.
Other users: desk_font_pens FP_BACK (pen 5 now), desk_pen_plan backdrop fallback at depth < 3, desk_dos.c backdrop pen colour (desk_darker, unchanged).
Backdrop fallback at 4-colour screens: pen 5 is off the screen, so it takes DK_MUTED (pen 1, recoloured to the dark as before). Unverified on hardware.
Stock icons outlining in pen 1 turn muted grey: accepted by owner.

# FP_SPECIAL source

prefs/font.h FontPrefs.fp_SpecialDrawMode (byte 19 of the WBFONT chunk body). The header gives no values; measured values are in
2026-10-29-amarchy-p1a-prefs-probe.md line ~104: 0 plain, 1 outline in BackPen, 2 shadow in BackPen.
New prefs key icon-text: plain (default), outline, shadow, cycle on the Flat Look page, following icon-style.

# Tests

tests/test_desk.c (test_desk_filename_pen, font mode cases, backdrop fallback), tests/test_iconpen.c (DK_MUTED == 1, DK_DARKER == 5),
tests/test_amconf.c (icon-text default/parse, flat row write/read).

# Finished (2026-11-02)

- Fold rule (owner decision): desk_pen_fold(pen, ncolors) in amarchy/desk_core.c; pen >= colour count becomes DK_BG. Applied in put_pen_tables (the one writer of both pen tables of a palette.prefs body: 4-colour table folded, 8-colour table raw) and in desk_pen_plan_for (backdrop at depth < 3 = DK_BG). The earlier "backdrop takes DK_MUTED at depth 2" is replaced.
- Goldens tests/data/desk/palette-*.prefs: no generator exists; the four palette-p1a/flat files were regenerated from desk_palette output (selection entry kept), palette-gui-save.prefs had colour entries 1 and 5 swapped. Tests assert the new order (pen 1 muted, pen 5 dark), 4-colour table folded.
- Revert proof: with the enum swap reverted the desk suite fails 73 checks (test_desk_filename_pen DK_MUTED=5 / DK_DARKER=1, goldens, backdrop); iconpen DK_MUTED==1 fails too.
- Unverified: desk_pens_reset compares the live DrawInfo array (no depth argument) with the unfolded map; on a 4-colour Workbench the live pens are folded, so the reset verdict reports "needs reset" there. Not checked on a rig.
