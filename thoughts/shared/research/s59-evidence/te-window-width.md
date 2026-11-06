# TE-WINDOW-TOO-NARROW: Theme Editor window cuts rows

Status: fixed on branch te-width, host-proven and m68k-built. NOT CHECKED ON AN AMIGA; RIG PROOF OPEN.

## Invariant
The window is wide enough that no row of any level it shows needs an ellipsis, as long as the screen is wide enough.

## Root cause
`mg_width_t` / `width_level` (amarchy/menu_geom.c) sized the window from the plain tree labels (label + icon + chevron).
The painter (amarchy/gui.c) shows "Label: value" (via lf_setting_relabel) and, for colour rows, a chip that takes
`row_h - 4 + 4 + row_pad` pixels off the text width. Neither value nor chip was in the width.

## Failing rows (before), 8x8 font, settings.theme with nord, Advanced mode
Window 360 px (room 280 px per row); widest row needs 464 px; 61 rows cut. Examples (needs vs room 280):
- Background Hover: Follows darker(background) 352 (short 72)
- Title Bar, Inactive Window Disabled: Follows its group 432 (short 152)
- Window Border, Inactive Window Disabled: Follows its group 464 (short 184)
- Gadget Glyph, Inactive Window Pressed: Follows its group 448 (short 168)
- Clear Override: Title Text, Inactive Window 344 (short 64)
After: window 544 px, widest row 464 px, 0 rows cut (wide screen 1280 and 640 screen).

## Changes
- amarchy/menu_geom.h/.c: hooks `mg_relabel`, `mg_rechip` (null = old behaviour); `mg_chip_w()` is the one chip-size function; `width_level` measures the relabelled text plus chip.
- amarchy/gui.c: sets the hooks after lf_init (the only program that measures widths; leaf.c is linked by programs without menu_geom.c, so the hooks are set in gui.c); the painter's text cut uses `mg_chip_w`.
- tests/test_keymap.c: `the_theme_editor_window_fits_its_rows`. Also asserts "apps" and root widths are unchanged with hooks set (label-only levels keep their old width).
- No new file, no Makefile change.

## Proof
Red (width_level ignoring hooks, via `if (0 && ...)`): `RUNTEST keymap rc=2 [FAIL] keymap: 21153 checks, 1 failed` (window 360, 61 rows cut).
Green: `RUNTEST keymap rc=0 [OK] keymap: 21153 checks, 0 failed`.
m68k: `make build/amiga/Amarchy build/amiga/amarchy-gui build/amiga/amarchy-menu build/amiga/amarchy-index build/amiga/amarchy-icons build/amiga/amarchy-flat build/amiga/upprefs` rc=0.

## Rows that cannot fit at 640
None: 544 px window fits a 640 screen. Open: a value that grows after the window opens (a longer font name, a pen change) is not remeasured; owner decides.
