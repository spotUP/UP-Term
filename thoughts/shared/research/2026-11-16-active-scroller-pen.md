# Active-window scroller/arrow blue vs inactive grey (Patina flat look)

## Cause (read in code)
- patina/flat_core.c:1088-1094 `flat_knob_state_pen(s, depth, active)`: active returns `pen_outline` (the accent = active title-band colour, blue in Tokyo-style themes), inactive returns `flat_inactive_pen` (pen_inbar, grey). This is a deliberate owner decision "S32-KNOB-STATE" (thoughts/shared/handoffs/2026-11-03_patina-session32-gadget-clone-rl8.md:60, thoughts/spot/todos.md:3686: active knob 7AA2F7, inactive 5A5E75, track 1A1B26).
- Consumers of that one function (all follow it):
  - scroller knob fill: flat_core.c:1138-1146 `flat_scroller_pen_box` (knob rect -> flat_knob_state_pen; rest of container -> pen_back track, same in both states).
  - arrow/size gadget glyph pen: flat.c:412 `p->knob = flat_knob_state_pen(...)` (comment S32-ARROW-KNOB-COLOUR: arrows draw in the knob pen).
  - knob frame round/AA draw: flat_glyph.c:699 `flat_knob_frame_draw`, roundaa_core.c:195.
- Arrow glyph colour extra: flat.c:420-421 sets `p->text = flat_inactive_pen` only when NOT active; active arrows keep stock/pen_ptext text pen (inference on the exact glyph colour; not traced to pixels).
- Not the cause: Intuition's FILLPEN fill of active border strips is already neutralised by `flat_side_fill_pen` (flat_core.c:1166, flat.c:2500 `rf_side`, desk_core.c:91-93) which repaints sides in pen_back; title band uses `flat_frame_pen` (flat_core.c:1181) = pen_outline and is a separate function. No theme field exists for an "active border gadget fill": gthemes_core.c:547-551 has only title-bar (FILLPEN), title-text, border (SHINEPEN); inactive colour is theme role `inactive` -> pen_inbar (flat.c:3649).
- Track (container) is pen_back in both states (flat_core.c:1146), so "blue container" is probably the knob/box only (inference: not verified on a rig; no screenshot read).

## Fix options
1. (Recommended) Change the active branch of `flat_knob_state_pen` (flat_core.c:1090-1093) to return the same pen as inactive: `flat_inactive_pen(...) >= 0 ? that : flat_knob_pen(...)` regardless of `active`. One-line change, fixes knob fill, arrows (flat.c:412) and knob frames (flat_glyph.c:699, roundaa_core.c:195) together; title bar (flat_frame_pen) and window ring untouched. Trade-off: reverses S32-KNOB-STATE, so active and inactive scrollers look identical (focus is still shown by title band + ring); also the active arrow glyph pen path (flat.c:420) may still differ from inactive text pen, so drop the `!wactive &&` condition there if arrows' glyph should match too. Must update tests/test_flat_core.c checks at 1128, 1598-1599 (on != off assertion), 2139-2154 (kn expected for active), which encode the old rule.
2. Add a theme role (e.g. `scroller-active`, in gthemes_core.c role table near 547-551 + flat_state field, plumbed like pen_inbar via flat.c:3649/flat_core.c:1320) so themes choose. Trade-off: much more code (pen allocation, theme files, tests) for a colour owner wants equal to inactive.
3. Leave knobs, only fix arrows: set `p->text` for active too (flat.c:420). Smaller, but knob stays blue; does not meet the report.

Unchecked: no rig/pixel look, no 3.1 vs 3.2 difference, pen_inbar availability under 8 colours (falls back to flat_knob_pen via flat_inactive_pen -1 path).
