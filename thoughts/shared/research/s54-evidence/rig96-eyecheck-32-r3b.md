# Rig 96 eye-check on AmigaOS 3.2, run 3b (s54)

Rig 96 as left by R3a (main ae859c52 install, KS 47.115 / WB 47.2). Not reinstalled, not rebooted, not stopped. Tools ran through a wrapper that sets rig.GATE_STATE busy, so ami.py's gate (rig.ensure_current) never pushed build/amiga: build_drift on rig 96 listed `kit-userland/upkg.info` (a refresh would have been triggered). Shots: ~/Desktop/upterm-shots/r96r3b-*.png.

| Row | Verdict | Evidence |
|---|---|---|
| e2 BAR-STRIP-RESERVE (backdrop icons) | PASS | BAR ON; root backdrop window box (0,11,640,235) ends at y=246 = bar top. Two leftover XCON hosts `barhost` and `f1host` (0,20,600,200) hid the icons; closed them (vsh exit). Then non-backdrop pixels in the backdrop area (excl. KnobShell): 521 px, bounding box x7-55, y18-189; 0 px in rows 236-245; bar rows 246-255 hold only the bar. No icon under the strip. Clean Up not run. Shot r96r3b-e2-backdrop.png |
| f FLAT-BUTTON-FILL | PASS (idle) | Palette editor: Save (88x14) 1136 of 1232 px = 3E4254, label A9B1D6, edge row 3E4254 168 / 1A1B26 8 (corners only), no bevel; Use same; "Multicolor Settings" 3700 px 3E4254. Pressed state (accent fill, dark label) NOT measured. Shot r96r3b-buttons.png |
| g FLAT-PENS-NOT-LIVE | PASS | `amarchy-flat STATUS`: flat=on calls=998, pens text=2 line=3 back=0 outline=3 button=4, rectfill hits=184 shits=102. Buttons in Palette editor flat (row f). Note: last line `frame=hooked calls=0 flat=0` (frameiclass hook path unused; buttons are drawn flat by the other route) |
| h R1.17-WBRESET | NOT RUN | needs the desk-apply rig (amarchy_desk_rig, imports would refresh/risk the rig) and budget |
| i AM-FLAT-WB | PARTIAL | WB root window is 640x235 frameless at (0,11) = Backdrop on (also R3a). Icon-border setting not measured (ENV:Sys/Workbench.prefs present, 84 bytes, not decoded); no theme switch / toggle exercised |
| TAB-FILLPEN | PASS | KnobShell (XCON 20,30,400x150), Left Amiga T opened tab 2 (window title became UP-Term = tab 2 active). Tab bar rows y41-51: active (right, x213-400) 1910 px A9B1D6 (TEXTPEN ground) + 158 px 1A1B26 (BACKGROUNDPEN letters); inactive (left, x22-212) 1890 px 1A1B26 ground + 211 px A9B1D6 letters. Clearly different. Shot r96r3b-tabs.png |
| 7 btnbg --prefs-magenta | NOT RUN | tool import would refresh/needs `make rig-helpers`; budget. Side measurement: Palette editor shows 167 FF33FF px (its own colour swatches, not a button fill) |
| 8 gui --welcome | NOT RUN | budget |
| 9 gadget probe (vtcon-16) | DONE | thoughts/shared/research/s54-evidence/gadprobe-rig96.txt (KnobShell 11 lines incl. rc, System 20+ lines incl. prop gadgets G5/G6 type=0003). KnobShell window closed afterwards (endcli) |

Rig state at the end: running (fs-uae present, wasabid answers ping). Amarchy bar ON. Windows: bar and WB backdrop only. Palette Preferences window (started via Run for the button measurement) could not be closed by gclick and is gone from the window list after the last close; barhost/f1host were closed. Wasabi connection fine.
