# THEME-PICK-MENU-GONE fix

Cause: thoughts/shared/research/s59-evidence/menu-gone-cause.md (vtcon-s58-integ).

## Invariant
The menu stays open across a theme pick. A launcher still closes when another program takes the focus. The focus rule must not
count the Keep/Revert requester that this very pick started as another program.

## Level of the fix
A pure focus-policy state machine `mg_focus` in amarchy/menu_geom.[ch] (linked into amarchy-gui and the keymap suite).
gui.c holds it instead of the bare `active_seen` flag. The pick sets the latch where it returns to the server (compares
`desk_dos_confirm_asks` before and after lf_run / lf_open_index). IDCMP_INTUITICKS drops a latch no requester used, via the new
`desk_dos_confirm_pending()` wrapper over the existing `dk_pending_mask` (no second reader). desk_dos apply order, requester,
gui_hide, gui_show unchanged.

## Reused
mg_grow style (pure geometry/policy in menu_geom), `slurp` source check in tests/test_keymap.c, `desk_dos_confirm_asks`,
`dk_pending_mask`.

## Test
tests/test_keymap.c `the_menu_stays_open_when_its_own_confirm_takes_the_focus`.

## Results
- RED (mg_focus_inactive stubbed to ignore own_confirm): `RUNTEST keymap rc=2 [FAIL] keymap: 21189 checks, 2 failed`
- GREEN: `RUNTEST keymap rc=0 [OK] keymap: 21189 checks, 0 failed`
- m68k build of Amarchy, amarchy-gui, amarchy-menu, amarchy-index, amarchy-icons, amarchy-flat, upprefs: rc=0 (only pre-existing warnings).
- tests/test_make_link_lists.py: OK.

## Rig check (not run here)
1. Pick a theme in Style > Theme; the requester "Amarchy desktop theme" opens. Press Keep: the "Amarchy" menu window must still exist.
2. After the requester closes, the menu window should become active again; then a focus change to another program must still close it
   (the latch was consumed by the requester's focus loss).
3. Pick the theme that is already active (no requester opens), then click elsewhere: the menu closes (tick drops the unused latch).
