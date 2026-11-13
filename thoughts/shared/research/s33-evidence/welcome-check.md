# PATINA-WELCOME / RL16 welcome window check (rig 65, main ba1e183c)

Rig 65 (64 was in use), `rig.py start --max --os 32`, Kickstart 47.115 / Workbench 47.2 (exec.library 47.13).
Patina, patina-gui staged in VTC:Apps/bin, started with `Run >NIL: VTC:Apps/bin/Patina`.

(a) PASS fresh start: uitree `W 0 36 36 568x183 active "Welcome to Patina"` (no ENVARC:Patina/prefs welcome key).
Text (screenshot): "Patina puts the whole Amiga under one key, called Super. / Super is the Left Amiga key. /
Open the menu: Super+Space / Snap a window left: Super+Shift+Ctrl+Left / Take a screenshot: Super+Help /
Change the theme: Style > Patina theme / Open this help: Super+Shift+Slash / Quit Patina: Super+Shift+Ctrl+Q /
The menu lists every program, setting and action. / Press Escape to close it. / Every key above can be changed in
the keymap file. / This text then shows your new key. / Tick the box below to stop this window from opening at
start. [ ] Don't show this again  [Close] [Open Help]".
(c) PASS: closed with Enter unticked, ARexx QUIT, restart: window "Welcome to Patina" reappeared (about 15 s after start).
(b) PASS on state: ticked (3 toggles: one click + two space), Enter; ENV:Patina/prefs and ENVARC:Patina/prefs both hold
`welcome-shown = on`; QUIT + restart: only "Patina bar" + Workbench windows, no welcome window, ARexx port up.

Found (visual defects):
1. The window (568x183) shows the text only; the checkbox row and Close/Open Help buttons are below the visible area
   and appear only after scrolling (Down key x9). First view has no visible way to tick or close.
2. The checkbox box never repaints as ticked (screenshots after click, space, space all show an empty box) although the
   toggle registers and is saved. Count of toggles: click, space, space = on. draw_list after toggle does not redraw the tail rows.
Screenshots: ~/Desktop/upterm-shots/welcome-a.png, welcome-scrolled2.png, welcome-ticked*.png, welcome-r2-*.png
