---
date: 2026-11-08
topic: Installer 43.3 push buttons drawn as rounded flat fills (INSTALLER-BORDER-BUTTON)
status: done
---

# Installer rounded buttons: fix proof

## Root cause

The Installer's buttons (gadget ids 90, 91, 1, 100; 238x16) are custom gadgets with a struct Border as
GadgetRender and a different Border as SelectRender (GADGHIMAGE, no GadgetText). The Installer draws each one as a
pen-0 RectFill of the box, then the label (Move + Text), then a temporary frameiclass object (OM_NEW, IM_DRAWFRAME,
OM_DISPOSE). c8_skip recognised push buttons only as render == select == frame object, so the temporary object
never matched: fill was 0, and amarchy-flat drew its own edges-only 1 px outline (the square pen-3 ring). A fill at
frame time could not work: the label is already drawn.

## Fix

- amarchy/flat_core.c: flat_border_button (the gadget rule), flat_box_exact (exact box match),
  flat_border_button_ids (GFLG_SELECTED to IDS_SELECTED).
- amarchy/flat.c border_button_at: the window's border button whose box equals the drawn rectangle, with its
  row role (flat_button_role). GZZ windows are skipped.
- amarchy/flat.c flat_rf_c: the pen-0 erase of such a box is drawn by raa_frame_draw with fill=1 (the same rounded
  fill as every flat push button), the label Text patch armed through flat_tx_arm, the box recorded for the
  disabled ghost (flat_gh_frame); counter bhits. bb_task keeps the fill's own RectFills stock.
- amarchy/flat.c c8_skip: the IM_DRAWFRAME over that exact box draws nothing (no ring) and disarms the label arm.
- b7377f75's push_gadget_of / flat_frame_fill path is kept: it serves edges-only frames of render == select
  push buttons, which the Installer buttons are not.

## Host test

tests/test_flat_core.c, INSTALLER-BORDER-BUTTON block (17 checks). Before the implementation: link failure on the
three new functions (rc=2). With the `render != select` term removed: 1 failure at test_flat_core.c:1033. After:
`[OK] flat_core: 2722 checks, 0 failed`.

## Rig 95 (sys.hdf, Workbench 800x600 depth 24, tokyo-night pens, Installer "Install DOpus5")

Window body 1a1b26. Pixel offsets inside each 238x16 button box.

| button | corner 0,0 | corner 237,15 | top 6,0 | top 119,0 | left 0,8 | fill | label pixels | ring px (7aa2f7 on edge) |
|---|---|---|---|---|---|---|---|---|
| 90 (primary) | 1a1b26 | 1a1b26 | 7aa2f7 | 7aa2f7 | 7aa2f7 | 7aa2f7 | ~218 in 1a1b26 (226 less 8 corner) | 0 (fill is the accent) |
| 91 (secondary) | 1a1b26 | 1a1b26 | 00ff9f | 00ff9f | 00ff9f | 00ff9f | 142 in a9b1d6 | 0 |
| 1 (primary) | 1a1b26 | 1a1b26 | 7aa2f7 | 7aa2f7 | 7aa2f7 | 7aa2f7 | ~72 in 1a1b26 | 0 |
| 100 (secondary) | 1a1b26 | 1a1b26 | 00ff9f | 00ff9f | 00ff9f | 00ff9f | 62 in a9b1d6 | 0 |

Anti-aliased corner pixels: 8 per button (3e4254 / 4473ff).

Pressed (button 91 held with the left mouse button): fill a9b1d6, label 142 pixels in 1a1b26, corners 1a1b26. Pointer
moved off and released: back to fill 00ff9f, label a9b1d6. Intuition re-runs erase + label + frame on press, so the
rf branch alone handles both states (bhits 4 to 7).

STATUS rectfill line: before the Installer `calls=24 hits=0 bhits=0`; after it opened `calls=305 hits=10 bhits=4`;
after the press `calls=723 hits=10 bhits=7`.

Unchanged elsewhere: Font Preferences GadTools buttons (ids 2, 8, 9, 11, 12, 13), md5 of each button's pixels with
the HEAD build and with the fix: 6 of 6 identical (1e484a8293d4, 04b262a29f0b, aba65423d561, 2d5d7db068e0,
e03e0ef05f4f, 86d303812610).

PNG: scratchpad flatbuttons/installer-fixed.png (session scratch, not committed).

## Open

- The disabled (ghosted) state of a border button was not measured; flat_gh_frame records the box, so the
  dither path should apply.
- Not run on AmigaOS 3.2 ROM (no 3.2 tree on this host for --os32) nor 3.1 light theme.
- flat-budget covers the c2 dispatcher only; the rf branch runs on the caller's stack (flat_state copy + raa_frame_draw).
- Secondary label a9b1d6 on 00ff9f has low contrast; GadTools secondary buttons look the same (pen_button = 4).
