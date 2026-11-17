# DOpus lister outline breaks beside the scroller arrows (s71 agent 33, rig 68, build 5ade172e)

## Measured (pixel reads of 800x600 shots; palette: ring active #7AA2F7, inactive #545A77 on this rig, bg #1A1B26)
Lister System DH0: (0/0/444/574, right edge x=443, bottom row y=573) and Rushhours DH1: (446/0/354/574, right edge x=799, bottom y=573).
| state | right column | bottom row |
| System inactive after `dopus refresh all` (patcher on) | y 520-541 present, **542-563 absent (22 px)**, 564-573 present | x 383-393 present, **394-425 absent (32 px)**, 426-443 present |
| System active, same refresh | 520-573 present | 383-443 present |
| Rushhours inactive after `dopus refresh all` | 542-563 absent (22 px) | 750-781 absent (32 px) |
| Rushhours active | intact | intact |
| System inactive after Break C + refresh (stock) | continuous | continuous |
| System inactive, patcher restarted + refresh | 542-563 absent, 394-425 absent (again) | |
| Inactive lister that BECAME inactive by activating the other one (frame redraw only, no refresh) | intact (545A77 continuous) | intact |
Same numbers as the owner's screenshot (22 px beside the up/down arrows, 32 px under the left/right arrows, L under the size gadget intact).
Both listers show it, but only while inactive and only after a gadget-image redraw (`dopus refresh all`). Activating a lister repaints its frame and heals it; deactivation by activating another lister does not break it.

## Cause (read from source; the measured events fit it, the function was not instrumented)
Arrows and size are not cloned since S69-ARROW-ASPECT (flat_glyph.c:250 flat_clone_route excludes them), so c2_draw (flat.c:645) falls to c2_glyph (flat.c:~593-603) -> flat_glyph_draw -> plan_run, which starts with `put(g, 0, 0, w-1, h-1, g->bg)` (flat_glyph.c:~450): a fill of the WHOLE gadget box in the frame/ground pen (BACKGROUNDPEN of the window, c2_get_pens flat.c:~410). A DOpus lister's arrow boxes reach the last column / bottom row, so that fill paints over the window's outer ring (the flat outline). Nothing repaints it: Intuition repaints the four outer edges only in a full frame redraw (activation), and a gadget refresh (DOpus lister refresh) does not.
The ring-protection code exists only on the clone path: c2_clone calls c2_ring_refill (flat.c:359-374, line 583) which refills the overlap through flat_outer_ring_overlap (flat_glyph.c:725) with flat_edge_pen. The glyph path c2_glyph never calls it. Introduced by S69-ARROW-ASPECT moving arrows off the clone route (the S32-ARROW-OUTLINE fix, 0f5e169d, was written for the clone route).
Active windows: the arrow box is filled in the knob pen, which equals the ring colour on this rig, so the same overwrite is invisible (gaps only where the box has its background rows between the blocks; seen as 1-2 px gaps in a first active shot at x=441/442 - not re-measured). Inactive: ground pen != ring pen, so the line disappears over the full gadget length.
Size gadget corner L survives: not explained by measurement. The unit test (test_flat_core.c:1721) says the size box overlaps right + bottom ring, yet the ring under the size gadget is intact on the rig, so either the DOpus size box stops 1 px inside the corner or something repaints it. UNVERIFIED.

## Violated invariant
The 1 px outer ring belongs to flat's edge pen in every state; no gadget draw may leave a different pen on it.

## Minimal fix
In c2_glyph (flat.c) call c2_ring_refill(r, d, im) after flat_glyph_draw returns drawn (same call as c2_clone line 583; d5_draw uses c2_glyph too, harmless). Better still: make the box fill itself clip -- but reuse of c2_ring_refill is the established pattern.
## Regression test
Reachability: drive the image dispatcher c2_disp_reg path with a stub rastport (existing flat_hook harness, if present) on an inactive window, assert the last RectFill on the ring strip uses flat_edge_pen (inactive pen) and a call-count sentinel on c2_ring_refill. Pure part already covered: tests/test_flat_core.c:1714-1726 (flat_outer_ring_overlap); add a case "arrow box 622..639 x 214..224 on an inactive state: flat_edge_pen != frame pen" and a test that c2_glyph routes for FLAT_W_UP.. (needs the hook seam; not designed here).

## Unverified
Function not instrumented (no log in c2_glyph); the active-state 1-2 px gaps at x=441/442 not re-measured; size-gadget survival; arrow box rects not read from the rig.
Deleted: all a33-*.png shots in the scratchpad. Rig end state: RAM:amarchy-flat resident (flat=on), 2 listers (System front/active, Rushhours behind); two stray listers my `lister new` created were closed.

## Fix and proof (s71 agent 34)
Commit 7241d8d1 on branch dopus-border-break (worktree vtcon-dopus-border-break, not pushed, not merged). Cause confirmed in source: c2_glyph returned flat_glyph_draw directly (plan_run fills the whole box in the ground pen) and never refilled the ring; only c2_clone did.
Diff: amarchy/flat.c c2_ring_get (new, builds the ring description), c2_ring_refill(r,d,im,drawn) (flat.c:373), c2_glyph calls it after flat_glyph_draw (flat.c:612), c2_clone passes 1; amarchy/flat_glyph.c/.h new flat_ring + flat_ring_refill_after (the one pure decision: drawn and a ring given, then flat_outer_ring_overlap); tests/test_flat_core.c test_glyph_keeps_window_ring. A first variant (wrapper flat_glyph_draw_ring nested above flat_glyph_draw) blew the stack budget (_c2_disp_reg headroom 0 < 64); the sequential form keeps flat-budget PASS.
Test: with the refill disabled in flat_ring_refill_after: "FAIL test_flat_core.c:1271: rect_log_pen_at(&l, 639, 220) = 70, want 69", :1272, :1276, "[FAIL] flat_core: 4214 checks, 3 failed". With the fix: "[OK] flat_core: 4214 checks, 0 failed". Limit: the test covers the decision and the glyph-then-refill sequence; that c2_glyph itself makes the call is proven by the rig only.
Binary build/amiga/amarchy-flat 227660 bytes, md5 80462b4b0e998686897b8aa369903e49; make flat-budget PASS (tightest _flat_rf_c headroom 64).
Rig 68 (build resident from RAM:amarchy-flat, flat=on, dopus patched=7), grab spans after 2x (dopus refresh all + Wait 1):
- Before (old build, Rush inactive): right x=799 y 542-563 #1a1b26 (22 px gap), bottom y=573 x 750-781 #1a1b26 (32 px gap).
- After, System active / Rush inactive: System right x=443 and bottom y=573 continuous #7aa2f7 (y480-573, x363-443); Rush right x=799 y480-573 and bottom x719-799 continuous #545a77.
- After, Rush active / System inactive (swapped by clicking the Rush title bar, source set to Rush, refresh): System #545a77 continuous on both edges, Rush #7aa2f7 continuous.
- Arrows and size glyph still drawn: Rush x775-799, y522-573 map shows the up/down arrows (y547-558), the scroller knob, the size glyph (x793-797, y566-571) and the ring column x=799 unbroken down to the corner (799,573).
Size-gadget corner: not measured. Likely the size gadget is not redrawn by `dopus refresh all` (DOpus only re-renders its own scroller gadgets), so its box fill never ran; UNVERIFIED (no instrumentation). With the fix a size redraw would refill both ring strips anyway.
Open: c2_glyph also serves d5_draw (title iconify/lock); the ring refill there only fires when the box touches the ring (it does not). No screenshots were written (in-memory grabs); no scratch over 10 MB.
