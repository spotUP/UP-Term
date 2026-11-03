---
date: 2026-11-03
topic: S32-INACTIVE-BAR - inactive windows get a visible dark grey dragbar
status: designed, queued after S32-TITLE-STRIP
---

# S32-INACTIVE-BAR

## Owner decisions (2026-11-03, session 32)
- Inactive windows must "really look inactive": a VISIBLE dragbar like the active one, in a dark grey tone.
- New colour of its own ("good to use a new color for it if people want to tweak it later").
- Title text and gadget glyphs on the inactive bar: the same pen as the active title text (pen_ptext).
- "All elements in the inactive window" take the bar colours: bar, gadget boxes on it, outer ring.

## Today (measured, s32-evidence/titlebar-strip.md)
- Active: title span band (flat_tx_c FLAT-TITLE-ACCENT, flat_title_decide) + on-band gadget boxes (flat.c frame_pen = flat_frame_pen) in pen_outline (accent 7aa2f7), text pen_ptext.
- Inactive: flat_frame_pen returns -1, flat_title_decide paints no band (text = dimtext), gadget boxes BACKGROUNDPEN: the bar is invisible (surface 1a1b26).
- Outer ring (FLAT-OUTER-EDGE, flat_rf_c) is pen_outline in BOTH states (inactive Shell shows 7aa2f7 ring).

## Design (symmetric with active, no new Intuition hook)
1. Theme: gtheme gets `inactive` (key `inactive` in [default]); derived when absent = gt_mix(bg, fg, 45 %) (Tokyo Night about #63687f). New role GR_INACTIVE in gt_roles_of (the one derivation; RL8 rule: no consumer picks its own colour).
2. Pen: desk obtains a pen for GR_INACTIVE (ObtainBestPen, same lifetime pattern as the knob pen in flat.c dim_refresh/dim_release, but desk side if desk owns the theme colours; implementer: reuse whichever owner already reads gt_roles) and writes flat_state.pen_inbar. Under 16 colours: pen_inbar = DK_MUTED pen. 0 = none = old behaviour.
3. flat_frame_pen(s, depth, active): inactive returns pen_inbar when it is a pen of the depth (same validity rule as flat_outline_of_depth).
4. flat_title_decide inactive: band_pen = bg_pen = flat_frame_pen(s, depth, 0), text_pen = pen_ptext when band >= 0, else dimtext as now.
5. flat.c gadget pen choice: the on-band branch runs for both states (band from flat_frame_pen with the real active flag); text_pen = pen_ptext. flat_clone_draw: when text_pen >= 0 the glyph takes text_pen for every non-selected state (today only FLAT_ROLE_TEXT, inactive is FLAT_ROLE_LINE).
6. Outer ring: flat_rf_c recolours to pen_outline when the window is active, pen_inbar when inactive (pen_outline when pen_inbar is 0).
7. Inactive window arrows + knob: arrow glyphs and knob take pen_inbar (owner: all elements). Active unchanged.
8. S32-TITLE-STRIP band_rows rule applies to both states.

## Tests (each fails on revert)
- gt_roles_of: GR_INACTIVE default mix and explicit key.
- flat_frame_pen inactive -> pen_inbar; pen_inbar 0 -> -1.
- flat_title_decide inactive: band drawn, text pen_ptext.
- flat_clone_draw inactive on-band: box = band pen, glyph = text_pen.
- outer edge pen choice per active flag (pure helper).
- Reachability: STATUS line prints inbar= pen (flat.c status dump, like knob=), rig 35 pixel check of an inactive window bar.

## Open
- Ghost dither replacement (s32-evidence/ghost-dither.md, research running) may change the inactive glyph path; read it before implementing step 5/7.
