# PREFS-NO-OUTLINES design (s72, 2026-11-11)

Evidence: `prefs-frame-census.md` in this directory (agent 1 census, "## agent 2: draw trace").

## Owner rulings (final)
- No coloured outlines anywhere in Workbench Prefs. Every boxed element uses fill.
- Calendar day cells: filled rounded cells like flat push buttons, no outline, selected day accent fill.
- Sliders everywhere: track filled #3E4254, rounded, no outline, knob accent fill.
- Read-only display boxes (date row in SYS:Prefs/Time) look the same as inputs: ground #3E4254, rounded, no outline.
- Group boxes: plain or subtle fill.

## Violated invariant
A frameiclass frame on a flat screen never draws a line in pen_line. Today the fill==0 branch of
`flat_frame_draw` (amarchy/flat_glyph.c ~529-533) draws 4 strips in pen_line for every edges-only frame.
Root-cause level: the frame hook `c8_draw` (amarchy/flat.c ~972) has no classification for edges-only frames.

## Measured facts the design rests on
- Every such frame arrives as IM_DRAW, state 0, fill 0. IA_Recessed / IA_FrameType / IA_EdgesOnly are not answered.
  Only geometry and the window's gadget list can classify a frame.
- Content is drawn AFTER the frame for day cells, date row and sliders: pen-0 RectFills (date row: two; slider: container
  ground), a pen-1 knob fill, Text (date row: JAM2, BgPen 0).
- Exceptions: the selected day's accent fill (pen 3) is drawn BEFORE its frame; a group box label Text is drawn BEFORE its frame.
- Redraws always repeat the frame before the content (day click, slider drag). Idle: no draws.
- Slider: prop gadget 130,156 186x7 inside frame 126,154 194x11 (inset 4 x, 2 y). Day cell frame 44x15 inside the calendar
  custom gadget 12,53 311x100. Date row 12,180 308x14: no gadget. Font Prefs group box 14,46 376x35 holds gadgets.

## Rule per frame type (c8_draw, fill==0 path, after the existing border_button / string_ground / smart_knob checks)
Classification is a PURE function in amarchy/flat_core.c (host tested), fed with the frame box and the window's gadget
boxes and types. First match wins:

1. TRACK: the frame borders a PROPGADGET (same tolerance as `flat_string_frame`: 6 px horizontal, 4 px vertical).
   Draw: rounded ground fill of the whole frame box (reuse `ground_box` / `raa_ground_frame_draw`), ground pen = the input
   ground pen (same source as `string_ground`). No outline. Knob stays as the existing RectFill path paints it (accent).
2. CELL: the frame box lies wholly inside the box of one gadget that is not a string or prop gadget, and is smaller than it.
   Draw: rounded fill in the push button face pen (pen_button, else pen_back: the PLAIN role face of `flat_frame_draw`).
   If the pixel at the box centre already is pen_line (the app's selected fill, drawn before the frame): rounded fill in
   pen_line instead (accent). No outline.
3. BORDERED-OTHER: the frame borders exactly one gadget of another type within the tolerance (listview and similar).
   NOT changed in this step: keeps today's draw. No measurement exists for it yet. Report it under Found.
4. GROUP: the frame interior wholly contains one or more gadgets (and rule 1 to 3 did not match).
   Draw: nothing (plain fill = the window ground, so content and the label drawn before the frame survive). Return handled.
5. DISPLAY: no gadget relation at all. Draw: the same rounded ground fill as an input (READONLY-BOX-GROUND).

## Content drawn after the frame: ground box records
TRACK, CELL and DISPLAY record their box (surface, task, box, fill pen) in a small ring in flat_core (same shape as
`flat_gh`: `flat_gh_frame` / `flat_gh_match`; extend or copy that pattern, do not invent another). Ring size 48 (a month has
up to 31 cell frames before their content may arrive). Any other frame draw overlapping a record drops it.
- RectFill patch: a SOLID fill (no area pattern) whose pen is pen_back (the window ground pen) and whose rect lies inside a
  recorded box is drawn in the record's pen instead, and must leave the rounded corner pixels of the box as the frame drew
  them (clip, or repaint the corners with the code `ground_box` already uses).
- Text patch: a JAM2 Text whose pen position lies inside a recorded box and whose BgPen is pen_back runs with BgPen = the
  record's pen for that call (restore after).
- Everything else is untouched.

## Known risk (accepted, written down)
An app that draws content BEFORE an edges-only frame with no gadget relation (rule 5) loses that content under the blind
fill. Not seen in the trace. If the rig proof finds a case, report it; do not work around it.

## Tests
- Host (tests of flat_core): classification for the five measured boxes above (slider TRACK, day cell CELL, date row
  DISPLAY, Font group box GROUP, year string frame still the string path) plus record match / miss / drop cases. Each must
  fail before the fix (missing symbol or wrong class) and pass after.
- Rig 68 proof (separate agent): pixel census of Time Prefs: zero 7aa2f7 on the border of sliders, day cells and date row;
  interiors ground #3E4254 (best match 3f4551) with rounded corners; selected day accent; text readable (a9b1d6 on ground);
  after a day click and a slider drag the same. Font Prefs group box: no 7aa2f7 outline, label and gadgets intact.
