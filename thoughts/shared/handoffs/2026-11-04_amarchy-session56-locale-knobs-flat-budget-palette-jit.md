---
date: 2026-11-04
session: Amarchy vtcon-16 (s56)
status: handoff
---
# Amarchy session 56 handoff

## Done (all on main, pushed)

- S56-LOCALE-DRAGBAR-COLOUR + S56-LOCALE-CANCEL-CORNERS: fix 85bdf561 / merge 7e51272c. SMARTKNOBIMAGE listview knobs (drawn via frameiclass IM_DRAWFRAME) are now found by `smart_knob_of()` in amarchy/flat.c (walks win->FirstGadget, `flat_smart_knob()` in flat_glyph.c) and drawn with `flat_knob_frame_draw` instead of pen 0. Tall-button corners: `flat_frame_draw_round` paints the inner diagonal pixel in pen_text for SECONDARY role with n > 4. Evidence: s56-evidence/locale-prefs-knob-corners.md.
- S56-FLAT-BUDGET-RED: merge 6a682293. Aspect lookup moved to `aspect_update()` on the main task (pens_run), per-screen slot table in flatshared (pure helpers flat_aspect_find/need/put in flat_core.c, test block FLAT-ASPECT-SLOTS). `flat_clone_draw_in` takes a struct; c2_draw split. `make flat-budget` PASS but deepest chain _c2_disp_reg 384/384 = ZERO headroom.
- Rig 85 smoke check of 6a682293 vs 7e51272c: same pixels (commit 1a8d35f9). XCON inactive knob pen 251 (= STATUS inbar) vs evidence 250, read as same role.
- FSUAE-31-PALETTE-UPSTREAM: lldb on stock FS-UAE v3.2.35 = host SIGSEGV at the opcode fetch in m68k_run_2 (newcpu.cpp:5241) after the RTG-to-native switch (c1d3a362). Amiberry (JIT on) also loses the guest in 5 of 6 Copy boots, green screen, host alive, JIT "Error in compiled code" stores pc=00f8198a (cb830707). Evidence: s54-evidence/fsuae-31-palette-upstream.md.

## Open (in this order)

1. Palette Copy crash: CLOSED. Mechanism found (0edf1639): IPrefs 40.7 applying a 3.2-saved palette.prefs drives ROM intuition into exec AddHead on a NULL-based list, writing $B2 into $4 (ExecBase); next interrupt jumps wild (JIT on = host SIGSEGV). Splits (1f33762b): needs IPrefs + those bytes, not SolidWin, not the RTG screen; a 3.1 Palette editor Save does not trigger it. This is the known DESK-31-CRASH, already gated in Amarchy (desk_palette_editor_ok >= 47). No emulator report (owner s56). fs-uae-stock worktree removed.
2. Owner eye check on rig 96 (7e51272c): new-shell knob, Locale listview knobs, Cancel corners. Rig 96 [WARN] from the install: DOpus 5.104 not installed, updemo files missing, WB stayed PAL.
3. S56-LOCALE-SAVE-FILL (unverified): the Save button fill may be pen 0 at depth 8. Check it with one pixel dump on the next shared rig run.
4. Flat budget has zero headroom. The next frame on the draw path breaks it, so trim before adding.

## Ledger

thoughts/spot/todos.md rows: S56-LOCALE-*, S56-FLAT-BUDGET-RED, S56-LOCALE-SAVE-FILL, FSUAE-31-PALETTE-UPSTREAM.
