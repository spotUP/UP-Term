# DOpus rig rows D5 D6 D7 D22 (rig 5, 2026-10-31, session vtcon-05 s39)

Rig 5 started with current deps (start refreshed handler, keymap, Apps/bin programs; only [WARN] was "Directory Opus 5.104 not installed in VTC:", the rig runs 5.82). Rows run with `UPTERM_RIG=5 python3 tools/rig/amarchy_menu_rig.py --only <row>`.

- D6 PASS: `dopus_present` 3 of 3. Super+Shift+D opens one new window "Device List". The earlier FAIL (km-hits 0) was the harness (rig shell path, fixed earlier by r_start `Path SYS:Rexxc ADD`), not the product.
- D7 PASS, weakly: Open UP-Term here opens one "UP-Term" window. The check asserts only the window title. With DOpus 5.82 as the Workbench replacement, the rig has a single screen ("Workbench Screen"), so "on the DOpus screen" is trivially true; a separate DOPUS.1 screen was not tested (needs 5.104 add-on rig).
- D5 PASS after a harness fix: `dopus_absent` on a `--no-dopus` rig, 11 of 11 (nine scripts RC 5, window count unchanged, no Dopus root group). Before the fix, `--only dopus_absent` on a fresh rig FAILED the last check: r_dopus_absent called open_terminal (no Amarchy commodity started, and over the UP-Term window the default menu-in-terminal = text opens amarchy-menu, not the "Amarchy" window), so root_menu_rows got no window. Fix in tools/rig/amarchy_menu_rig.py (r_dopus_absent): ensure_terminal() + x9_drawn() + RELOAD, as r_dopus_present does. Regression test tests/test_menu_rig_dopus_absent_commodity.py (make test ONLY=menu_rig_dopus_absent), fails on the old code.
- D22 NOT MEASURED. No row exists in amarchy_menu_rig.py for the vanish-on-quit half (grep for quit finds only a menu-path check at line 849). DOpus 5.82 cannot be quit by `dopus quit` over ARexx on the rig (earlier finding); the quit half needs either a DOpus quit command that 5.82 honours or a row that kills the DOPUS.1 process, then re-reads root rows. Not guessed.
- `rig.py install` without build/amiga/vsh: YES, still crashes. FileNotFoundError from shutil.copyfile(vsh, dest/"bin/vsh") in stage_upterm (tools/rig/rig.py:1187), called from _install (rig.py:1521). The other programs in stage_upterm are guarded by is_file; vsh is not. Not changed: a guard would hide a missing vsh in the drawer, owner call. The handler copy and refresh_programs ran before the crash.

## D22 quit row (s39)

- D22 PASS on rig 5 (DOpus 5.82): `UPTERM_RIG=5 python3 tools/rig/amarchy_menu_rig.py --only dopus_quit`, 3 of 3. Root rows list Dopus while DOPUS.1 exists; after the quit DOPUS.1 is gone and a re-read of the root rows lists no Dopus group (Apps and the rest stay).
- What ends DOpus 5.82: the Opus > Quit... menu item, then the left button (gadget id 1) of "Directory Opus Request" (task dopus_quit). Measured NOT working: ARexx `quit`, `dopus quit`, `close`, `command quit/Close` (all return empty, port stays); `wasabi kill <dopus process>` (Ctrl-C ignored; --force RemTask not tried, it would leave screens and windows). No DOpus 5.82 ARexx docs exist in the repo (grep found none), so no ARexx quit was found.
- Gotchas: the Opus menu belongs to the active window, so the row first clicks empty backdrop (720,450) to make the DOpus backdrop window active; the terminal window of the earlier rows would otherwise own the menu. The requester's buttons have no labels (dopusbuttongclass), the left one is clicked by position.
- The row leaves the rig without DOpus; `Run >NIL: <NIL: LoadWB` on the rig brings 5.82 back (DOPUS.1 returned after ~8 s) without a rig restart. Harness: `with_drawn_menu(fn)` now holds the drawn-menu setup that dopus_present used inline.
- Not verified: 5.104 (rig has 5.82 only); the Quit... confirm button position on 5.104.

## 5.104 rows (s39)

Rig 5, `rig.py dopus` installed 5.104 (Installer ran to 100%), restart booted it: `Version DOpus5:DirectoryOpus` = "DirectoryOpus 5.104 [OS3]", dopus5.library 73.8, process DOpus5:DirectoryOpus, rexx port DOPUS.1 answers. The rig-start [WARN] is gone after the install. Rows run with `--only <row>` (one name per run; a comma list runs 0 rows).

- dopus_present (D6, D7): PASS, "ok dopus_present: 3 of 3 checks passed" on 5.104. D7 is still title-only.
- dopus_listers (D21): PASS, "ok dopus_listers: 4 of 4": `lister new` + `lister read` give two handles, Open listers rows ['RAM:', 'Fonts', 'Devs'], picking Fonts / Devs flips the front lister. `lister query all` over ARexx returns handles on 5.104 (it answered nothing on 5.82/5.91).
- dopus_quit (D22): PASS, "ok dopus_quit: 3 of 3": Opus > Quit... removes DOPUS.1 and the Dopus root row. Harness fix: the row used to leave the rig without DOpus; it now ends with restore_dopus (`C:LoadWB`, the boot's LoadDB) in a finally; after it dopus_present 3 of 3 and dopus_listers 4 of 4 pass again on the same rig. Test: tests/test_menu_rig_dopus_quit_restore.py (make ONLY=menu_rig_dopus_quit), fails 2 of 2 with the fix reverted.
- dopus_absent (D5): NOT RUN here, as designed it FAILs on a rig with DOpus ("DOPUS.1 exists on this rig"); it needs a `--no-dopus` rig (PASS 11 of 11 earlier, section above).
- Not tested: a separate DOPUS.1 screen for D7 (5.104 runs as the Workbench here, one screen).

## D7 own screen (s39)
- DOpus 5.104 put on its own screen on rig 5: no ARexx verb sets the screen mode (`lister set mode` is the lister view; `dopus screen` only reads). The way is the GUI: click empty backdrop, `ui menu Settings/Environment...`, list gadget id 19 row 7 (y 270) = "Display Mode...", click a fixed-mode row in list id 23 (450,245), `ui click --id 21` (Use). Within 8 s the screens list shows two screens: "Directory Opus" 1344x1080 (lister, button bank, backdrop) and "Workbench Screen" 800x600 (Amarchy bar). `dopus screen` answers DOPUS.1.
- `amarchy_menu_rig --only dopus_present`: PASS 5 of 5. Two new checks (only when `dopus screen` names DOPUS.n): the "UP-Term" window opened by Open UP-Term here is on the screen holding the DOpus windows, and the drawn Amarchy menu (window "Amarchy") opened on that screen. Helper on_dopus_screen, test tests/test_menu_rig_dopus_screen.py (make ONLY=menu_rig_dopus_screen).
- D7 is no longer title-only. Not verified: the menu's colours or geometry on the 1344x1080 screen (only the screen placement); the mode was switched by the Environment window, not persisted as a reproducible rig step (a rig restart from a fresh clone is back on the Workbench screen).
- Also fixed this session: `--only a,b` (comma list) runs those rows in the amarchy rig scripts (tests/test_rig_only_list.py); amarchy_prefs_rig no longer fails to import (stage_themes was removed in d7d64cf8).
