# Installer buttons / bottom border / inactive border: code read (no rig, no measurement)

Paths relative to vtcon/amarchy/. READ = seen in source this session. GUESS = inference, not in source.

## 1. Pens (READ unless marked)
- desk_core.c desk_pens_flat (~l.78-85): DK_SHINE = DK_SHADOW = surface pen (GR_SURFACE -> DK_BG). Comment: "FLAT-SOLID-FILL: highlight, shadow and the
  active frame fill all take the surface pen, so every window is one solid rectangle". desk_core.h:30-33 repeats it ("bevels vanish").
  Also DK_BACKGROUND = surface (desk_pens). So SHINEPEN == SHADOWPEN == BACKGROUNDPEN is INTENTIONAL (the OD-F4 "flat signal"; flat_core.h:163).
- DK_FILL (FILLPEN) stays the accent: desk_core.c desk_pens (`pens[DK_FILL] = desk_pen_of_role(GR_ACCENT)`), comment desk_core.c:90-92 (S57-LOCALE-LIST-NO-SELECT).
  DK_FILLTEXT = surface. DK_BARBLOCK = surface, DK_BARDETAIL = text.
- Role -> pen index: desk_core.c desk_pen_of_role table: SURFACE=DK_BG, TEXT=DK_FG, MUTED/INACTIVE=DK_MUTED, ACCENT=DK_ACCENT, SELECTION=DK_SELECTION.
  The RGB for tokyo-night behind those DK_* indexes is in the theme (gthemes_core.c); not read here (GUESS: surface 1A1B26, accent 7AA2F7).
- Inactive frame pen is NOT a DrawInfo pen: flat_inactive_pen (flat_core.c:662) = s->pen_inbar (theme `inactive`, set by desk_flat_inbar) if depth 4..8,
  else pen_dimtext (DK_MUTED, depth>=3) else -1. Active ring pen = pen_outline = accent (desk_flat_state, desk_core.c ~l.119).

## 2. Symptom A: buttons label only
Bevel suppression path (READ):
- flat.c:509-640 c8_skip (frameiclass hook, IM_DRAW 0x202 / IM_DRAWFRAME 0x206). Gate: flat_frame_decide (flat_core.c:204-210): enabled && !held &&
  SHINEPEN == BACKGROUNDPEN. Under tokyo-night that is true (b10 measure), so the stock bevel is SKIPPED and replaced by flat.c:603 raa_frame_draw
  -> flat_frame_draw_round (flat_glyph.c:481) -> flat_frame_draw (flat_glyph.c:408-436):
    * fill (IA_EdgesOnly == 0): solid box, pen_button (DK_SELECTION) for PLAIN role, pen_line (accent) primary, pen_text pressed; no outline (l.428-430).
    * edges only: 1 px outline in pen_line (accent) (l.432-435).
  So buttons DO get a flat box, but only through frameiclass.
- If raa_frame_draw returns 0 (flat_frame_draw_round returns 0 for w<3 or h<3, flat_glyph.c:484), c8_skip returns 0 (flat.c:604-605) and the STOCK frame is
  called with SHINE = SHADOW = BACKGROUND: invisible bevel, label only. c8_skip also returns 0 when imp_RPort or imp_DrInfo is NULL (flat.c:563).
- Only frameiclass is hooked (flat.c:1514 frameiclass_slot, 1532). No hook on gadtools.library DrawBevelBoxA, bevel.image, or DrawBorder/DrawImage
  (grep bevel/DrawBevelBox in flat.c: only frameiclass hits). The DOpus DrawBox/DrawField patch (d4_flat, flat.c:~640) is dopus5.library only.
- MOST LIKELY (GUESS): Installer 43.3 buttons on 3.x are drawn by something other than frameiclass (bevel.image / bevelclass / DrawBevelBoxA line draws
  with SHINEPEN/SHADOWPEN). Those run with SHINE=SHADOW=BACKGROUND -> every bevel line is body colour, nothing replaces them: label only.
  Second candidate (GUESS): frameiclass is hit but the box is 1A1B26-equal: PLAIN-role fill uses pen_button = DK_SELECTION via flat_button_pen
  (flat_core.c:574): returns pen_back (= body) when pen_sel is 0, depth <3 or >8, or pen_sel >= 1<<depth (flat_core.c:577-579). A screen where
  pen_sel is unset gives a fill equal to the body -> invisible button. fill=0 path: outline pen_line = accent, visible.
- Not gated by screen type: no "Workbench only" test found in c8_skip; gate is per-screen pens (st_scr) + enabled + !held.

## 3. Symptom B: no bottom border row, no size gadget
- The ring is not drawn by a window-owned routine. It is painted by the graphics RectFill patch (flat.c:1180-1250, flat_rf.s) only when the caller
  returns into intuition ROM (ret in irom_lo..irom_hi, flat.c:1229) AND window not WFLG_BORDERLESS AND the rect is EXACTLY one of the four strips of
  flat_glyph.c:525-531 outer_strip (top (0,0)-(w-1,0), left, right, bottom (1,h-1)-(w-1,h-1)); flat_outer_edge_fill (flat_glyph.c:573) -> pen
  flat_edge_pen (flat_core.c:674): active = pen_outline (accent), inactive = inactive pen. Always 1 px bottom (px widening only left/right, outer_ring_strip).
- Any other fill falls to flat_side_fill_pen (flat_core.c:683-692): needs rp->FgPen == pen_outline (FILLPEN accent) and the rect inside the border band
  (x0<bl, x1>=ww-br, or y0>=wh-bb); repaints it in pen_back. It reads w->BorderBottom (flat.c:1241).
- Stock border pens: with SHINE=SHADOW=BACKGROUND (and FILL accent only on the ACTIVE window) the stock bottom border band is body colour.
  So a bottom border that is not caught by the 1 px ring strip or the FILLPEN side fill is body-coloured = indistinguishable from the body, i.e. "no row".
- No flat code reads WFLG_SIZEGADGET. strut_patch.c / tile.c read it only for layout. No gate skips the bottom edge for windows without a size gadget (READ).
  But the strip match is exact-rect: if Intuition on that window draws the bottom border with a different rect (GUESS: size/ BorderBottom=0 or a
  window opened with a custom border like Installer's), the ring RectFill never matches and nothing is painted.
- "Black below the body" is not pen 0 (pen 0 = 1A1B26 per b10). Black = outside the window (or an unfilled layer). GUESS: Installer window is shorter than the
  body fill (BorderBottom 0, or WFLG_BORDERLESS bottom), so the missing row is the window's own height, not a pen.
- borderblank_patch.c is unrelated (blanks the chipset overscan border of native-mode screens via VTAG_BORDERBLANK_SET; borderblank_core.c bb_native_mode).

## 4. Symptom C: inactive windows
- Inactive ring: flat.c:1234 flat_edge_pen -> flat_inactive_pen (flat_core.c:662) = pen_inbar if depth 4..8 and pen_inbar < 1<<depth. Fallback pen_dimtext
  (DK_MUTED), else -1 -> pen_outline (flat_edge_pen l.676-677, i.e. falls back to the accent, not to body).
  pen_inbar comes from the theme `inactive` colour (desk_flat_inbar) or derived (flat_inbar_colour: gt_mix(surface,text,GT_INACTIVE_PCT)).
  A visible border on inactive windows therefore depends on that colour differing from 1A1B26; its RGB was not read. GUESS: if the mix is a few percent
  the ring is faint but not equal to the body.
- Stock inactive FILL is BACKGROUNDPEN (body colour), so any stock inactive border paint that the ring patch does not rewrite is invisible.
  Same exact-strip / ROM-caller gate as section 3 applies to inactive windows.

## Rig measurement to confirm (read only, via a probe or DOpus/Snoopy-free)
1. Installer window: Window->Flags (BORDERLESS? SIZEGADGET?), BorderBottom, Height, WScreen. Expected cause of B if BorderBottom==0 or BORDERLESS set.
2. Pixel read (ReadPixel) of the row Window.TopEdge+Height-1 across the width, and the row below it, for an Installer window and a Shell window with size gadget.
3. STATUS rhits/flat counters of amarchy-flat before/after opening the Installer: do rhits rise (ring RectFill matched) or only shits?
4. For A: STATUS "frame=hooked calls=N flat=N" delta while the Installer opens. calls flat = 0 means the buttons are not frameiclass (bevel.image / DrawBevelBox),
   calls rising and flat rising with body-colour buttons means pen_button == pen_back (read s->pen_sel/pen_button, depth, IA_EdgesOnly via GetAttr).
5. For C: dri pens of a probe plus pen_inbar RGB (GetRGB32 on pen_inbar vs pen 0) and ReadPixel on the inactive window's left/top edge.
