# s80 New tab, no terminal open: rig 9 (3.1 default rig, 060 --max, purpose "s80 vtcon session: New tab no-terminal check")

Procedure as s77 launch-fix-rig.md: rig.py start --max, patina_gui_rig.py --setup (fresh build; setup log: Patina,
patina-menu, patina-gui, patina-flat in VTCX:Apps/bin "matches build/amiga/<name>"). Build md5 prefixes: see reply.

## Verdict: NOT EXERCISED (row unreachable with no terminal open, text-only)
- Menu open with no UP-Term window: root rows Apps, Dopus, Settings, Agents, Network, Style, Capture, Window, ...;
  Apps rows UP-Term, Tmux, Editor, ...; Window rows start Next window, Previous window, Bring to front ... (PATINA_ROWS is
  cut at 256 bytes; Window level has more than 33 rows, remainder not read). No "New tab" seen.
- patina/menu_tree.c mn_row_needs: menuid: rows (MENU_TAB_NEW is one) need a terminal except style.terminal.* and agents.*,
  so Menu > New tab is hidden with no terminal (same finding as s77 for Apps > Claude). The fix's path
  (lf_menu_open_is_all) is therefore reached only if a row shows it; not seen in the first rows.
- Left Amiga + Shift + T with no terminal: nothing happened (no window, no T:Patina-uterm.* file).
- T:Patina-uterm.*: none written (no check reached an open).
End state: rig 9 up, hold 4 h, no UP-Term window open, no menu open. Scratch: none large (logs in scratchpad, few KB).
