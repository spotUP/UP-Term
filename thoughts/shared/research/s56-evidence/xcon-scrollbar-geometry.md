---
date: 2026-11-04
topic: S56 XCON scrollbar knob drawn as a thin line under amarchy-flat on AmigaOS 3.2
status: fixed (rig 74 pixel proof)
---

# XCON scrollbar knob: thin line instead of a filled knob (3.2)

## Symptom (owner, rig 96)

New AmigaShell (XCON:, vtcon-handler) with scrollback, window active: the scroller knob is a thin
light-blue line in the lower half plus a stray tick under it; the track is empty. Pixel measurement
of the owner's shot showed the arrows ARE aligned with the container (x 626..635); the defect is the
knob body only: it took the track pen, and only slivers took the knob pen.

## Root cause

amarchy-flat decides the pen of each Intuition RectFill inside a prop gadget by whether the fill lies
wholly inside the knob rectangle. That rectangle was computed from PropInfo (`flat_scroller_knob`:
size = body * container / 0xFFFF, pos = pot * (container - size) / 0xFFFF). Intuition 3.2 lays the
knob out differently. Rig 74, XCON KBShell 640x240, measured with a probe (`kbprobe`, ReadPixel
plus the gadget's AUTOKNOB Image):

    G5 type=0005 (boopsi propgclass) box=626,12 10x195
    PI flags=001d vp=ffff vb=5af2 cw=10 ch=195
    KNOB img le=0 te=125 w=10 h=70          <- Intuition's own knob: rows 137..206

The formula gives 69 rows at row 138 (138..206). Intuition's knob fills start at row 137, straddle
the computed rectangle, and therefore took pen_back (invisible). Only fills wholly inside 138..206
(single rows/strips) took the knob pen: the thin line and the tick. The earlier S54 reproof only
covered Total == Visible (knob = whole container), where both layouts agree.

Root: `amarchy/flat.c` scroller_pen used the formula knob instead of the knob Intuition drew.

## Fix

The knob rectangle now comes from Intuition itself: an AUTOKNOB prop gadget's GadgetRender Image
holds the box Intuition laid out (LeftEdge/TopEdge relative to the container, Width x Height).

- `flat_core.c`: `flat_scroller_knob_img` (Image box when present and inside the container, else
  the formula) and `flat_scroller_pen_box` (pen for a given knob box); `flat_scroller_pen` wraps it.
- `flat_glyph.c`: `flat_scroller_cuts_box` (corner cuts for a given knob box); `flat_scroller_cuts`
  wraps it.
- `flat.c`: `prop_knob` reads the gadget's Image and feeds scroller_pen, the corner cuts and the
  STATUS sfill ring from one knob box.

## Regression test

`tests/test_flat_core.c`, block S56-XCON-KNOB-BOX (rig 74 numbers). With the Image path disabled:

    FAIL tests/test_flat_core.c:1044: ky = 138, want 137
    FAIL tests/test_flat_core.c:1045: kh = 69, want 70
    FAIL tests/test_flat_core.c:1046: flat_scroller_pen_box(... 626, 137, 635, 206) = 0, want 3
    [FAIL] flat_core: 2313 checks, 3 failed

With the fix: `[OK] flat_core: 2313 checks, 0 failed`.

## Rig pixel proof (rig 74, --os 32, fixed amarchy-flat restarted)

ReadPixel pens, x = 620..639 window-relative (container x 626..635), pen 3 = outline (active
knob), K = pen 250 (inbar, inactive knob), 0 = track:

XCON active (flags WINDOWACTIVE):

    137 ii233300333333003333     knob top, corners cut
    139 ii233333333333333333     rows 139..204 full 10 px knob, pen 3
    206 ii233300333333003333
    207..: track 0, arrows below at their usual rows

XCON inactive: rows 137..206 knob pen K (250), track 0, same rounded shape.

System drawer 220x110 (G5 horizontal knob 101x6 at x 3..103, track to x 167):
active: rows 102..107 x 3..103 pen 3, x 104..167 pen 0; inactive: same box pen K.
(The probe dumps the first prop's area only; the drawer's vertical knob 10x46 matches the formula
there, so it was not affected.)

Screenshot `~/Desktop/upterm-shots/s56-xcon-knob-fixed-rig74.png` (XCON inactive): column x=630,
screen y 149..218 (window rows 137..206, 70 rows) #D0D0D0 knob against #AAAAAA track.

## flat-budget

Before: 6 FAIL lines, rc 2 (S56-FLAT-BUDGET-RED, unchanged). After: 6 FAIL lines, rc 2, no new
or changed FAIL line.

## Rig

Rig 74 stopped and cleared (build/rig74 removed).
