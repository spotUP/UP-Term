---
date: 2026-10-16
topic: What the renderer's cursor pass redraws each frame and why it costs 2.7 ms (sync-line)
status: draft
---

# Cursor pass, from source (read-only; no rig run)

## 1. Where, trigger, area
- "Cursor" in the PROF line (handler/vtcon_handler.c:6086, prof_part[2]) is NOT one routine. It is two timed blocks
  in `vtwin_render` (render/vtwin.c:540-558): block A = `vr_mask_begin` + `vr_cursor_off` (:542-544), block B =
  `vr_cursor_on` + `vr_mask_end` (:555-558), each wrapped in PROF_IN/PROF_OUT (:50-51). The flush between them
  (`vt_flush`, :550) is the row painter and is not in the counter.
- Trigger: every rendered frame (`render_pending`, vtwin.c:531). No test for "cursor moved / blinked / cell
  dirty": off and on run unconditionally. Blink adds more calls (`vr_blink_tick`, amiga_render.c:3340-3345).
- Area: ONE cell (a double-size row's half or whole cell; underline/bar = 2 px strip). Old cell in `cursor_draw(r,0)`
  (amiga_render.c:3683-3695), new cell in `cursor_draw(r,1)` (:3698-3763).
- Runs when cursor neither moved nor blinked: YES. `cursor_on_now` (:3807-3830) only skips when drawn already
  (`!r->cursor_drawn`); but `vtwin_render` always calls `vr_cursor_off` first (:3770-3794), which clears
  `cursor_drawn`, so the cell is always erased and redrawn.
  Exception: the row painter sets `cursor_drawn=0` when it paints over the cursor cell (:2047, :3144, :3292),
  but that happens in the flush AFTER the off was already paid.

## 2. What is redrawn, repeats
- OFF, default inverse cursor (`cursor_colorful`=1, set at :3718/3732/3739): `draw_rows(r,x,y,x+1,y+1)` (:3689),
  i.e. the full row-painter entry for one cell: `draw_rows_now` prologue (blank_look, tail/blank bookkeeping,
  :3042-3075), `cell_style`, glyph render, `pad_flush` (:2738-2744). Only a double-size block uses COMPLEMENT
  XOR (:3757-3761), and the non-colorful off path XORs (:3691-3693).
- ON: `vt_row`, `cell_style`, `style_swap`, `vt_map_glyph`, `fill()` (cell rect), `ink_a`, `SetFont`, `Move`, `Text`
  of one glyph (:3698-3763). Glyph is re-rendered through graphics.library, not XORed.
- Repeat of row-painter work: yes. On sync-line the cursor sits on the line just written. Per frame: OFF repaints
  old cell (via draw_rows), flush then paints the new line (its row, possibly the same cell/row), ON draws the new
  cell. The off repaint of a cell the flush is about to repaint anyway is pure waste (unverified: needs the
  flush's damage list; not read, `flush()` engine/vtengine.c:6380 not traced).

## 3. Fixed per-frame costs independent of movement
- `vr_mask_begin`/`vr_mask_end` (:979-1005): SetWriteMask x2 (planar).
- Two `to_front`/`to_back` pairs (:2610-2635, used at :3777 and :3801-3803); outside a composed pass they return
  0 (cheap); inside one they call `planar_present`/`back_present` (full present of the composed damage).
  Unverified whether sync-line (not ?2026) composes.
- `all_planes()` (:966-971) sets `r->mask=0xFF` and `blank=0` in the XOR paths: widens the write mask for the
  frame's later painting (fill/vp_span planes-written logic, ledger FP3.6 row 5). Only XOR paths (double-size,
  non-colorful off) hit it.
- graphics.library calls each take the layer lock and clip-rect loop: RectFill (`fill` :930-960), SetDrMd x2-3,
  SetFont/Move/Text, BltTemplate. About 8-12 library calls per frame, each a few hundred microseconds on 020/060 with
  layers (estimate, unverified).
- The painter's WaitBlit (:1878, :2524) is charged to the later row pass, not the cursor (waitblit line 20 ms).
- Instrument overhead inside the counter: 4 ReadEClock a frame at 41 us each (research fp34-rest-clean.md) =
  about 0.16 ms of the 2.7 ms (6 %).
- Evidence it is fixed per frame, not per movement: cursor ms per frame is flat across shapes: sync-line 543/200
  = 2.7, scroll-nl 187/70 = 2.7, bytewise 235/134 = 1.75, plain-lines 45/17 = 2.6 (fp34-rest-clean.md:44-53).

## 4. Candidate cuts, ranked (algorithmic only)
1. Skip off+on when cursor cell unchanged and nothing dirtied it: remember the drawn cell + its cell content/style
   (or a damage test against the cell), and in `vtwin_render` (vtwin.c:542-557) leave the cursor standing. Needs
   `vt_flush` damage rects (or a "cursor cell touched" flag set in the engine). Effect: whole 2.7 ms on frames
   where the cursor stays (bytewise, wrap, most repeated frames); NOT on sync-line, where the cursor moves each
   line. Cost to verify: damage must include scroll and any paint that covers the cell (:2047/:3144/:3292 already
   handle the painted-over case, so the invariant exists).
2. Fold the OFF into the flush: do not repaint the old cell in `vr_cursor_off`; mark it not-drawn and let the
   flush repaint it only if the engine damaged it, otherwise repaint (or XOR-off) just that cell afterward.
   Also skip the off when the cursor row is about to scroll (`vr_scroll` already calls `vr_cursor_off` at
   :3571, so on sync-line the off happens twice on frames with scrolls: first call clears `cursor_drawn`, the
   second is cheap). Effect on sync-line: removes the single-cell `draw_rows` (about half of the cursor cost,
   estimate 1.0-1.5 ms; unverified split, the off/on halves are not measured separately).
3. XOR the cell instead of re-rendering: `r->cursor_ink==VR_KEEP` inverse is exactly COMPLEMENT of the cell on a
   truecolor-free screen, but the code moved away from it on purpose (:3661-3668, pen NUMBER flips give wrong
   colours on a palette screen, rig 2026-10-03). Not recommended for the default; usable only where the screen
   has colour-stable planes (e.g. mask 1 plane). Small effect, correctness risk.
4. Drop the redundant ON glyph work: for a blank cell under the cursor (`g.code`==0 or space) the code still sets
   font and calls Text (:3752-3759 gate is `g.code`, a space is non-zero). Skip Text for U+0020. Effect: small, but
   the prompt/blank cell is the common case on sync-line (unverified).
5. Measure first: split prof_part[2] into off vs on (two counters) and count library calls per frame before
   choosing 1 vs 2. Both numbers are unmeasured.

## Unverified
- How the flush reports damage to the renderer (engine flush path not read).
- Whether sync-line renders composed (to_front cost).
- Split of the 2.7 ms between off and on.
