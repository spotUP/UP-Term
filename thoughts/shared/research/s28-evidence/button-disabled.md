# FLAT-BUTTON-DISABLED-DIM: rig steps (rig 8, 3.2 and 3.1) - NOT RUN, host only

Cause: Intuition draws a disabled gadget (frame, label), then ghosts it with one pattern RectFill (rp->AreaPtrn set,
AreaPtSz != 0) over the gadget box, called from Intuition ROM. flat_rf_c only knew outer-edge and scroller fills, so the
dither fell through to the stock RectFill on top of the flat fill.

Invariant: with flat on, a disabled filled primary button is one solid dim tone (60 percent of the theme button fill,
ObtainBestPen) with the label in the muted pen; no dithered pixel. Level: the RectFill hook (flat_rf_c), fed by a record of
filled button boxes (flat_gh_frame, from c8_skip) and their labels (flat_gh_text, from the Text patch).

Assumption to confirm on the rig: Intuition's ghost fill is a RectFill on the same layer and task as the frame draw, with
the gadget box (or a part of it) and rp->AreaPtSz != 0. If the sentinel `ghits` does not rise, read the ghost call's rect and
rastport (rcalls rises) before changing anything.

Setup (same as thoughts/shared/handoffs/s28-evidence/button-fill-rig.md): `UPTERM_RIG=8 rig.py start --max --os 32` then
`--os 31`, `--purpose "Amarchy s28: disabled button dim"`; palette tests/data/desk/palette-flat-pen8.prefs applied first,
flat-system-look=on, flat-window-gadgets=on, build/amiga/amarchy-flat started (md5 in the report), a window with a disabled
GadTools button (the probe gwin with a button created GA_Disabled, or a Prefs editor with a ghosted gadget).

Checks, each on 3.2 and 3.1:
1. STATUS shows a dim pen: amarchy-flat prints it; the block's pen_dim (byte 22) is non-zero on a >= 8 colour screen.
2. Read pixels inside the disabled button (box interior, away from the label): ONE colour only, not equal to an enabled
   button's fill, darker than it (about 60 percent per channel). Zero pixels of the dither's second colour.
3. Label pixels are the muted pen (DK_MUTED), not the full text pen.
4. An enabled button in the same window is unchanged (pen_button fill, full label).
5. Enable the button via the program (or gadget toggle) and redraw: fill returns to pen_button.
6. STATUS: `ghits` rose by one per disabled button redraw. No Guru, no requester, rig answers.
7. A 4 colour screen (depth 2): disabled button is solid in the plain button pen (pen_line), no dither. (Fallback, no dim tone.)

Known limits (unverified): pen_dim is obtained at start and on CTRL_D; a theme change needs `Break <task> D` or a restart
to refresh it. A disabled secondary (outline) button keeps the stock dither.

Build: build/amiga/amarchy-flat md5 70aedaaeb2f686006e1040a041548704 (68020 cross-build, host flat_core 660 checks 0 failed, flat-budget PASS).
