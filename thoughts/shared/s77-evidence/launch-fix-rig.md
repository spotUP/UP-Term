# s77 launch fix, rig 9 (3.1 default rig, 060 --max, purpose "s77 Patina session: launch fix check")

Binaries: `patina_gui_rig.py --setup` (made fresh; --no-build refused as stale). md5 on rig = build:
patina-gui ce8426f4, Patina 216ce26a, patina-menu d2f9c992 (VTCX:Apps/bin and Rushhours:Tools/UP-Term/bin both).
Status still shows "Loaded as command: Rushhours:...patina-gui / patina-menu" for processes 1 and 3; Break did not end them,
files on disk match build.

## Q1 UP-Term row (open:uterm), no UP-Term window open: PASS
- Menu > Apps > UP-Term. T:Patina-uterm.00 written: `FailAt 2147483647 / UP-Term:bin/vsh / EndCLI >NIL:`.
- Exactly one window titled UP-Term; Status gains `UP-Term:bin/vsh`; screen text (ami.screen_text) shows the vsh
  banner art, "R O Y A L   K I N G S ...", tutorial hint, prompt `Rushhours:Tools/UP-Term/bin %` (no `1.SYS:>`).

## Q2 Claude program row, no window open: PASS, via Agents > "Launch the agent" (default agent Claude), NOT Apps > Claude
- Apps > Claude (menuid:MENU_CLAUDE) is not shown with no terminal open (menu_tree.c mn_row_needs: menuid needs a terminal;
  only style.terminal.* and agents.* are exempt). The Apps level rows were UP-Term, Tmux, Editor, ... with no Claude/pi.
  So MENU_CLAUDE itself was not exercised; MENU_AGENT goes through the same lf_menu_program / open_uterm(prog).
- T:UP-Term-claude: `FailAt / Stack 32768 / C:Claude / If WARN / Ask "Press Return to close this tab." / EndIf / EndCLI >NIL:`.
- One window titled Claude, Status shows only `C:Claude` (no vsh process), screens list one window, no second tab.
- C:Claude is installed on the rig.

Not checked: the logo pixels / front order by screenshot (text checks only; no screenshots taken).
End state: rig 9 left up (hold 4 h) with one UP-Term (vsh) window opened from the UP-Term row.
