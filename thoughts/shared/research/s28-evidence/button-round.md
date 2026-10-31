---
title: s28 FLAT-BUTTON-ROUND rig 8 check
status: rig-unverified
---

# FLAT-BUTTON-ROUND

Change: `flat_frame_draw_round` (amarchy/flat_glyph.c) reads the pen at the pixel just outside each corner with ReadPixel
(c2_read in amarchy/flat.c) BEFORE the box is painted, paints the box, then sets the cut pixels to the sampled pens.
Cuts come from `flat_button_corners(w, h, out)`: 4 corner pixels; 12 when h >= 12 (two more beside each corner).
Applies to primary fill, pressed (accent) fill and the secondary outlined button. Window corners untouched.

## Rig 8 check (a later agent; start rig 8 with `UPTERM_RIG=8 python3 tools/rig/rig.py start --max --os 32 --purpose "<session>: button corner check"`)
1. `make wasabi`, `make amarchy-flat`; copy build/amiga/amarchy-flat to the rig (md5 must match the one in the commit report).
2. Start amarchy-flat, switch flat on (as in the FLAT-BUTTON-FILL checks), open a GadTools requester with buttons (e.g. an Installer page, Ok / Cancel row).
3. Sample with wasabi (ReadPixel of the screen, or screenshot + haiku zoom) the pixels (x0,y0), (x1,y0), (x0,y1), (x1,y1) of one primary button:
   PASS = each equals the window background pen next to the button, NOT the button fill pen; pixel (x0+1,y0+1) is still the fill pen.
4. Button 12+ px tall: also (x0+1,y0), (x0,y0+1) and the three other corner pairs equal the background. 11 px or less: they are the fill pen.
5. Press a button (hold the mouse): the accent fill has the same rounded corners, label still dark.
6. Put a button on a patterned fill / a MUI group box: corner takes the group-box pen, not pen 0.
7. Stop the rig, confirm with ps, `rig.py clear`.
md5 amarchy-flat 3e341da2272ee7df016abae07b79d8f7
