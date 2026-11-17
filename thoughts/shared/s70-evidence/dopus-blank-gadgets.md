---
topic: DOpus 5 lister title-bar gadgets (ids 10/11/13) draw blank under amarchy-flat
status: root cause found by code reading (V); rig 68 confirmation see "Rig" section; no code changed
---
# DOpus blank title gadgets (s70)

Paths: A = vtcon/amarchy/, D = ~/Code/Reference/dopus5/source (vendor 5.104 source). V = read in code / measured, U = inferred.

## (a) What the three gadgets are
- ids from D/Program/lister.h:90-99 enum: GAD_ICONIFY=10, GAD_LOCK=11, GAD_GAUGE=12, GAD_ZOOM=13. V
- id 10 (x=373) iconify: D/Program/graphics.c:150 create_iconify_gadget -> CreateTitleGadget(IM_ICONIFY). V
- id 11 (x=349) lock: D/Program/lister_window.c:150-156 CreateTitleGadget(IM_LOCK), offset -iconify width. V
- On 68k (no MORPHOS/OS4/AROS) both images are NewObject("dopusiclass", DIA_Type type) (D/Library/boopsi.c:476-506),
  gadget class dopusbuttongclass, GA_RelRight + GA_TopBorder (boopsi.c:510-528). NOT sysiclass, so A/flat.c c2_draw never
  sees them (it hooks only the sysiclass dispatcher). V
  - IM_ICONIFY draw: D/Library/image_class.c:307-330: RectFill FILLPEN (active) / BACKGROUNDPEN (inactive) inside the box,
    inner square in SHADOWPEN (old style, :340-346). V
  - IM_LOCK draw: image_class.c:498-540: RectFill FILLPEN/BACKGROUNDPEN, padlock outline in TEXTPEN. V
- id 13 (x=398) zoom: the window's own Intuition zoom gadget (sysiclass ZOOMIMAGE). DOpus "hijacks" it after OpenWindow:
  D/Program/lister_window.c:307-333 clears GTYP_SYSTYPEMASK|GTYP_SYSGADGET from GadgetType and sets GadgetID=GAD_ZOOM. V
  That is why uitree lists it as "custom" and shows no sys:zoom (s69 table). V (code), consistent with s69 uitree.

## (b) Why nothing draws: flat's title band paints over them (root cause)
- FLAT-TITLE-ACCENT: on the window-title Text call, flat_tx_c -> tx_title (A/flat.c:1276-1287) -> tx_title_draw
  (A/flat.c:1252-1273) RectFills the band pen over bx0..bx1 x rows 0..BorderTop-1 via the saved RectFill. V
- The span comes from tx_span (A/flat.c:1237-1249), which folds ONLY gadgets with `GadgetType & GTYP_SYSGADGET`
  (:1244). V
  - lock/iconify are app boopsi gadgets (no SYSGADGET) -> ignored. V
  - the hijacked zoom had SYSGADGET cleared by DOpus -> ignored. V
  - so sx1 stops at depth.LeftEdge-1 = 420 and the band covers x=21..420 including 349..420, exactly the s69 measured
    all-accent run x=335..420 rows 14..27 (thoughts/shared/s69-evidence/dopus-dividers.md "Rig 68 measurement"). V (code + s69 pixels)
- The span contract itself is right: flat_core.h:364-373 "the band spans only the title area between the system gadgets
  ... so it never wipes the close / depth / zoom gadgets Intuition draws once and does not redraw". The invariant
  "the band never covers a title-bar gadget" is violated because the caller's filter equates "title gadget" with
  "system gadget". flat_title_span_add (A/flat_core.c:863-878) already handles any gadget (relright, y < bar_h). V
- Not the pens: active DOpus images fill FILLPEN (= accent, same as band) but draw the square in SHADOWPEN (flat pen 0, dark)
  and the lock in TEXTPEN, both visible on accent if they survived. So pens alone cannot give a fully blank 24x14. V (code),
  U on the exact flat pen numbers of rig 68's screen (taken from research 2026-11-09 sec.1 dump).
- Order: Intuition draws border gadgets, then the title Text is drawn (or redrawn on activate/SetWindowTitles, which does not
  re-render GA_TopBorder app gadgets) -> the band lands last. U (ordering inferred; see Rig).

## (c) Fix (root-cause layer: flat hook, A/flat.c tx_span; no DOpus patch, no new painter)
Step 1 (the bug): tx_span folds every gadget in the top border, not only system ones:
  `if ((g->GadgetType & GTYP_SYSGADGET) || (g->Activation & GACT_TOPBORDER))`
  and tx_gad's drag test becomes `(g->GadgetType & GTYP_SYSGADGET) && (type & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING` so an
  app gadget whose type nibble happens to be 0x20 is not taken as the drag bar. Keep the ng < 8 cap counting only folded
  gadgets (DOpus lister: close, depth, hijacked zoom, iconify, lock = 5; drag excluded by flat_title_span_add). The span
  then ends at 348 (lock left - 1) and the three gadgets keep what they drew. Same fix covers any app that puts
  GACT_TOPBORDER gadgets in its title (MUI iconify/snapshot, ReAction). Pixel aspect: unaffected (span is in pixels; no glyph).
  Better still (single source): move the filter into flat_core as `int flat_title_gadget(unsigned gtype, unsigned activation)`
  so the host test owns it.
Step 2 (look, optional, separate commit): after step 1 the gadgets show DOpus/ROM art (dopusiclass square + ROM 3.1 zoom with
  flat pens), not flat glyphs. To give them the flat style through the EXISTING path:
  - zoom (id 13): it is still a sysiclass image; c2_which_31 (A/flat.c:598-610) misses it on KS 3.1 because
    flat_gadget_which (A/flat_core.c:350-355) needs GTYP_SYSGADGET. Fix: record window-gadget whichs at OM_NEW too
    (flat_img_record A/flat_core.c:357-360 accepts only 0x0A..0x0D arrows; widen to DEPTH/ZOOM/SIZE/CLOSE) so img_lookup
    resolves it; then the normal clone/peel (flat_clone_route -> c2_clone) or c2_glyph (side_px of the drawing screen) runs. U
  - iconify/lock (dopusiclass): would need a hook on dopusiclass's dispatcher (FindClass("dopusiclass") after dopus5.library
    opens, same flat_swap model as the DrawBox LVO patch in flat.c header :15-20) mapping IM_ICONIFY -> FLAT_W_ICONIFY
    (g_iconify, A/flat_glyph.c:84) and IM_LOCK -> a new FLAT_W_LOCK glyph, drawn by flat_glyph_draw with
    side_px(w->WScreen). Larger change; owner decision whether worth it. U

## Test plan (tests/test_flat_core.c)
- Pure: test_title_span_app_gadgets: DOpus lister geometry (win_w 444, bar_h 14): close {0,0,20,14}, depth {-23,0,24,14,relright},
  zoom-hijacked {-46,..}, iconify {-71,..}, lock {-95,..} (exact LeftEdges from uitree: 349/373/398/421 - 444); with the new
  flat_title_gadget filter applied, flat_title_span gives x0=20, x1=348; fails today (x1=420). Fails again when the filter reverts
  to SYSGADGET-only.
- Filter unit: flat_title_gadget(0x0005 custom, GACT_TOPBORDER)=1; (0x0005, 0)=0; (0x8040 sys depth, 0x?)=1; drag only when SYSGADGET.
- Reachability (one): drive flat_tx_c / tx_title with a fake Window whose gadget list is the lister set above and a fake rastport;
  sentinel = the saved RectFill stub records (x0,x1) of the band call; assert x1 == 348 and call count 1. NOTE: tx_* live in flat.c
  (Amiga-only); if the host harness cannot link flat.c, the reachability test is the rig check below, and the host test drives
  flat_title_span_add through the extracted filter. Step 2 (if done) gets the c2_draw reachability test: fake sysiclass image
  recorded at OM_NEW with which=ZOOMIMAGE on a gadget with SYSGADGET cleared, c2_draw returns 1 and the glyph-draw sentinel counts 1.
- Rig: fresh lister after flat, ami.grab rows 14..27 x=349..420 must show non-accent pixels inside each of the three gadgets
  (iconify square, padlock outline, zoom art); before the fix all accent.

## Rig (s70)
- rig 68 was down; `rig.py start --purpose "s70 session: DOpus blank title gadget root cause"` hung at "boot ...: (go not
  started)" and wasabid did not answer ping, so no new measurement this session. The verdict rests on code + the s69 pixel
  scan. The FS-UAE window of rig 68 may still be open (not stopped: stop deletes build/rig68). Rig proof still owed:
  click lock gadget id 11 on an active lister (GM_RENDER of that gadget alone, no title Text) -> padlock should appear; then
  re-activate the window -> blank again. That separates "band overwrote it" from "never drew". U
- Unverified also: that Intuition's zoom gadget carries GACT_TOPBORDER after DOpus clears its type (if not, step 1 must also
  fold gadgets by geometry: TopEdge < BorderTop, which flat_title_span_add already checks).
