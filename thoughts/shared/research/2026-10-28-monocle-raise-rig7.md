# Monocle raise-on-activate, rig 7 (2026-10-28)

Question: with AmTile in monocle, does activating a window that is behind raise it (amarchy/tile.c ~345, g_last_raised, per poll)?
Verdict: PASS.

Method
- Activation without raise: scratch helper `actwin ADDR` (calls only ActivateWindow; source and binary in the session scratchpad, not in the repo), run via wasabi exec on rig 7. `AmTile CYCLE` (tile.c:455) and wasabi ui_front both call WindowToFront, so neither proves raise-on-activate.
- 3 UP-Term windows (A 0x1002b854 "echo AAAA", B 0x10120e54 "echo BBBB", C 0x102e2b44 "echo CCCC"), C opened last = front.
- Control, tiling OFF (AMTILE port absent), windows stacked at same box: actwin A, then B, then C, 1.5 s wait each. Wasabi reports the activated window active each time; screenshot still shows CCCC in front for A and B (actwin does not raise). Shots: monocle-ctl-act0/1/2.png.
- Tiling ON via the keymap row (Amarchy RUN 15 = AmTile TOGGLE), `AMTILE LAYOUT monocle`: STATUS "on 3 2 monocle 1 55". Shot monocle-1-monocle.png.
- Monocle: actwin A -> screenshot shows AAAA front; actwin B -> BBBB front; actwin C -> CCCC front. 1.5 s wait each. Shots monocle-mono-act0/1/2.png.

Shots in ~/Desktop/upterm-shots/ (monocle-*.png). No bug seen.
