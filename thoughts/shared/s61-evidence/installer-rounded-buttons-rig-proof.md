# Installer buttons after b7377f75 (rounded solid push buttons): rig 97 measure, FAIL

Rig 97 (purpose "Amarchy session: Installer rounded buttons check"), sys.hdf clone, tokyo-night, 800x600. HEAD b7377f75 built (`make amiga`),
`rig.py install`, fresh boot; `VTCX:amarchy-flat STATUS` = `dopus=68.16 patched=3 calls=1 flat=1`, rectfill hooked, `frame=hooked`.
DOpus 5.104 Installer started as on rig 94 (`Execute VTCX:dopus_run.script`, CD VTCX:dopus_install, `Installer "Install DOpus5" NOLOG`),
page 1. Pixels from `ami.grab` read with PIL (scratch script, image not viewed). Window 496x210 at 152,195.

## STATUS before / after the Installer opened
- before: `rectfill calls=477 hits=20`, `frame=hooked calls=16 flat=0`
- after (page 1): `rectfill calls=818 hits=30`, `frame=hooked calls=32 flat=4`
Four frame calls were replaced; the 16x9 gadgets 2,3,4 (radio boxes, real frameiclass images) account for them. The four 238x16 buttons are not among them.

## Pixels (host RGB), buttons 90 (160,369), 91 (402,369), 100 (402,386), 1 (160,386), all 238x16
Same for all four: corner (x0,y0) 7aa2f7; row 0 x0..8 all 7aa2f7; row 1 x0 7aa2f7, x1..8 1a1b26; col 0 y0..8 all 7aa2f7;
top-mid, left-mid, right-mid, bottom-mid 7aa2f7. Interior (x+8..w-8, y+3..h-3): 1a1b26 fill (= window body 1a1b26) plus a9b1d6 label pixels:
button 90: 222, 91: 142, 100: 61, 1: 76 (non-zero = label drawn, readable).
=> square 1 px accent ring in the corner, body-coloured interior: the old outline-only look. Not rounded, not filled, outline present. FAIL.
String field edge: not measured (page 1 has no string gadget; it appears on later pages).

## Cause (gadget dump with a throwaway probe `gprobe`, walks Window->FirstGadget under LockIBase, scratchpad, not committed)
```
g5 id=91 box 250,174 238x16 type=5 flags=2 act=1 text=no render=103c7d4a select=103c7d6a same=0 imgflag=0
g6 id=90 box   8,174 238x16 type=5 flags=2 act=1 text=no render=103c7c8a select=103c7caa same=0 imgflag=0
g7 id=100 ...                                     render=103c7bca select=103c7bea same=0 imgflag=0
g8 id=1  ...                                      render=103c7b0a select=103c7b2a same=0 imgflag=0
```
GadgetType 5 (custom gadget), Flags 0x0002 = GFLG_GADGHIMAGE without GFLG_GADGIMAGE (0x0004): GadgetRender and SelectRender are two
different objects, and they are not Images (word at Image->Depth reads 4156, not CUSTOMIMAGEDEPTH). The Installer's buttons are
Border/custom-drawn gadgets, not frameiclass images. So the frameiclass hook (c8_skip, amarchy/flat.c:583) never runs for them: the commit's
flat_frame_fill / push_gadget_of path is not reachable from these buttons. Even if it were: gadget_of_image (flat.c:565) requires
flat_button_gadget (render == select == obj), false here (render != select). IA_EdgesOnly is not the question for these buttons; the ring is
drawn by the border line pass (not traced further).
Measured, so not a guess: the frame counters (calls +16, flat +4) match the three radio boxes + one more frame; per-button image identity
differs; neither of the two conditions the commit relies on holds for these gadgets.

## Verdict
FAIL for the Installer buttons: b7377f75 does not change them. Fix level: the border-drawn custom-gadget draw path (where the ring is drawn),
not frameiclass. Not fixed here (measure only). PNG: scratchpad flatbuttons/installer-after.png.
