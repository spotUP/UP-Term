# Scroller knob DK_MUTED pixel proof (rig 52, Amiberry, OS 3.2, HEAD 33f03a31 at run time; task base 39152f89)

Setup: rig.py start --max --emu amiberry --os 32; amarchy_gui_rig.py --setup; amarchy-flat to RAM:, flat prefs on, theme pick (amarchy_desk_rig.pick).
Shot: ~/Desktop/upterm-shots/s30-knob-muted.png (SYS:Prefs, View by Name, Show All). Pixels by PIL, whole-screen grab 640x256.

| check | region | measured | verdict |
|---|---|---|---|
| vertical knob | x603..613 y26..151 | 7A7A7A 940 px, 000000 310 px (track) | PASS |
| horizontal knob | x21..565 y176..182 | 7A7A7A 3264 px, nothing else | PASS |
| file names | name column x20..200 y30..150 | 7A7A7A 1584, 8D8D8D 334 (drawer names, not file names), rest 000000 | PASS |
| labels plain | whole window body | only 000000, 7A7A7A, 8D8D8D: no shadow colour | PASS |
Earlier shot of SYS:C (full-height knob): vprop 7A7A7A 550/550 px, hprop 7A7A7A 2070/2070 px.
Bold not measured by colour; no second colour beside glyphs.
