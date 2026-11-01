---
date: 2026-11-01
topic: THEME-PICK-MENU-STABLE - second theme pick fails "open menu: no stable state within 60s"
status: rig-tool race fixed; one product question open (menu level after a pick)
---

# THEME-PICK-MENU-STABLE

## Verdict

A rig-tool race, not a product hang. Both pick paths closed the menu while Amarchy was still applying the theme.

- `desk_dos_apply` hides the menu window (`amarchy/gui.c` `gui_hide`, wired as `desk_dos_own_close`) while the palette is written, and opens it again (`gui_show`) when the apply returns. The theme list recolours about 5 s before that.
- `amarchy_theme_save_rig.one_select` did not wait for the apply at all. `amarchy_desk_rig.pick` waited (19888120), but only AFTER `g.close_menu()`.
- `close_menu()` during the apply finds no "Amarchy" window, presses nothing and reports closed. The apply then shows the menu again. The next pick's Left Amiga + Space toggles it shut (MENU-TOGGLE), and `wait_stable(what='open menu')` sees no window for 60 s: "open menu: no stable state within 60s (last None)".
- Host load was normal during the hunt (fs-uae rig 10 about 100 % of one core, no test runner).

## Fix

- `tools/rig/amarchy_theme_save_rig.py`: `APPLY_DONE`, `APPLY_SECS`, `wait_apply_done()` live here now. `one_select` deletes `ENV:AMARCHY_WBCLOSES` before the pick and waits for it (written by `lf_gt_select`, `amarchy/leaf.c`, once the apply returned) before it reads prefs or closes the menu.
- `tools/rig/amarchy_desk_rig.py`: `pick()` reuses them and waits for the apply BEFORE `close_menu()`.
- `tests/test_amarchy_desk_rig_pick_waits.py` (`make test ONLY=desk_rig_pick_waits`): a fake rig hides the menu during the apply and shows it after; asserts both paths end with the menu closed. Fails with either tool change reverted.

## Rig 10 evidence (AmigaOS 3.2, 2026-11-01)

- Original tool (`origin/main`), `--selects 1-2`:
  - `[FAIL] select 1: rows after the pick do not mark 'white (current)': ['Amarchy theme', 'Terminal']`
  - `[FAIL] select 2: ENV/ENVARC differ or unreadable: {'ENV:Amarchy/prefs': '', 'ENVARC:Amarchy/prefs': ''}` - prefs read while the apply still rewrote them.
- Fixed tool, `--selects 1-2`: both selects get through open menu, pick and apply wait; the menu is closed at the end (`windows` shows only "Amarchy bar" and "Workbench"). Both still fail the rows check (next section).
- The exact "open menu: no stable state" text did not reproduce in these two runs; the race that produces it is shown by the fake-rig test and by select 2's unreadable prefs above.

## Open: the menu leaves the theme list when a theme is picked

Driven by hand on rig 10 (Style > Amarchy theme, Down, Return):

- before the pick: `AMARCHY_ROWS` = `catppuccin|catppuccin-latte|ethereal|...`, `AMARCHY_SEL` 19
- 1 s after Return: `AMARCHY_ROWS` = `Amarchy theme|Terminal`
- after `AMARCHY_WBCLOSES` was written: `Amarchy theme|Terminal`, `AMARCHY_SEL` 0, the "Amarchy" window open

So the menu is on the Style level after a pick, not on the recoloured theme list. `one_select` expects the theme list with the new row marked "(current)". Either the product changed (candidates: 3fd58afc gui_show rebind, fe80eec4 WBRESET-EXT, 30590aa8 WA_WBenchWindow) or the tool expectation is stale. Owner decides which is intended. FLAT-OFF "Down to Style" timeout likely shares the menu-close race fixed here.
