---
topic: DOPUS-PATH-FIELD survey (path string gadget unstyled; resize leaves trails)
date: 2026-11-10
status: read-only survey, branch dopus-title-flat. V = read in code; U = unverified (inferred, not measured on a rig)
---

Paths: A = /Users/spot/Code/UP-Term/vtcon-dopus-title-flat/amarchy, T = .../vtcon-dopus-title-flat/tests, D = ~/Code/Reference/dopus5/source (vendor 5.104).

## A. What flat patches (the install table)
Table = install_all (A/flat.c:2603-2624) which calls hook_install (:2189), rf_install (:1881), tx_install (:1357), plus dopus_scan (:2268, called :2728/:2753) for the DOpus entries. Nothing else is installed (no Refresh, SizeWindow, EraseRect, backfill or layer hook anywhere: grep of Refresh|SizeWindow|EraseRect|BeginRefresh|Backfill in flat.c / flat_core.c / flat_glyph.c finds only comments; V).
1. sysiclass h_SubEntry (hook_install :2192-2198; c2_disp_reg/stk): window/screen system gadget images (close, depth, zoom, size, arrows) redrawn as flat glyphs (flat.c:82).
2. frameiclass h_SubEntry (hook_install :2203-2210; c8_disp_reg/stk :875-888, decision c8_skip :817): every IM_DRAW/IM_DRAWFRAME bevel (GadTools buttons, strings, listviews, cycles) replaced by flat fill+outline (flat_frame_decide, flat_core.h:230).
3. graphics.library RectFill LVO -306 (rf_install :1881-1893; flat_rf_c :1850): outer 1 px ring (rf_edge :1818), side strips (rf_side :1833), scroller fills, disabled-button dither, menu ghost, Installer border-button erase.
4. graphics.library Text LVO -60 (tx_install :1357-1366; tx_title :1347): window title band/text, pressed-button label pen, icon label antialias.
5. dopus5.library DrawBox LVO -696, DrawFieldBox LVO -702, and the internal bevel body (FLAT_EL_BEVEL) (dopus_scan :2296-2335, d4_drawbox :952, d4_fieldbox :957, d4_fieldbody :966, d4_bevelbody): flat outline instead of the shine/shadow L (d4_flat_pens :909-938 -> flat_dopus_box_draw, flat_glyph.c:529). Gated by dopus version/revision + routine checksums (flat_core.c:123-125 table: 73.8 = 5.104, 68.16 = 5.82). Body hooks are installed when the prologue matches (flat_body_install :2319), else LVO swap for DrawBox/FieldBox, nothing for BEVEL.
6. dopusiclass h_Entry (dopusiclass_slot :2175; d5_disp_reg, install :2340-2345): title-bar iconify/lock images only (flat.c:652, flat_dopus_image_which flat_core.c:297).
The header comment flat.c:17-20 ("path field ... not patched") is STALE: FieldBox body is hooked since 0c (flat.c:2312-2327, d4_fieldbody :966).

## B. How DOpus lister gadgets are recognised today
There is no DOpus-specific gadget-id recogniser. No GAD_PATH / gadget id test anywhere in flat (grep of GA_ID|GAD_|GadgetID in A: no hits; V). Recognition is by dopus5.library entry/class, window geometry, or generic gadget shape:
- Title-bar gadgets (iconify / lock / hijacked zoom): flat_title_gadget (flat_core.c:891-896): GTYP_SYSGADGET 0x8000, or Activation GACT_TOPBORDER 0x40, or (not GZZ, no RIGHT/LEFT/BOTTOMBORDER mask 0xB0, wholly in rows 0..BorderTop-1). Used by tx_span (flat.c:1272-1283) and flat_title_span_add (flat_core.c:872). Images drawn via dopusiclass hook d5 (type -> flat_dopus_image_which).
- Toolbar buttons, field boxes, Environment buttons: not by gadget; by call into dopus5.library DrawBox / DrawFieldBox / bevel body (d4_*; flat_dopus_decide flat_core.c:281, flat_dopus_ring :302).
- Scroller arrows (sysiclass image inside a buttongclass gadget): flat_img_record / flat_img_which table keyed by object (flat_core.h DOPUS-ARROW-WHICH; flat.c:496). Scroller track/knob: flat_gadget_is_prop (flat_core.c:1049) + rf_scroller (RectFill, ROM-caller gated).
- Frame/list borders on GadTools-like gadgets: gadget_of_image / button_gadget_of / push_gadget_of (flat.c:735-755), flat_button_gadget (flat_core.c:744): render == select == the frameiclass object.
- Status/info row ("D:[0/0] F:..."): NOT recognised; theme layer sets pens (menu_theme.c source/dest pens, per research 2026-11-09-dopus-lister-flat.md Fix item 2).
- Position / class of the path field: not tested.

## C. String-gadget handling
- The DOpus path field is class "dopusstrgclass" (D/Program/lister_window.c:355-358), GA_ID GAD_PATH, GA_Left = BorderLeft, GA_RelBottom = -(BorderBottom + font YSize + 4), GA_RelWidth, Height = YSize+4, GTCustom_ThinBorders TRUE. Dispatcher is button_dispatch (D/Library/libinit.c:1371), subclass of strgclass.
- GM_RENDER (D/Library/button_class.c:~879-975): thin: draw_field_box (:900) for the outline, then interior = rect inset 2; fills only the margin between gadget_box and interior with Extension->Pens[1] (ActivePens[1] when selected) by plain RectFill (button_class.c:~935-960); text drawn by strgclass with Extension pens. So: border = dopus5 FieldBox (hooked), margin fill + text pens = DOpus RectFill/strgclass, not hooked.
- In flat: NO code references GTYP_STRGADGET, StringInfo, strgclass, STRINGA_, GACT_STRINGLEFT. Checked: (1) the patch table in A (no strgclass/dopusstrgclass slot; classes hooked are sysiclass, frameiclass, dopusiclass only: class_slot callers flat.c:2165/2182), (2) the recogniser functions in B (flat_title_gadget, flat_button_gadget, flat_border_button, flat_gadget_is_prop, flat_push_button, flat_gadget_which: none test GadgetType & GTYP_STRGADGET (0x0004) or SpecialInfo), (3) grep `StringInfo|strgclass|GTYP_STR|STRING` in flat.c/flat_core.c/flat_core.h/flat_glyph.c: only comments (flat_core.h:225, :448, :476, :502). So not filtered out: absent. (flat_border_button explicitly requires type&7 == 5 custom gadget, flat_core.c:748-749, so a boopsi string gadget (type 5) with render NULL would also fail the Border-render test.)
- What does reach it: the thin FieldBox outline (d4_fieldbody) if dopus is gated in and the prologue matched (STATUS `dopus=73.8 patched=N`; U whether bit FIELDBOX is set on the owner's setup). Interior margin fill, text pens, selected state: unstyled.

## D. Resize / refresh behaviour
- No hook on RefreshGList, RefreshWindowFrame, Begin/EndRefresh, SizeWindow, EraseRect, layer backfill (A). Repaint of the lister on resize is entirely DOpus + Intuition; flat only recolours individual RectFills when they pass through the RectFill patch.
- Only repaint-ish code: RectFill patch flat_rf_c (flat.c:1850-1878). Branches, all computed from the CURRENT w->Width/Height (rp->Layer->Window, read at call time), none cached:
  - rf_edge (:1818): flat_outer_edge_fill (flat_glyph.c:714) matches ONLY exact rectangles of the 4 outer strips (outer_strip, flat_glyph.c:666; bottom strip = x 1..w-1, y h-1, 1 row, :672) and repaints them in edge pen widened to side_px columns left/right (outer_ring_strip :697).
  - rf_side (:1833): flat_side_fill_pen (flat_core.c:854-863): fg must equal pen_outline; rect must lie in left strip (x1 < BorderLeft), right strip (x0 >= ww-BorderRight) or bottom strip (y0 >= wh-BorderBottom); output clipped below BorderTop. Rows between the content and the bottom border (y < wh-BorderBottom) are excluded by definition, which is exactly where the path field sits (RelBottom -(BorderBottom+YSize+4), height YSize+4: rows wh-BorderBottom-YSize-4 .. wh-BorderBottom-1). Candidate for (2) only if some clear of that region passes the rom/pen gates; otherwise no flat code touches it.
  - Both gated by `rom` = caller return address in Intuition ROM (flat.c:1862-1863, :1873-1877). DOpus's own clears (lister_init_lister_area, path field margin RectFills, RefreshGList of the path field D/Program/lister_display.c:301-303) come from dopus5/DirectoryOpus code, so they go to the saved RectFill unchanged (:1878).
- Places using old or excluded geometry: none found that cache an old size (no stored window size in flat_rf_c / rf_* / side_px; side_px caches per screen only, flat.c:1494). Whether Intuition issues the ring/side RectFill before or after updating w->Width/Height during a size operation: UNVERIFIED (not measured; would decide whether flat_outer_edge_fill's exact-match sees the old strip rectangles and misses the new ones). Static finding: the exact-match design (flat_core exact rect equality) fails silently for any rectangle that is not precisely the current ring strip, e.g. a ring strip drawn with the OLD size during the resize.
- Trails candidate: with flat on, the 2 px flat outline of the path field (d4_flat_pens -> flat_dopus_box_draw) is drawn at the old position on a previous refresh; DOpus after a resize repositions the RelBottom gadget and RefreshGList's it, but old pixels outside the new gadget box are only cleared by whatever Intuition/DOpus clears (the layer region exposed by SizeWindow). Flat never clears. U.

## E. Existing host tests
- A tests file: T/test_flat_core.c (3431 lines), run via the Makefile `ONLY=flat_core` target (Makefile:87), source listed Makefile:58. Role logic: T/test_flat_role_core.c. Rig/py: T/test_flat_budget.py, test_flat_clone_stack.py, test_flat_pens_per_screen.py etc. (not gadget recognition).
- Closest to this bug:
  - test_dopus_box_outline (T/test_flat_core.c:520): flat_dopus_box_draw on a char-grid canvas (cv_clear/cv_fill, CV x CV, :446-470 region), asserts outline columns, side=2 tall-pixel, under-3x3 draws nothing.
  - test_dopus_no_ring (:553): face-pen band rule mirroring d4_flat_pens.
  - test_dopus_decide (:414), test_dopus_path (:457; fake slot table + patched_call counts g_skipped/g_stock), test_dopus_body_hook (:322), test_dopus_bevel_hook (:276).
  - test_title_gadget_filter (:587): builds `flat_gadrect` structs (x,y,w,h,relright,relwidth,drag) and calls flat_title_gadget(type, activation, gzz, &rect, bar_h); pattern for a gadget recogniser test. A path-field recogniser test would build gadget type/activation/geometry arguments the same way (no fake Intuition Window/Gadget chain exists in the host tests; the pure functions take scalars, A/flat.c's window walks are not host-built).
  - Repaint rectangles: flat_outer_edge_fill asserts at T/test_flat_core.c:1677-1687 (left/right/top/bottom strips, interlace, clamp); flat_side_fill_pen asserts :2476-2480 (inside the block that starts in test_inactive_pens at :2464 by grep; enclosing function name may differ: UNVERIFIED), signature flat_side_fill_pen(&s, fg, ww, wh, bl, bt, br, bb, x0, y0, x1, y1, out).
- No existing test for string gadgets or for the path-field rows.
- The coupling code that walks Window/Gadget lists (role_of, gadget_of_image, border_button_at, tx_span, flat.c:693-790, :1272) is not host-testable today; reachability tests are rig py (tests/amiga/flatwin.c, flatprobe.c build probe windows).

## F. Repo notes on the path field
- A/flat.c:17-20 (stale, see A).
- thoughts/shared/research/2026-10-29-amarchy-flat-ui-patch.md:202 (table row: "Path field ... draw_field_box (Library/gui.c:60, called from button_class.c:90x): row 2 above and 2 below -> 0; pen-1 bottom row and left column stay: L shadow").
- thoughts/shared/plans/2026-10-29-amarchy-flat-ui-patch.md:137, :266, :268 (owner decision patch DOpus DrawBox; path field "not reachable by LVO ... remaining route: gated byte patch of library code" -> since done as the 0c body hook).
- thoughts/shared/research/2026-11-09-dopus-lister-flat.md:34 and its Fix (s66) section (no path-field fix; lists unreached: path field, scroller knob/arrows, sort arrow).
- thoughts/shared/s66-evidence/0c-dopus-gadget-outline.md:19 (jsr 0x245de FieldBox body at 0x18d68 in fn 0x18b78 = string gadget), :94; s66-evidence/0c-dopus-env-flat.md:34; s65-evidence/dopus-env-unstyled.md:14.
- No note mentions resize trails or clearing the path field area: NOT FOUND.

## Root-cause candidates
(1) Unstyled path field. a) No hook for dopusstrgclass: only the outline goes through FieldBox body (flat.c:966; D/Library/button_class.c:900); the margin fill (Extension->Pens[1]) and text pens are DOpus's, never recoloured, and nothing in flat recognises GAD_PATH / GTYP_STRGADGET (C). b) The FieldBox body hook may not be installed on the owner's build: gate by version+checksum (flat_core.c:123-125, flat.c:2296-2335) and prologue (flat_body_prologue); check STATUS `dopus=... patched=` bit 1.
(2) Resize trails. a) Flat repaints bottom/side strips only for ROM-caller RectFills whose rect exactly equals the current ring strip or lies in the borders (flat.c:1873-1877, flat_core.c:854-863, flat_glyph.c:693-714), so the path-field rows (above BorderBottom) are never cleared by flat and its flat outline from the previous size (d4_flat_pens :909) leaves trails unless DOpus clears them. b) The exact-rect matcher uses the window size at call time; if Intuition fills the ring before the size update, the match misses (U, unmeasured).
