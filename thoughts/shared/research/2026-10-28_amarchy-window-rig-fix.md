# amarchy_window_rig.py 17 of 17 (s19)

## menu_move_left / move_left: script bug, not the product
Product title is intact. Measured rig 7: `AMARCHY SHOW` -> window "Amarchy" 503x504 (list_windows and ami.window both agree).
The rig's Super+Space is pressed with a console (the target) active; amarchy.c act_menu_kind -> cx_menu_style(menu-style,
menu-in-terminal, in_term) gives the TEXT menu (menu-in-terminal = text is the default): an UP-Term XCON window with EMPTY title
(296x224 here, 608x352 in the earlier run). The script waited for the drawn menu title "Amarchy". Fix: the step sets
menu-in-terminal = graphical in ENV:/ENVARC:Amarchy/prefs for its run (same key/store as amarchy_prefs_rig.set_graphical,
amarchy_menu_rig DRAWN), RELOAD, restores after. Files are rewritten with Echo lines (x9_conf way): ami.restore_file's
rename-into-place fails on ENV: of the OS 3.2 rig.
move_left was the knock-on (menu step did not move the window back; also STATE['box0'] missing under --failed): both steps now
FAIL with "needs step open_target in the same run" instead of KeyError.

## b2_*: needs a WORKBENCH port
Rig 7 default (DOpus) and --no-dopus (OS 3.1): ports lack WORKBENCH. Only an OS 3.2 Workbench has it. `--os32` needs the owner's
tree (missing at ~/Downloads/AmigaOS-3.2-full (1)); `--os 32` (clean Workbench_3.2.hdf) works. b2 steps now go through
needs_wb(): absent port -> FAIL "needs a WORKBENCH port ...", never a silent pass.

## Verdicts
rig 7 default DOpus image: 13 of 17 (b2 x4 FAIL with the clear message).
rig 7 `--max --os 32` (AmigaOS 3.2, AmTile not running): 17 of 17; b2_redraw rc 0, b2_patterns 31 sent 0 refused,
b2_icons_selected 9 rows 0 refused, b2_viewname rc 0 (View by Name checked).
Command: UPTERM_RIG=7 rig.py start --max --os 32; amarchy_window_rig.py --fresh --no-build. Logs: build/rig7/amarchy-window.
