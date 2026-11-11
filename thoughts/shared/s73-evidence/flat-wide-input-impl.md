---
date: 2026-11-11
topic: FLAT-WIDE-INPUT implementation evidence
branch: flat-wide-input
design: thoughts/shared/s72-evidence/flat-wide-input-design.md (main checkout)
---

# FLAT-WIDE-INPUT implementation evidence

## Scope
DISPLAY-class ground boxes shrink to fit their text (pad 6*side). Typed inputs,
slider tracks and day cells keep full width (fit flag 0). Empty display box
keeps the full ground box.

## Tests (nice -n 19 make test ONLY=flat_core)
- Fail before: 20 compile errors (flat_gr_add arity, FLAT_FIT_PAD,
  flat_fit_box, flat_fit_strip, flat_gr_fit undeclared).
- Pass after: `[OK] flat_core: 4411 checks, 0 failed`.
- Fail on revert (flat_fit_box forced to 0, flat_gr_fit no-op):
  `[FAIL] flat_core: 4411 checks, 21 failed`, lines 3749..3827 (cases 1-10).
  Case 11 (empty box keeps full ground) passes on revert by design: it asserts
  the unfitted behaviour.

## Budget (make flat-budget, PASS)
- _flat_rf_c own frame 64, deepest chain 320, headroom 64
- _flat_tx_c own frame 48, deepest chain 316, headroom 68 (limit 64 held)

## Build
build/amiga/amarchy-flat: 239792 bytes, vbcc -warnings-as-errors clean.

## Reuse / search
No clipping fill existed (searched c2_fill, c2_ring_fill callers in flat.c,
flat_core.c, roundaa_core.c, flat_glyph.c). fit_fill clips and calls
c2_ring_fill (ring installed) or c2_fill. Reused: c2_read, c2_rgb, raa_load,
raa_ground_frame_draw, raa_radius, flat_inset, flat_gr_match, the
flat_on_stack/cl_ss/cl_sss pattern from c8_draw_on.

## Answers
- TEXT_KIND: by source a frame whose box equals a gadget box classifies
  BORDERED, one holding a gadget GROUP; neither reaches DISPLAY, so no fit.
  That GadTools creates a real Gadget for TEXT_KIND is autodoc knowledge,
  unverified on rig.
- Side 2: x inset, pad (6*side) and tolerance (2*side) scale with side; y inset
  stays 1; corner radius comes from height (raa_radius). Centred 17 chars at
  side 2 test: 86..245. Verified by source and test, not rig.

## Deviations from design
- Case 9: design "62..317" holds only for clip 14..317; clip 234..317 gives
  234..317. Test uses both.
- Case 5: right gap 1, not 0.
- Added flat_gr_erase: an erase RectFill covering all recorded text resets the
  text span, so a slider step without frame redraw refits 96..235 instead of
  keeping the union 92..239.
- Left band empty when ix0 == x0.
- Strip safety relies on raa_radius - 1 < pad*side.
- flat_shared.h gains gfit, gfitbusy at the end of the struct: amarchy-gui
  and amarchy-flat must come from the same tree.

## Rig agent must check (side 1, Time Prefs)
1. Date row box 92..239 on open (17-char date).
2. Slider step to a 16-char date: box 96..235, no stale ground outside.
3. Day click redraw: box stays fitted, no full-width flash left behind.
4. Status line gfit increments, gfitbusy stays 0 or low.
5. Typed string gadgets (e.g. Locale/Serial inputs) keep full width.
