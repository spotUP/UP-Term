---
topic: Amarchy text menu - mouse motion selects text; Super+Space opens graphical menu
date: 2026-10-26
---
# Findings (read-only, origin/main)

## Q2 (design, not a bug)
- amarchy/cxdesc.c:63 `is_text = (style == UC_MENU_TEXT) != other;` picks amarchy-menu vs amarchy-gui (cxdesc.c:74).
- dist/keymap:40 super+space = menu:root (style from the amarchy-menu setting, default graphical);
  dist/keymap:43 super+alt+space = othermenu:root (the OTHER style) = the owner's "special combo".
- Plan thoughts/shared/plans/2026-10-23-amarchy-drawn-menu.md ("amends ... the text menu stays; a graphical one is added and is the default", line 5) and:
  "The extra modifier: Alt. ... Super+Alt+Space and Super+Alt+K open the style the setting does not name."
- The setting is global; the code has NO terminal-aware choice (inside UP-Term it still opens amarchy-gui). If the owner wants text inside UP-Term, that is a new feature, not a regression; set the style to text or use Super+Alt+Space.

## Q1 (mouse selection)
Mechanism that makes motion extend a selection (confirmed in code):
- handler/vtcon_handler.c:2881 IDCMP always includes IDCMP_MOUSEMOVE; :5564-5565 sends MOUSEBUTTONS and MOUSEMOVE to vtwin_mouse(..., move=cls==MOUSEMOVE, code, qual,...).
- render/vtwin.c:1676-1679: `if (move) { if (w->dragging) drag_to(...)`; drag_to (vtwin.c:1606-1617) calls vr_select(...) to the pointer cell. Nothing checks that the left button is still down (qual & IEQUALIFIER_LEFTBUTTON is never read).
- vtwin.c:641-648 vtwin_tick ALSO follows the pointer by polling w->win->MouseX/MouseY while w->dragging, with no button check at all (comment: moves may never reach us as events).
- w->dragging is set only at vtwin.c:1700 (VTI_SELECT, on a SELECTDOWN with !selecting, mouse modes off) and cleared only by VTI_SELECT_END (vtwin.c:1721, needs the SELECTUP to reach this window while dragging) or resets (70, 96, 983, 1100, 1198).
- So: one SELECTDOWN whose SELECTUP never reaches the window leaves dragging=1 forever; every pointer move then extends from drag_ax/drag_ay (the cell where the press landed = where the pointer was).
Invariant violated: "a drag selection exists only while the left button is physically down"; the handler trusts the event stream instead of the live button state.
Why clicks do not end it: not proven. vti_button (render/vtinput.c:115-123) returns VTI_SELECT_END only on a SELECTUP with selecting set; a SELECTDOWN restarts selection (VTI_SELECT). If the text menu (libupkgui) turns on a ?1000/?1002 mouse mode the click is reported to the program (vtinput.c:84 branch needs !selecting, so it is skipped while dragging=1). UNVERIFIED: which event supplies the stray SELECTDOWN. Amarchy side checked: act_in_window (amarchy/act.c:97) opens an XCON:.../CLOSEINACTIVE window and runs the command with SystemTags; no synthetic mouse event, no mouse qualifier in the hotkey (cx.c injects only RAWKEY). Suspect: the new XCON window opening under the pointer / activation while the press that opened or preceded it is still down, then the SELECTUP goes to another window.
Needs a rig/live repro (not allowed here): log SELECTDOWN/UP and w->dragging at menu open.

## Proposed fix (root level: vtwin)
1. vtwin_mouse move path and vtwin_tick drag follow: continue only while (qual & IEQUALIFIER_LEFTBUTTON) for events, and for the tick poll check the live button state (e.g. keep last qual from the IDCMP, or read input.device qualifier); if not held, end drag exactly like VTI_SELECT_END (dragging=0, pointer_sync, copy-on-select).
2. Also end drag on IDCMP_INACTIVEWINDOW and window close.
## Regression test idea
Host test in the vtwin/vtinput test suite: feed vtwin_mouse SELECTDOWN, then MOUSEMOVE events with qual lacking IEQUALIFIER_LEFTBUTTON and no SELECTUP; assert vr_selection unchanged after the first button-less move and w->dragging==0. Fails today (selection keeps extending).
