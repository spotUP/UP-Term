# TE-WINDOW-REMEASURE: an open Amarchy menu window widens when a row value grows

Status: host-proven and m68k-built. NOT CHECKED ON AN AMIGA; rig proof rides a later shared rig run.

## Where a value change is handled
Every set / cycle / pick / slider ends in `list_rebuild_keep` (amarchy/gui.c, the one rebuild path; callers: go_up, set paths, watch poll).
It already re-runs `lf_setting_relabel` / `lf_setting_rechip` and then `relayout` (ChangeWindowBox). g->w (level width) was only set at open (gui.c gui_open and gui_show).

## Change
- amarchy/menu_geom.[ch]: `int mg_grow(int x, int w, int need, int screen_w, int *nx, int *nw)`; pure. Returns 1 and sets nx/nw when need (capped at screen_w) > w; nx moves left so the right edge stays on screen; returns 0 and leaves outputs untouched otherwise (never shrinks).
- amarchy/gui.c `list_rebuild_keep`: while a list window is open, calls `mg_width_t` (the SAME opening measure, same hooks, same `mg_chip_w`) and `mg_grow`; a grown width goes into g->w, then the existing `relayout` -> mg_layout places the window (centred or anchored, clamped on screen) and sends ChangeWindowBox. No second measure path.
- tests/test_keymap.c: `an_open_window_widens_when_a_value_grows` (real Theme Editor level, Screen Font set to a long name via gte_row_set_text; plus the mg_grow cases).
- Note: mg_grow's nx output is covered by the host test; gui.c takes nw only, because mg_layout already computes the on-screen x for the new width.

## Proof
NOT RUN YET (budget ended while the runtest.sh helper waited about 15 minutes in the shared lock queue, never started):
- red run (mg_grow forced to return 0), green run, and the m68k build are all open. WIP commit; the next agent runs them.

## Red/green on the final test code (main session, 2026-11-06)
The agent's first test value ("a-rather-long-font-family-name-that-grows-the-row.font 11", 57 characters) could never pass: an SD_FONT row holds at most 39 characters ("name.font/size", `char font[GT_FONTS][40]`), so `gte_row_set_text` returned GTR_BAD. Fixed in 7ea65f19 (value "a-long-font-name-that-grows-it.font/11").
- RED (mg_grow broken with `if (w2 <= w || 1)`): `RUNTEST keymap rc=2 [FAIL] keymap: 21175 checks, 9 failed`
- GREEN (fix restored): `RUNTEST keymap rc=0 [OK] keymap: 21175 checks, 0 failed`
- The host test proves the pure decision (mg_grow) and that the level measures wider. The gui.c wiring (the open window really moves and widens) has no Amiga proof yet; it rides the shared verify rig run.
