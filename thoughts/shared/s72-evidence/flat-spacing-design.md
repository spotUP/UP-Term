# FLAT-SPACING design (s72, 2026-11-11)

## Owner ruling (2026-11-11)
"It's starting to look really modern and good but we lack modern spacing between the elements."
Decision: inset fills now, gap of 2 pixels between two neighbouring elements (1 pixel inset per side, scaled for the pixel
aspect). Afterwards ONE research agent measures what a layout patch (moving gadgets, growing windows) would cost for
Workbench Prefs only. No layout patch before that research is read by the owner.

## Constraint
Element positions and sizes belong to each program. amarchy-flat only draws. A gap can therefore only come from drawing
each filled element smaller than the box the program gave it.

## Invariant to establish
Two filled flat elements whose boxes touch show window ground between them: every filled flat box is drawn inset by
one visual pixel on each side, and the band between the program's box and the inset box is window ground (pen_back).

## Where (one source, no per-caller arithmetic)
- Checked from the call graph (grep of every caller, 2026-11-11): the one point every rounded flat box passes is
  `raa_frame_box(raa_job *)` (amarchy/roundaa_core.c:133). Its callers: `raa_frame_draw` (roundaa_core.c:150, used by
  `c8_draw` flat.c:1064, `raa_knob_frame_draw` :165, `raa_ground_frame_draw` :178 and through it `ground_box` flat.c:851)
  and the border button draw `bb_draw` (flat.c:2089, which runs `raa_frame_box` + `raa_frame_corners` itself).
  The inset is applied ONCE, to the job box (`j->ox, oy, w, h`) at the start of `raa_frame_box`, so the corner pass
  (`raa_frame_corners`) works on the inset box too. The exemption is a field of `raa_job` set by the caller.
- NOT through this point, to be listed by the implementing agent before coding: fills painted by the RectFill patch itself
  (`flat_rf_c`: `rf_gbox` / `rf_gband`, knob fills, any button highlight fill). They must stay inside the inset box:
  ground box records hold the inset box (below); every other RectFill path that paints a whole element box needs the
  same inset function. `flat_dopus_box_draw` (flat.c:1246) is an outline (fill 0), not a filled element: unchanged.
- New pure function in flat_core (host tested): given the box and the screen's aspect, return the inset box.
  Vertical inset: 1 row. Horizontal inset: `flat_state.side` columns (the existing per-screen aspect source of
  S57-BUTTON-ASPECT-BORDERS / FLAT-ASPECT-SLOTS; side below 1 counts as 1). No second aspect source.
- A box too small to inset (inset height under 5 rows or inset width under 5 columns) is drawn at full size.
- The band (box minus inset box) is painted pen_back BEFORE the inset fill, so no stock bevel or old fill pixel survives
  there. Rounded corner sampling then reads window ground, as today.
- Applies to: push buttons (all states), inputs / ground boxes, day cells, slider tracks, display boxes, cycle and
  similar filled frames, Directory Opus button bank frames that take the filled path.
- Exempt (written exemption, each needs a line in the code comment): window border gadgets and scroller knobs in window
  borders (`raa_knob_frame_draw`: they sit in the window frame, not in the content), menu items, and any fill whose box
  is a whole window or requester ground.
- Ground box records (`flat_gr`, PREFS-NO-OUTLINES) record the INSET box, so content fills are clipped to it and the band
  stays window ground on redraw.

## Risks to measure on the rig (report, do not work around)
- Text or a glyph (cycle arrow, check mark) that sits within 1 pixel of the box edge now lies on the band: list each case.
- Day cells 44x15 become 42x13 (square pixels): day number must stay inside.
- Slider: prop gadget 130,156 186x7 inside track 126,154 194x11 stays inside the inset track (127..318, 155..163).
- Hit areas do not change (only drawing does).

## Tests
- Host: inset box for square and non-square aspect (side 1, 2), too-small box unchanged, band rectangles cover exactly
  box minus inset box, neighbouring boxes 44 wide at x and x+44 leave a gap of 2 * side columns. Fail before, pass after.
- Rig 68: pixel census of Time Prefs (gap between day cells = window ground 1a1b26, 2 columns / 2 rows; text counts
  unchanged from the "agent 6" census), one Directory Opus lister and button bank, one requester.

## Follow-up (after the owner's eye check of the inset)
One sonnet research agent: for Workbench Prefs only, where gadget positions are decided (GadTools CreateGadget calls with
fixed NewGadget boxes, font-relative layout code, window size), what a spacing patch would need to hook, which editors
would break. Output: a cost table per Prefs editor. No code.

## agent 10: fills outside raa_frame_box (FLAT-SPACING survey)

Read-only survey of worktree `vtcon-prefs-no-outlines` (branch `prefs-no-outlines`, 778e7e3f). File:line refer to that tree.
"Inset box" = the box after the planned inset in `raa_frame_box`.

### Table A: callers of the raa frame functions

| function | caller file:line | element | content / exempt | reason |
|---|---|---|---|---|
| `raa_frame_box` | `raa_frame_draw` roundaa_core.c:159 | every raa_frame_draw job | (as the caller below) | the one shared point |
| `raa_frame_box` | `bb_draw` flat.c:2089 (from `flat_rf_c` flat.c:2361 and :2368) | Installer Border-render push button (`bb_claim`/`border_button_at`, flat.c:2045) and DOpus dialog push button (`dopus_fill`, flat.c:2060, box from `dpt` put at flat.c:959) | content | push buttons in the window interior, not window border gadgets despite the "border button" name (Border = Intuition `struct Border` render) |
| `raa_frame_draw` | `c8_draw` flat.c:1064 | frameiclass frame: push buttons, cycle, other filled frames (fill = `flat_frame_fill`) and also outline-only frames (fill 0) | content | GadTools/BOOPSI gadgets in the window. Note: fill 0 jobs also pass here; the design only covers filled boxes, so the inset must key on `j->fill` (UNVERIFIED intent) |
| `raa_knob_frame_draw` | `c8_draw` flat.c:1056 | `smart_knob_of` (flat.c:750): knob of a SMARTKNOBIMAGE prop gadget in the window's gadget list (S56-LOCALE-KNOB, the listview scroller knob in Locale Prefs) | CONTENT, not exempt | this is the ONLY caller and it is a GadTools listview/scroller knob inside the window content, not a window border scroller knob. The design's exemption "raa_knob_frame_draw: they sit in the window frame" does not match the code. A border-scroller knob (GFLG_RELRIGHT prop in the border) would also satisfy smart_knob_of if it uses a SMARTKNOB frame (UNVERIFIED whether any does); the exemption must test the gadget's border flags (GACT_RIGHTBORDER/BOTTOMBORDER), not the function |
| `raa_ground_frame_draw` | `ground_box` flat.c:855 | rounded ground box | content | see its three callers below |
| `ground_box` | `noline_frame` flat.c:1009 | PREFS-NO-OUTLINES track / day cell / display box | content | Prefs content boxes |
| `ground_box` | `c8_draw` flat.c:1049 | INPUT-GROUND: GadTools/strgclass string input frame | content | input field |
| `ground_box` | `d4_flat_pens` flat.c:1234 | DOpus FieldBox string field | content | input field |

Rows: 8 (4 direct callers of the four functions + bb_draw + 3 ground_box callers).

### Table B: fills that do NOT pass through raa_frame_box

| function | file:line | paints | box used | outside inset box after change | must change |
|---|---|---|---|---|---|
| `rf_gbox` / `rf_gband` | flat.c:2322 / :2313, clip `flat_gr_clip` flat_core.c:851 | app's pen_back RectFill inside a recorded ground box, repainted in the record pen with corners kept | ground record box (`flat_gr`), today the PROGRAM box | yes, while the record holds the program box | clip must use the inset box. BUT `flat_gr_match` (flat_core.c:836) requires the fill rect to lie fully INSIDE the record: if the record holds only the inset box, an app erase of the whole program box no longer matches and falls through to a stock pen_back fill that wipes the element. Record needs both boxes (program box for match, inset box for clip), or match on program box and clip to inset |
| `flat_tx_c` JAM2 ground branch | flat.c:1758-1765 | JAM2 text background: BgPen switched to the record pen when the text cursor point (cp_x, cp_y) lies in a record | ground record box, matched by ONE point; the text cell is not clipped at all | yes: a text whose baseline point is inside the box but whose cell (ascent, descent, width) reaches the band paints the record pen into the band (already today it can paint past the box edge) | match against the inset box at least; full fix needs the text cell (TextExtent/ cp_y - tf_Baseline .. + tf_YSize) tested against the inset box, else leave BgPen. Rig-measure which Prefs texts sit within 1 row of the box edge |
| `rf_disabled` / `dim_fill` | flat.c:2158 / :2131 | FLAT-BUTTON-DISABLED-DIM: solid dim fill over a disabled push button (replaces Intuition's ghost dither), corner squares kept by `dim_corner` radius of the full box | ghost record box `flat_gh` (`flat_gh_frame`, flat_core.c:627) = PROGRAM box, recorded at flat.c:1067 (c8_draw) and flat.c:2104 (bb_done) | yes: fills the full program box, band included, with rounded corners computed for the full box | needs the inset function: match on the program box (Intuition's dither covers the gadget box), fill the inset box (and pen_back the band), radius from the inset height |
| `scroller_paint` / `scroller_thin_fill` / `scroller_cuts` | flat.c:2237 / :2227 / :2210, via `rf_scroller` flat.c:2270 | FLAT-SCROLLER solid track fill and knob corner cuts of ANY prop gadget in the window (`scroller_pen` flat.c:1915 walks all prop gadgets: border scrollers and content sliders / listview scrollers alike) | the RectFill rect itself (Intuition's container/knob fill), limited to the prop gadget box (`scroller_hit` flat.c:1887) | no for the Time Prefs slider (container inside the track frame by 4 cols / 2 rows, design line 45); UNKNOWN for other content props (listview scroller container vs its frame) | border scrollers exempt; content props: check geometry on the rig; if a container touches its frame box edge it needs the inset |
| `rf_edge` | flat.c:2280 | window outer ring in edge pen | `flat_outer_edge_fill` rect (window ring) | n/a | exempt (whole-window border) |
| `rf_side` | flat.c:2295 | S57-LOCALE-LIST-NO-SELECT side fill (`flat_side_fill_pen` flat_core.c:1097, uses window border sizes) | clipped rect from window borders | UNVERIFIED (not read: whether this is a listview item highlight inside content or a border band) | settle before coding |
| `ghost_menu` | flat.c:1987 | menu ghost recolour / title ghost | menu item rect | n/a | exempt (menu) |
| `tx_title` band fill | flat.c:1700-1712 | window title band | title bar rect | n/a | exempt (window border) |
| `flat_dopus_box_draw` (DOPUS-BUTTON-NO-RING) | flat.c:1246 -> flat_glyph.c:536 -> `flat_frame_draw` fill 0, flat_glyph.c:529-532 | 1 row / `side` column ring at the PROGRAM box edge, in the interior's face pen (`flat_dopus_face_pen`, flat.c:1243) when `flat_dopus_ring` is 0 | program box | yes: the "outline" is painted in the face pen so the button reads as one flat block reaching the box edge; DOpus paints the interior itself (not ours), so the block stays full size | design line 27 calls it "an outline, unchanged"; that is only true when the ring is a real ring. For the no-ring case the ring must take pen_back (band) and the interior stays DOpus's, i.e. the gap comes from painting the edge ring in ground pen. Owner/design decision |
| `c2_glyph` -> `flat_glyph_draw` | flat.c:603-616, flat_glyph.c:499 | sysiclass/dopusiclass images: arrows LEFT/UP/RIGHT/DOWN, depth, zoom, size, close, iconify, lock, menu check/amiga key/submenu (`flat_glyph_known` flat_glyph.c:189) incl. box fill | image box | border gadgets and menu images: exempt. Arrows also render GadTools content scrollers/listviews (sysiclass arrows in window content): UNVERIFIED whether those reach this path and whether they are "filled elements" for the design | decide whether content scroller arrows get the inset; if yes the image box needs the inset function |
| `clone_on_private` -> `flat_clone_draw_in` | flat.c:453, flat_glyph.c:411 | cloned window-gadget images (`flat_clone_route` flat_glyph.c:250: window gadgets only, not knobs) | image box | n/a | exempt (window border gadgets) |
| `string_ground` / `g9_ground` pen write | flat.c:843, flat.c:1167 | strgclass repaints its own interior from StringExtend Pens[1] = input pen | the string gadget's container box (Intuition's own fill, not ours) | UNKNOWN: GadTools string gadget box sits inside its frame (normally a few px in), so likely no; a gadget whose box equals its frame box would paint into the band | rig check: input gadget box vs frame box per Prefs editor |

Rows: 12.

### Where ground records are added

- One call site only: `flat_gr_add` at flat.c:1010 in `noline_frame` (PREFS-NO-OUTLINES track, day cell, display box), storing the
  PROGRAM box `ox, oy, ox + w - 1, oy + h - 1` and the pen. Implementation flat_core.c:829.
- `ground_box` callers for string inputs (flat.c:1049, :1234) add NO record: their interior is repainted by strgclass from Pens[1].
- Ghost records (`flat_gh_frame`, used by `rf_disabled`): flat.c:1067 (c8_draw, program box) and flat.c:2104 (bb_done, program box
  or the DOpus full box `bb_fb`).
- Text arm records (`flat_tx_frame`): flat.c:1043, :1070, :2105, all program box; they only set pens, they fill nothing.

### Open points

1. `raa_knob_frame_draw` exemption is wrong as written: its only caller (flat.c:1056, `smart_knob_of` flat.c:750) is a content
   listview scroller knob. Decide: inset it like content, or exempt by border flags. Border-scroller knobs proper go through
   `rf_scroller` RectFills (flat.c:2270), not raa.
2. `flat_gr_match` containment (flat_core.c:836-847): record holding only the inset box breaks app erases of the full box. Record
   needs program box + inset box.
3. JAM2 text BgPen (flat.c:1758) is matched on one point and never clipped: text cells reaching the band will paint into it.
4. DOpus no-ring box (flat.c:1240-1246): face-pen edge ring makes a full-size block; the design calls it an outline. Decide.
5. Content arrows (GadTools listview/scroller) through `c2_glyph` (flat.c:603) and the content prop container fills (`rf_scroller`)
   and string gadget interiors (Pens[1]): geometry vs the frame box UNVERIFIED; rig census needed.
6. `rf_side` (flat.c:2295, flat_core.c:1097): not read far enough to say content or border.
7. `raa_frame_draw` in c8_draw also runs fill-0 outline jobs (flat.c:1064): the inset must apply to filled jobs only (design says
   "filled flat box"), so the exemption test should include `j->fill`.

## Design amendments after the agent 10 survey (main thread, 2026-11-11, checked against code at 778e7e3f)
These replace the matching statements in the sections above.
1. `raa_knob_frame_draw` is NOT exempt. Its only caller (flat.c:1054, `smart_knob_of`, S56-LOCALE-KNOB) draws the listview
   scroller knob in the window CONTENT. Knobs of window-border scrollers are painted by `rf_scroller` RectFills and never
   reach `raa_frame_box`. The `raa_job` exemption field therefore has no caller that sets it today for knobs; keep the
   field only if a border or whole-window caller is found during implementation, otherwise do not add it.
2. Ground box records hold BOTH boxes: the program box (match key: `flat_gr_match` at flat_core.c:837 needs the fill to lie
   inside the record, and the program erases its full box) and the inset box (clip for the painted fill). A fill that
   matches is painted only inside the inset box; the part on the band is painted pen_back. One call site adds records
   today (flat.c:1010, `noline_frame`), storing the program box.
3. `rf_disabled` / `dim_fill` (flat.c:2158) paints the full program box from the ghost records (flat.c:1067, :2104): it
   uses the same inset function, corner radius from the inset height.
4. JAM2 text background in `flat_tx_c` (flat.c:1758) matches by the cursor point only and does not clip the text cell.
   Measure on the rig first whether any text cell reaches the band (risk list above); change code only for a measured case.
5. Directory Opus no-ring box (flat.c:1240-1246, `flat_dopus_box_draw` with pen_line = face pen) IS a filled element
   (a full-size block in the face colour), not an outline. It takes the inset like every other filled element (the owner
   ruling is spacing between elements; default chosen by the main thread, owner may veto at the eye check).
6. `raa_frame_draw` also runs outline-only jobs (flat.c:1064): the inset applies only when `j->fill` is set.
7. Open, to settle on the rig or by reading during implementation: GadTools content arrows through `c2_glyph` (flat.c:603);
   prop container fills and string gadget interiors reaching the frame edge; whether `rf_side` (flat.c:2295) is a content
   or a border fill.

## agent 11: FLAT-SPACING implementation

Commit `f998ac0a` on branch `flat-spacing` (worktree `/Users/spot/Code/UP-Term/vtcon-flat-spacing`, base 778e7e3f). Not pushed, not merged.

Changed fill paths:
- `amarchy/flat_core.c`: `flat_inset`, `flat_band_k` (scalar band math), `flat_inset_band`, `flat_gr_band` (after `flat_button_side`); `flat_gr_add` stores the inset box, `flat_gr_clip` clips to it. `flat_core.h`: `flat_grbox` gains `ix0..iy1`, `FLAT_INSET_MIN 5`.
- `amarchy/roundaa_core.c`: `raa_inset` called first in `raa_frame_box` (filled, enabled, not held jobs); covers `raa_frame_draw` (c8_draw), `raa_knob_frame_draw`, `raa_ground_frame_draw` (ground_box, noline_frame, c8 string, d4_flat_pens), bb_draw. Knob and ground draw take a `side` argument.
- `amarchy/flat.c`: `rp_side`; `ground_box`, `noline_frame`, knob caller pass side; DOpus no-ring ring pen = `back` when `flat_inset` insets; `dim_band` + `rf_disabled` dim the inset box; `rf_gband` radius from inset height; `rf_gring` paints the band part of a recorded ground erase.

Open-point verdicts: c2_glyph scroller arrows, rf_scroller prop container, string interiors (inside GadTools bevel): no change, rig to measure. rf_side border strip: exempt (window border, not an element). No raa_job exemption field added.

Fail before: base tree + new test: `tests/test_flat_core.c:1058:108: error: too many arguments to function call, expected 13, have 14` (also flat_gr_add 7 vs 8, no member ix0, flat_inset undeclared).
Pass after: `[OK] flat_core: 4363 checks, 0 failed`. `make amarchy-flat` clean. `make flat-budget` PASS: flat_rf_c headroom 64 (at the minimum; band math moved to scalar `flat_band_k` to get there), flat_tx_c 68, c2_disp_reg 64.
Binary: scratch `amarchy-flat.fs`, 237076 bytes, md5 f29f948e862696609d92d5b583d0f166.

Rig proof must measure: Time Prefs day cells gap 2 columns / 2 rows of ground (1a1b26) on square pixels, 4 columns on a non-square side-2 screen; button label text counts unchanged; slider knob; Locale listview knob and container; DOpus lister and button bank (no-ring ring = ground); requester buttons and a disabled button (dim inside inset, band ground); JAM2 text cells next to a band.

UNVERIFIED: DOpus `back` pen equals window ground; flat_tx_c JAM2 text backgrounds reaching into the band; any element whose fill is drawn outside raa_frame_box / ground records.

Build note: worktree had no `build/amiga/menu_icons.inc` and no python with fontTools; copied the main checkout's generated file into the worktree build dir (read only from main).

## agent 13: rig 68 proof of FLAT-SPACING f998ac0a

Binary: scratchpad/fs/amarchy-flat.fs, 237076 bytes, md5 f29f948e862696609d92d5b583d0f166, installed to UP-Term:bin/amarchy-flat (List 237076). Screen 800x600x24 "rtg", square pixels, so side=1 and the expected gap is 2 columns / 2 rows of 1a1b26. Rig 68 was down at start; started with `UPTERM_RIG=68 python3 tools/rig/rig.py start --max --purpose "s72: FLAT-SPACING proof"`, rebooted twice with the same purpose.

Environment finding (affects every fresh-clone rig proof): after the first reboot flat drew nothing (Time Prefs stock 414868 grid, frame flat=0, gboxes=0, dopus flat=0). The main build 235528 gave identical stock pixels in the same boot, so the binary was not the cause. `flat_frame_decide` (amarchy/flat_core.c) draws flat only when DrawInfo SHINEPEN == BACKGROUNDPEN. The fresh clone had no ENVARC:Sys/palette.prefs; Amarchy writes ENV:Sys/palette.prefs at boot after the Workbench screen has opened, so the screen kept stock pens. Putting the saved ENVARC:Sys/palette.prefs (434 bytes, identical to the ENV copy) and rebooting fixed it. ENV/ENVARC:Amarchy/prefs were also restored (`amarchy-gui-theme = tokyo-night`, `flat-system-look = on`).

| # | Check | Verdict | Numbers |
|---|-------|---------|---------|
| 1 | Install | PASS | List 237076; STATUS flat=on, dopus flat=38, rectfill gboxes=38 side=1 looks=1 |
| 2 | Calendar gap | PASS | rows 69/84: 1a1b26*2 at x55-56, 99-100, 143-144, 187-188; cols 30/77: 1a1b26*2 at y76-77, 91-92, 106-107, 121-122; cell fill x13..54 for box 12..55 |
| 3 | Day text / selected day | PASS | calendar a9b1d6 1553 (same as agent 9 pre-inset); text pixels in bands 0; selected cell inset 7aa2f7 492 + digits 1a1b26 38, band 1a1b26 114, accent in any band 0 |
| 4 | Sliders + drag | PASS | Hours band 1a1b26 406 only, inset 3f4551 1668, knob 7aa2f7 52; Minutes band 406, inset 1682, knob 38; Hours knob 199..206 to 130..137, Minutes 310..315 to 181..186; 0 stale pixels mid-drag and after release |
| 5 | Date row / year / month | PASS | date band 1a1b26 640 only, inset 3f4551 3265; year band 130 only, inset 492; month band 380; JAM2 text never reaches a band |
| 6 | Button row | PASS | Save/Use/Cancel bands 1a1b26 198 each; Save inset 7aa2f7 916, Use 935, Cancel 3e4254 893; label margins L/R 28/27, 32/31, 20/20, T/B 2/3; 12 ground columns to each window edge; Save to Use 25 columns |
| 7 | Locale listview + scroller | PASS | list ground 3e4254 x14..148, gap x149-150, prop container 3f4551 x151..166, knob 7aa2f7 x154..163 inside container; gaps rows 248-249, 257-258; arrow fills 250..256, 259..265; glyphs inside inset |
| 8a | DOpus lister | PASS | lister buttons 3e4254 x205..332 then 1a1b26*2 at x333-334 (rows 56, 70); bottom field 3f4551 x217..629 inside 1a1b26 ground; lister ground 1a1b26 204272 |
| 8b | DOpus button bank (W6 712,55 84x332) | REPORT (not themed) | stock DOpus colours 000000 5892, cf2f00, 222222, 8d8d8d, efaf4f, 7f007f, ffff00; rows filled edge to edge x713..794 with 000000*2 separators; no 1a1b26 band. Not a spacing regression: the bank is outside flat's DOpus hooks on this build (same look as before) |
| 9 | Disabled button (Locale "Clear Languages") | PASS | band 1a1b26 368 only; dim inset 222c40 1827 + ghost label 414868 189; fill x369..538 |
| 9b | RequestChoice requester | n/a | buttons are 1 px 7aa2f7 outline, interior 1a1b26; no filled element, so no inset applies |
| 10 | End state | done | requester and Locale closed, Time Preferences active in front, DOpus lister behind; STATUS flat=on, gboxes=397, frame flat=34; List 237076; `rig.py hold` (4 h) |

Commands: grabs via `ami.grab` (pickled, deleted), element censuses with scratch scripts g.py / el.py, clicks via `ami.click_px`, status via `python3 ami.py exec 'UP-Term:bin/amarchy-flat STATUS'`.

UNVERIFIED: DOpus button-bank verdict whether the bank should be themed at all (compare with the main build on a rig with the palette fix); non-square-pixel modes (side=2) were not measured on this rig.
