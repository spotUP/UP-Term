# UP-Term title bar on rig 96 (2026-11-04, s57)

Rig 96 (AmigaOS 3.2, 060, full install from 31ca8e69; ami.py warned pty-handler, vtcon-handler differ from the checkout build, rig not refreshed). Window opened with RAM:wbrun SYS:System/UP-Term (needs a temporary Assign UP-Term: SYS:UP-Term; the rig's runtime had no UP-Term: assign, so a plain double-click gives a Volume Request; removed again afterwards). Screenshots: ~/Desktop/upterm-shots/s57-rig96-upterm-active.png, s57-rig96-upterm-inactive.png; analysis script s57-title-analyse.py (PIL, pixel dump).

Window 640x235 at 0,11; title bar rows 11-20; gadgets: close x0-19, zoom x594-617, depth x617-640.

## Defect 1: black rectangle behind title text: ABSENT
- Active: bar (122,162,247) over rows 11-20 x20-593: 5604 px bar colour, 136 px (26,27,38) = the glyph strokes only. Dump shows text "UP-Term" with bar colour between and around glyphs, rows 11 and 19-20 bar colour above/below.
- Inactive: same layout, bar (90,94,117), 5604 bar px, 136 text px (26,27,38). No filled box.

## Defect 2: bottom line missing or stepped under depth/zoom: ABSENT
- Row 20 (last bar row) is one run across x0-639 in bar colour, active (122,162,247) and inactive (90,94,117): no step or gap under zoom (x594-617) or depth (x617-640).
- Gadget glyph boxes end at row 19 (runs at x599-611 and x625-635 dark on row 19 only); row 20 under them is bar colour.
- Row 21 (window interior top) runs: bar colour x0-1, (26,27,38) x2-637, bar colour x638-639 (the border), same in both states.
- Note: there is no darker separator line between bar and interior in this build; the bar edge is the colour change at row 21, continuous.

Owner screenshot is therefore from a different/stale build, or from a window state not reproduced here (not tested: other themes, other fonts, a window not at 0,11).
