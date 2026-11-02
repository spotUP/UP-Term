# Flat outer borders by pixel aspect (commit 230044cf, rig 70, OS 3.2, FS-UAE 060)

Build: `make amiga` rc 0; amarchy-flat and flatwin from the branch merged with origin/main (97408c0f).
Flat switched on with `flatwin 1 SET 1 0 2 3 0 3` (pen_outline 3 = RGB 102,136,187), window: `NewShell CON:40/40/400/100`
at screen x 40..439, y 40..139. Pixels read with `ami.grab()` (screen framebuffer), runs of equal colour counted.

## PAL hires non-interlaced, 640x256 (ScreenMode 0x29000, rig.py aga16)
- STATUS: `rectfill installed=yes calls=1430 hits=172 shits=120 side=2 looks=1` after 8 more window redraws (calls rose 885 -> 1430, looks stayed 1).
- Row y=90: outline-pen columns x=40,41 (left) and x=438,439 (right): 2 px each. Interior colour at x=42 and x=437.
- Column x=200: bottom border one row, y=139 only (1 px). Top rows y=40..49 are the title bar, same colour as the outline pen, so the
  top line cannot be separated from the title fill on this palette: top thickness NOT measured separately.
- Verdict: sides 2 px: PASS. Bottom 1 px: PASS. Top 1 px: unproven (title bar fill is the same pen).
- Shot: ~/Desktop/upterm-shots/s31-aspect-640x256.png

## PAL hires interlaced, 640x512 (ScreenMode 0x29004, written to ENVARC:Sys/screenmode.prefs, warm reboot)
- STATUS: `rectfill installed=yes calls=1626 hits=164 shits=84 side=1 looks=1`, then calls=2841 looks=1 after 5 more redraws.
- With the flat palette applied (Palette USE of tests/data/desk/palette-flat-pen8.prefs): row y=100, outline pen at x=40 only (1 px) and x=439 only (1 px).
- Verdict: sides 1 px: PASS. Top/bottom not re-measured (an IPrefs Retry requester from Palette USE was left open).
- Shot: ~/Desktop/upterm-shots/s31-aspect-640x512-flatpal.png
- Without the flat palette (default grey Workbench pens, pen 3 = active title fill) the left edge reads 3 px of the same colour: the outline
  is indistinguishable from the active window fill there, not a drawing error; measure only with the flat palette.

## Open
- Top line thickness in non-lace needs a window whose title bar is not outline-coloured, or a pen_outline different from the fill pen.
