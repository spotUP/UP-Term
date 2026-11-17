# FLAT-WIDE-INPUT: fit a read-only ground box to its text (design, agent 16, s72)

Read against local main `f03934cf`. Source reading only: no build, no test, no rig. Every statement not read in source or in
an earlier measured section is marked UNVERIFIED.

## 0. The finding that decides the scope (read first)

The only Time Prefs element the owner's description can mean is the date row (12,180 308x14, DISPLAY class, no gadget). Its
text is NOT left-aligned. It is centred, measured by agent 2 (`prefs-frame-census.md` "## agent 2: draw trace", Q4/Q5):
- open: app fills 14,181-97,192 and 234,181-317,192 in pen 0, then Text len 17 at cp 98,189 (JAM2, fg 2, bg 0);
- after a slider step: len 16 at cp 102. Centring predicts exactly this: 12 + (308 - 17*8)/2 = 98, 12 + (308 - 16*8)/2 = 102.

Left gap and right gap of the text inside the inset box (13..318) are equal (85/85, then 89/89). The other wide-looking boxes
are not candidates: year field 54..106 is a string gadget (41 px gadget, "2026" = 32 px, not wide); month 142..319 is a
cycle/button with a centred label (census table). So a rule that fits ONLY left-aligned text changes nothing in Time Prefs.
The owner must choose how a centred display text is fitted (question Q1 below). The mechanism is the same for every option;
only the pure decision function `flat_fit_box` differs.

## 1. Which elements qualify

- DISPLAY records only: frames that `flat_fc_class` returns as `FLAT_FC_DISPLAY` (flat_core.c:785-795: no string, prop or
  other gadget relation, holds no gadget). Today these are drawn by `noline_frame` (flat.c:975-1020) with `ground_box` and
  recorded with `flat_gr_add` (flat.c:1017).
- NOT TRACK and NOT CELL (also recorded at flat.c:1017): the record gets a `fit` flag, set only for DISPLAY.
- NOT string gadgets (`string_ground`, flat.c:843-855, drawn at flat.c:1055-1059): strgclass repaints its whole interior from
  `StringExtend Pens[1]` on every refresh and keystroke (comment flat.c:840-842), cursor and scrolling included; a fitted ground
  would be overpainted by the gadget itself, and the text extent changes while typing. Integer gadgets are string gadgets on
  this path, so they are excluded by construction, not by a size test. The string path never adds a ground record.
- GadTools TEXT_KIND / NUMBER_KIND display boxes: if their bevel frame is the gadget's own render it borders the gadget, so the
  class is BORDERED (flat_core.c:793-794) and the stock draw runs; they are out of reach of this change. UNVERIFIED (no trace of
  a TEXT_KIND frame exists).

Pure decision, new in flat_core (host tested), on scalars and the record, no arrays:
`int flat_fit_box(const flat_grbox *b, int tx0, int tw, int pad, int out[2])` -> 1 and the fitted columns out[0]..out[1], or 0
(leave the record as it is).
- in = inset of the program box (`flat_inset(b->side, ...)`, flat_core.c:1364-1373), u0..u1 = union of this text
  (tx0 .. tx0+tw-1, tx0 = rp->cp_x, the Text pen position is the left end of the string) with the texts already seen in this
  record since its frame draw (b->tx0..b->tx1).
- lgap = u0 - in[0], rgap = in[2] - u1. tol = 2 * side (odd centring remainder, kerning).
- LEFT-aligned: lgap <= pad and rgap > lgap + 2 * pad. Fit: out = (b->x0, min(in[2], u1 + pad)). Left edge = program x0.
- CENTRED: |lgap - rgap| <= tol and rgap > 2 * pad. Fit depends on Q1: (A) out = (u0 - pad, u1 + pad); (B) out = (b->x0,
  u1 + pad); (C) return 0.
- RIGHT-aligned or anything else (rgap <= lgap + 2*pad and not centred): return 0. A text whose cp lies outside in[] or a
  record with `fit == 0`: return 0. A fitted width that is not at least 2*pad narrower than in[] : return 0 (nothing to gain).

## 2. When is the text known: options and choice

The order is fixed and measured: frame (flat.c:1069 -> noline_frame: full rounded ground) -> app pen-0 RectFills (rf_gbox) ->
Text. Every measured redraw of the date row repeats the frame first (day click, every slider step; no frame-less redraw; no idle
redraw).

| Option | Cost | Fails to cover |
|---|---|---|
| (i) full box at frame time, shrink at Text time (paint the surplus in window ground, redo the corners at the new edge) | one rounded draw per Text into a fit record; must run off the Text caller's stack (flat_tx_c headroom 68) | a visible full-width flash on every redraw: the date row is redrawn on every slider step |
| (ii) defer the box draw to the first Text | frame draws window ground only, record "pending" | an empty box (no Text) never shows; content drawn between frame and Text lands on window ground; the pen-0 fills of the app must be special-cased while pending |
| (iii) CHOSEN: (i) plus carry the last fit across redraws | as (i), plus one ring scan in noline_frame (c8 private stack, not budget-bound) | the first draw after open flashes full width once; a box whose text changes without a new frame draw only grows until its next frame draw |

Chosen: (iii).
1. Frame time (`noline_frame`, already on the c8 private stack, flat.c:1084-1128): before `flat_gr_drop` (flat.c:983), a pure
   `flat_gr_prev_fit(g, surf, x0,y0,x1,y1, out[2])` returns the fitted columns of a live record with the SAME surface and SAME
   program box. If found, the frame draws the fitted box directly (no flash) and `flat_gr_add` stores it as the painted extent.
   Else the frame draws the full box as today.
2. Text time (`flat_tx_c`, flat.c:1762): when `r->gfit_live > 0` (count of live fit records; 0 = no extra work on any Text), match
   the record by the pen position (as flat.c:1772 does now, for JAM1 and JAM2 both), and if it has `fit` set run the fit job
   BEFORE the stock Text call, so the text is drawn on the final ground. The JAM2 BgPen patch (flat.c:1771-1778) stays and keeps
   its condition.
3. The fit job runs on the private clone stack exactly like `c8_draw_on` (flat.c:1099-1128): already on `cl_stk` -> call
   directly; else `AttemptSemaphore(&cl_ss)` (never wait; busy -> skip the fit for this Text, count `gfitbusy`, the box stays as
   drawn) and `flat_on_stack(fit_run, &fit_arg, &cl_sss)` with `fit_arg` a static under `cl_ss` (as `cl_arg`, flat.c:197-198).
   On the private stack: `TextLength(rp, str, count)` (not patched by flat: no TextLength in flat.c), `flat_fit_box`, the strip
   repaint (section 3), then `flat_gr_fit` updates the record.

Cases:
- Text later SHORTER without a new frame: the union keeps the wider extent (no shrink, so an earlier text in the same box is
  never cut). The app's own erase of the old text is a pen-0 fill: rf_gbox paints it ground inside the fitted extent and window
  ground outside (unchanged code, section 4). Shrinks at the next frame draw. In Time every change redraws the frame (measured).
- Text later LONGER: the union grows; the strip repaint extends the ground before the text is drawn.
- Empty box, no Text: stays the full inset box, exactly as today (default chosen: nothing to fit; owner may veto, Q4).
- String gadgets while typing: not on this path (section 1).
- Day click / slider step: frame (pre-fitted from the previous fit) -> fills -> Text -> fit job: 17 vs 16 characters moves each
  edge by 4 columns; only those strips repaint.
- Several Texts in one box (label + value): union; the strip clip (section 3) never touches columns already holding text.

## 3. Geometry

- Program box x0..x1, y0..y1 (the match key, unchanged). Normal inset (flat_core.c:1364-1373): x0+side .. x1-side, y0+1 .. y1-1.
- Fitted painted box: rows y0+1 .. y1-1 (top/bottom inset KEPT), columns out[0] .. out[1] from `flat_fit_box`:
  - left-aligned text: left = x0 exactly (NO left inset, band k=2 of flat_band_k becomes empty: flat_core.c:1383-1385 gives
    x1 = ix0-1 < x0 -> 0), right = last text column + pad, never past x1 - side (the normal right inset);
  - centred text: per Q1.
- pad = FLAT_FIT_PAD * side columns, FLAT_FIT_PAD = 6 (square pixels: 6 columns; side 2: 12 columns). Reason: the year string
  field next to the date row starts its text 6 columns after its program box (frame 54, gadget 60; census table), so the visual
  lead and tail match. Must be >= the corner radius: inset height 12 rows -> `raa_radius(12)` = 4 (roundaa_core.c:6-9,
  FLAT_ROUND_TALL 12 at flat_glyph.h:99), so no text column ever lies inside a corner square. Value is an owner decision (Q3).
- Rounded corners at the new edges: the existing `raa_frame_box` (roundaa_core.c:152-168: beside-corner reads, then
  `flat_frame_draw_round`) plus `raa_frame_corners` (roundaa_core.c:113-134). Refactor, no new routine: split `raa_inset`
  (roundaa_core.c:136-150) out of `raa_frame_box` into the callers, so a job can draw an already-fitted box with no inset, and
  add `raa_ground_fit_draw(s, pen, x0,y0,x1,y1, fx0, fx1, cx0, cx1, ...)`: paints the program-box rows y0+1..y1-1 outside
  fx0..fx1 in pen_back, then the rounded box fx0..fx1 x y0+1..y1-1 in pen, both clipped to columns cx0..cx1.
- Column clip: through the job's fill callback (`j->fn`, a `flat_fill_fn`): a clip wrapper `fit_fill(ctx, ...)` with a static
  ctx {rp, cx0, cx1} under cl_ss that clips x and calls `c2_fill`. Search first for an existing clipping fill callback in flat.c
  before adding it (UNVERIFIED that none exists; not searched in this pass).
- Strips repainted at Text time: for each edge that moves, columns [min(old,new) - r + 1 .. max(old,new) + r - 1] clipped to the
  program box (r = corner radius). Text pixels are never inside a strip because pad >= r and the job runs before the Text.
  At frame time with a carried fit, the clip is the whole program box (no content drawn yet).

## 4. Records

`flat_grbox` (flat_core.h:537-541) gains, all ints, no pointers:
- `fit` (1 = DISPLAY, fitting allowed; set by noline_frame through `flat_gr_add`, whose signature gains `fit`),
- `side` (the inset side used at add, so `flat_fit_box` can recompute the normal inset),
- `tx0, tx1` (text union since the add; tx1 < tx0 = no text yet).
The painted extent is the EXISTING `ix0 / ix1`: `flat_gr_add` (flat_core.c:829-838) writes the normal inset (or the carried fit);
the new `flat_gr_fit(g, b, fx0, fx1, tx0, tx1)` overwrites `ix0 / ix1` and the union after the strip repaint. Written only in
`flat_gr_add` (single site flat.c:1017) and `flat_gr_fit` (single site: the fit job). Ring memory: 48 x 16 bytes more in the
shared block, nothing on any stack.

Consumers need no code change, because they all read `ix0 / ix1` already:
- `flat_gr_match` (flat_core.c:840-851) keys on the program box: the app's erases of its full box still match.
- `rf_gbox` (flat.c:2362-2380): `rf_gring` / `flat_gr_band` (flat.c:2351-2357, flat_core.c:1402-1404) paint everything between
  program box and ix box in pen_back, so a pen-0 fill outside the fitted box paints WINDOW GROUND (the date row's right fill
  234..317 becomes ground only up to out[1], window ground after it); `rf_gband` / `flat_gr_clip` (flat_core.c:853-875) paint the
  ix part in the record's pen with its corner squares kept; radius from `iy1 - iy0 + 1`, unchanged by the fit.
- JAM2 BgPen patch (flat.c:1771-1778): matches by cp point on the program box; after the fit the whole text cell lies inside
  out[0]..out[1] by construction, so the ground-pen text background never lands on window ground. A JAM2 Text whose cp lies
  outside the fitted columns but inside the program box (only possible if the fit was skipped as busy): it gets the ground BgPen
  on window ground. Accepted as rare; counted by `gfitbusy`.
- `flat_rf_c` chain: NO code change (budget headroom 64 untouched). `flat_tx_c` chain: one counter test and one call to a small
  helper `tx_fit` that holds no arrays (its arguments go into the static `fit_arg`); its chain is `tx_fit` + `flat_on_stack`,
  shallower than the existing icl_label branch (UNVERIFIED until `make flat-budget`; headroom must stay >= 64, today 68).
  `flat_on_stack` is already reached from the `_c8_disp_stk` entry and passes the budget tool there (agent 8: c8_disp_stk
  headroom 328); UNVERIFIED how the tool treats it on the flat_tx_c chain.
- `flat_gr_drop` (flat_core.c:819-827): unchanged; drops fitted records like any other. `gfit_live` is recomputed by add/drop
  (or counted lazily: number of records with `fit` and a live surf).

## 5. Tests

Host tests in `tests/test_flat_core.c`, one new function `test_flat_wide_input` next to `test_flat_spacing` (:3643), called from
main (:3800). Fail before: compile error (`flat_fit_box`, `flat_gr_prev_fit`, `flat_gr_fit` undeclared, `flat_gr_add` arity,
no member `fit`), the same fail-before form agents 8 and 11 used. Cases (expected numbers for square pixels, side 1, pad 6,
date row record 12,180..319,193):
1. `date_row_box_ends_after_text`: text cp 98 width 136 -> Q1-A: 92..239; Q1-B: 12..239; Q1-C: 0 (no fit).
2. `date_row_after_slider_step`: cp 102 width 128 -> A: 96..235; B: 12..235.
3. `left_aligned_display_box_starts_at_program_edge`: cp 16 width 40 -> 12..61.
4. `left_aligned_box_side2_pads_twelve_columns`: side 2, cp 18 width 40 -> 12..69 (57 + 12).
5. `right_aligned_display_text_unchanged`: cp 278 width 40 (rgap 0) -> 0.
6. `nearly_full_text_unchanged`: cp 16 width 290 -> 0.
7. `slider_track_and_day_cell_never_shrink`: records added with fit 0 -> 0 for any text.
8. `second_text_grows_box_never_cuts_first`: texts 16+40 then 70+24 -> 12..99; then a shorter third text -> still 12..99.
9. `erase_right_of_text_paints_window_ground`: after fit 12..61, `flat_gr_band(m, 3, o)` for clip 234..317 x 181..192 -> 62..317
   band (pen_back), `flat_gr_clip` middle band of 14..97 -> 14..61.
10. `redraw_keeps_previous_fit`: `flat_gr_prev_fit` for the identical box -> the fitted columns; for 12,180..318,193 -> 0;
    other surface -> 0.
11. `empty_display_box_keeps_full_ground`: record without text -> ix0..ix1 = 13..318.
12. `day_cell_text_does_not_fit` and `string_ground_never_records` are covered by 7 and by the absence of a string-path add.
Reachability (top-level entry): host tests cannot drive flat_tx_c. Sentinel: STATUS counter `gfit=` (fit jobs run) and
`gfitbusy=`, printed on the `rectfill` line next to `gboxes=`; the rig proof reads it.

Rig 68 proof (separate agent, purpose text "s72: FLAT-WIDE-INPUT proof"), Time Prefs, square pixels (side 1), radius 4, pad 6.
Numbers for Q1-A; for Q1-B replace the left edge 92/96 by 12. Font assumed topaz 8 (8 px per character, UNVERIFIED on the rig).
- Open: in the program box 12..319 x 180..193: 0 px 7aa2f7. Column 91 and column 240, rows 181..192: 1a1b26 x12 each. Columns
  96..235 (4 in from each fitted edge), rows 181..192: no 1a1b26. Columns 12..88 and 243..319, rows 180..193: only 1a1b26. Text
  a9b1d6 only inside 98..233. Ground 3f4551 + text + corner blends = 148 x 12 = 1776 px in 92..239 x 181..192.
- After one slider step (16 characters): edges 96 and 235; column 95 and 236 rows 181..192 = 1a1b26; no stale 3f4551 in
  92..95 or 236..239 (must be 1a1b26). No full-width ground visible mid-drag (grab during the drag: columns 12..88 all 1a1b26).
- After a day click: same numbers as open. STATUS `gfit` +1 per date-row redraw, `gfitbusy` 0.
- Unchanged: year field, month button, calendar, sliders, buttons: agent 13's counts.
- Non-square (side 2) mode: pad 12 columns, left/right edges at text -/+ 12; not measurable on rig 68 today (800x600 square).

## 6. Risks

- R1 (decides scope): the date row text is centred (section 0). A strict "left-aligned only" rule is a no-op in Time Prefs.
- R2: first draw after open flashes full width for one frame -> fit; afterwards carried fits remove the flash. UNVERIFIED
  whether visible.
- R3: content drawn by an app between the frame and its first Text inside a moving strip is overpainted (only DISPLAY boxes,
  only edge strips). Not seen in any trace.
- R4: record fields are read by rf_gbox in other tasks without a lock while the fit job writes ix0/ix1 (same class as today's
  unlocked flat_gr_add). Worst case one fill uses the old extent; the next redraw repairs it.
- R5: left edge at x0 puts the date row 1 column (side columns) left of every other element's inset edge (calendar, buttons start
  at 13). The owner gave up the shared right edge; the left edge misalignment was not stated (Q2).
- R6: `make flat-budget` must still pass (flat_tx_c >= 64). If the tx_fit branch costs more than 4 bytes over today's deepest
  chain, move the cp test into the helper.

## 7. Owner questions (recommended first)

Q1. The date row text is centred, not left-aligned. How should it fit?
  - (A, Recommended) Hug the text: ground box from text start - pad to text end + pad, staying centred where the text is.
    Trade-off: the box keeps a left inset on centred text (follows the text, not the "no left inset" wording).
  - (B) Literal ruling: left edge at the program box (12), right at text end + pad. Trade-off: 80 columns of empty ground before
    the text.
  - (C) Fit left-aligned text only; the date row stays full width. Trade-off: no visible change in Time Prefs.
Q2. Left edge of a fitted left-aligned box:
  - (A, Recommended) Program box x0 (no inset, as ruled). Trade-off: 1 column (side columns) left of the calendar and buttons.
  - (B) Normal inset x0 + side. Trade-off: keeps every left edge aligned, but is an inset on the left.
Q3. Padding after the text:
  - (A, Recommended) 6 columns x side (matches the year field's text lead). (B) 4 x side (tighter, equals the corner radius).
    (C) 8 x side (one character).
Q4. Typed inputs (year field, any string/integer gadget): shrink too?
  - (A, Recommended) No: strgclass repaints its full interior on each keystroke and the text length changes while typing.
  - (B) Yes, fitted to MaxChars: needs a strgclass-side change and a new design; not in this step.
Q5. An empty display box (no text drawn):
  - (A, Recommended) Keep the full ground box as today. (B) Draw no ground at all until text arrives (option ii above).

## Owner decisions (2026-11-11, through the question tool, after the agent 16 report)
- Q1 scope: the date row only (DISPLAY class). Typed inputs (year string gadget) keep their full width.
- Q1 fit for centred text: hug the text on both sides. The box runs from pad columns left of the text to pad columns
  right of it and stays centred (expected 92..239 at side 1 for the 17-character date, 96..235 for 16 characters).
  The shared left edge with the calendar and sliders is given up knowingly.
- Q2: for left-aligned DISPLAY text the left edge is the program x0, no left inset (owner, earlier the same day).
- Q4: typed inputs do not shrink (follows from Q1 scope).
- Main-thread defaults, not put to the owner: Q3 pad = 6 * side columns; Q5 an empty display box keeps the full ground.
- Still UNVERIFIED before implementation: flat-budget headroom on flat_tx_c with flat_on_stack; GadTools TEXT_KIND reach;
  side 2 modes; whether flat.c already has a clipping fill callback to reuse (search before building one).
