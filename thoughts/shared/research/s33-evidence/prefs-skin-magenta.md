---
date: 2026-11-03
topic: S33-PREFS-SKIN-FAIL - magenta (FF33FF) buttons in Locale Prefs on OS 3.2
status: root cause located (rig 64), rig 96 trigger not reproduced
---

# S33-PREFS-SKIN-FAIL: magenta buttons in Locale Prefs

## Setup
Rig 64, OS 3.2, `--max`, fresh-all install from main (ae5755e3), every Installer option ticked
(`installer_rig.py ONLY fresh-all` PASS). Dist built against the ports kit worktree
`upterm-ports/.claude/worktrees/kit-main` (STAGED-FROM d94a685), because the default kit path failed
the PORTS_KIT_FRESH stamp check.

## Measurements (SYS:Prefs/Locale open, Workbench 640x256, 8 planes)
- Main as installed: pen 4 = 3E4254 (theme selection), pen 252 = 3E4254. FF33FF pixels on the whole screen: 0.
  The 3E4254 count was 3693 pixels (the button fills).
- `amarchy-flat STATUS`: clone draws=96 fallback=0, frame=hooked calls=96 flat=24,
  pens-held 16 refs=3 dim=251 knob=249 inbar=250.
- Forced pen 4 = FF33FF with a scratch SetRGB32 tool, with nothing else changed: the Locale window (0,54 552x192) shows
  3693 FF33FF pixels. That is exactly the earlier 3E4254 count, so every button fill in the window is literal pen 4.
  Pen 252 (the pen palette.prefs themes for entry 4 at 8 planes) stayed 3E4254.
- Shots: ~/Desktop/upterm-shots/s33-prefs-magenta-locale-main.png, s33-prefs-magenta-locale-pen4-forced.png

## Draw path
- amarchy/desk_core.c:145 desk_flat_state: pen_button = GR_SELECTION = DK_SELECTION = 4 (a role index used as a pen number).
- amarchy/flat_core.c:537-544 flat_button_pen returns pen_button (4) whenever 4 < 1<<depth. It never checks that pen 4
  is a pen anyone holds with the theme colour.
- amarchy/flat.c:440 c8_skip (frameiclass hook, FLAT-C8) sets st.pen_button from it.
- amarchy/flat_glyph.c:406 flat_frame_draw_round fills the button with s->pen_button.
- Knobs: flat_knob_pen falls back to flat_button_pen when pen_knob/pen_dimtext are unusable (flat_core.c:446-451).

## Mechanism
At depth > 3, palette.prefs entries 4-7 land on pens 252-255 (desk_palette_pen, desk_core.c:287). Pens 4-7 are "live role
pens" (desk_live_role_pens, desk_core.c:307): no palette entry reaches them. They hold the theme only through a SetRGB32 from
dk_live_pens (desk_dos.c:592-622), repeated only when ip_sync sees a new screen (iconpen_core.c:226-229). They are also never
obtained, so they stay free in the ColorMap. Any later write to pen 4 changes flat's button fill: a palette reload, another
program's LoadRGB32, or a program that obtains pen 4 as a free pen. A web-safe cube value (FF33FF, 0x33 steps) points at a
256-colour palette load. Flat does not re-follow, so the buttons stay magenta. The clone marker path is not involved:
fallback=0, and the clone renders into its own bitmap.

Not reproduced: which program wrote pen 4 on rig 96. Rig 96 was not touched (brief). Fresh main on rig 64 is clean until
pen 4 is overwritten.

## Proposed fix
Flat must paint the button fill (and the knob fallback) with a pen it holds. Two options:
- ObtainBestPen the selection colour in flat_pens_refresh (flat_core.c:680-740), as it does already for knob, dim and inbar.
- Use the palette.prefs-reached pen, desk_palette_pen(DK_SELECTION, depth) (252 at 8 planes).
Do not use the unheld literal role index 4.

## Regression test idea
Host test (flat_core): depth 8, gadgets on, desk_flat_state as built by desk_core. Assert that flat_button_pen does not return a
live-only role pen (desk_live_role_pens(depth) must not contain it), and that it equals the obtained or palette-reached pen.
Rig check: open Locale, run setpen 4 FF33FF, assert 0 FF33FF pixels in the window.
