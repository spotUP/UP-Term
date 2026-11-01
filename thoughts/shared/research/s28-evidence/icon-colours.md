---
date: 2026-11-01
topic: OWNER-ICON-COLOURS-INVERTED, rig measurement
status: Q1 root measured; Q2 and "filled" open
---
# Lucide icon colours on a 3.2 rig (rig 11, AGA 256, 640x256)

Tool: tests/amiga/wbpenprobe (GetRGB32 pens 0..63 + ReadPixel rows of the Workbench screen), driven by
tools/rig/wbpenprobe_rig.py `<tag> [apply]` (amarchy-icons APPLY, opens SYS: via the WORKBENCH ARexx port, probes it).

## Q1 inverted colours: measured root
lucide.dat draws the glyph in the reserved pens 32 (33% coverage), 37 (66%), 33 (100%). Those pens get theme colours
only while amarchy-flat runs and holds them (flat.c pens_sync -> ip_colours -> ip_holder_obtain).

| case | pen 32 (edge 33%) | pen 37 (66%) | pen 33 (core 100%) |
|---|---|---|---|
| APPLY, no amarchy-flat (stock 256-colour palette) | ffffff | ffffff | 888888 |
| same, Amarchy running with flat-system-look = on but no amarchy-flat binary reachable | ffffff | ffffff | 888888 |
| amarchy-flat running (STATUS `pens-held 16`) | 494d60 | 787e9a | a9b1d6 |

Without amarchy-flat the stroke core is darker than its edges (white rim, grey core): the owner's "inverted" look,
same as the screenshot (s28-owner-rig8-icons.png). The icon frame was also still drawn (borderless not set).
Pixel rows of SYS: show pen 0 (transparent) around the glyph: no box behind the glyph in this window.

Not checked: whether amarchy-flat runs on rig 8 (owner's rig, not touched). Check there: `amarchy-flat STATUS` must say
`pens-held 16`.

## Q2 label background: not measured
Stock WB (no Amarchy font.prefs): labels have no background (pen 0 under the text). Amarchy's desktop-labels target writes
font.prefs WBFONT FrontPen=DK_FG, BackPen=DK_DARKER, DrawMode 0 (JAM1), SpecialDrawMode 1 (desk_core.c desk_font_pens).
Next probe: apply a theme with desktop-labels on and compare the pens under a label with the window pen.

## "Flat and filled"
lucide.dat is rasterised from the Lucide font: a stroke-outline icon set, 2-3 px strokes, nothing filled.
"Filled" needs a design choice: another icon set with filled glyphs (e.g. Phosphor Fill), or filling closed Lucide paths.
