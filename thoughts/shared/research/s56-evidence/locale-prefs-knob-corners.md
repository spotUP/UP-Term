---
date: 2026-11-04
topic: Locale Prefs listview knob colour and Cancel button corners under amarchy-flat
rig: 75 (060 max, AmigaOS 3.2, Workbench 640x256 depth 8)
status: fixed, rig-proven
---

# Locale Prefs: listview knob colour (A) and Cancel corners (B)

Owner report 2026-11-04: "the drag bars are not the correct color, it should be the same as the workbench
windows and the corners of the cancel button are cut off".

Palette on rig 75: pen 0 = 1a1b26 (back), pen 2 = a9b1d6 (text), pen 3 = 7aa2f7 (accent / active knob).
Locale Preferences window at 88,54 552x192, active during every measurement.

## A. Listview scroller knob

### Measured before the fix
- Locale listview props (flatprobe PROP lines): GAD 8 (242,83 10x18), 13 (612,139), 18 (426,83), 25 (242,139),
  all gflags 0006, PropInfo Flags 0x3d = AUTOKNOB | FREEVERT | PROPBORDERLESS | PROPNEWLOOK | SMARTKNOBIMAGE.
- The Workbench window's own prop has Flags 0x1d (no SMARTKNOBIMAGE).
- Knob boxes 242..251 x 83..100 and 612..621 x 150..175: every pixel pen 0, so the knob was invisible on the track.
- The AUTOKNOB Image has Depth 0 and NextImage = 0x1005956c, the same for all four props. That object is a
  BOOPSI image: Depth -1, class chain `frameiclass imageclass rootclass` (flatprobe KNOBOBJ / KNOBCLASS lines).

### Root cause
With SMARTKNOBIMAGE (V47), Intuition draws the knob through IM_DRAWFRAME on that shared frameiclass object, not through
the RectFill call that flat's scroller_pen paints in flat_knob_state_pen. flat's frameiclass patch (c8_skip) took the
call as an ordinary frame. It drew the frame as an edge-only box or a fill in pen_button, never in the knob pen.

### Fix
- `flat_smart_knob` (amarchy/flat_glyph.c) recognises the knob: a prop gadget with AUTOKNOB and SMARTKNOBIMAGE set,
  where the image is its AUTOKNOB Image's NextImage.
- `flat_knob_frame_draw` paints the knob as a box in `flat_knob_state_pen(depth, window active)`. This is the same pen
  as the Workbench window knob. It keeps the rounded corners of flat_frame_draw_round.
- c8_skip calls them through `smart_knob_of` (amarchy/flat.c).

### Measured after the fix (wbpenprobe 240 81 14 22)
- Box 242..251 x 83..100: pen 3 (7aa2f7), the active-window knob pen.
- Corners cut to the track pen 0 (1a1b26): rows 83 and 100 span 244..249, rows 84 and 99 span 243..250, rows 85..98 span 242..251.
- Workbench window knob in the same run (inactive window, stock RectFill path): pen 249 at 627..636, the same rounded cuts.

## B. Cancel button corners

### Measured before the fix (wbpenprobe 538 222 8 5)
- Cancel is the secondary (outlined) button at 540,224 88x14.
- The top row starts at x=542 and the left column at y=226.
- The inner diagonal pixel (541,225) is pen 0, so the 1 px outline is open at each corner.

### Root cause
flat_frame_draw_round takes three pixels per corner for a tall button (h >= 12): the corner pixel and one pixel on each
side of it. On a filled button that shape reads as a round corner. On the secondary role the outline is only 1 px wide,
so the cut removes the two outline pixels next to the corner and leaves a gap.

### Fix
For a tall secondary button that is not pressed, flat_frame_draw_round paints the inner diagonal pixel of each corner in
pen_text. This closes the outline into a rounded corner.

### Measured after the fix (wbpenprobe 538 222 92 18)
- Top-left: (540,224), (541,224) and (540,225) are pen 0 (the cut). (541,225) is pen 2 (a9b1d6). The row starts at
  542,224 and the column at 540,226: the outline is closed.
- Bottom-left: (541,236) is pen 2, the row at 542..,237 and the column to 540,235: closed.

## Tests
tests/test_flat_core.c has two blocks:
- S56-LOCALE-CANCEL-CORNER: a secondary 30x14 button has pen_text at the four inner diagonals, and the corner cut stays
  pen_back.
- S56-LOCALE-KNOB: flat_knob_frame_draw paints the box in the active and inactive knob pens with the corners cut;
  flat_smart_knob accepts flags 0x3d and refuses 0x1d, another object and a gadget that is not a prop.
