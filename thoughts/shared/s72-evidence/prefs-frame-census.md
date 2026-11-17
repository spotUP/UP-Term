# Time Prefs frame census (s72, step 1, read-only)

Rig 68, SYS:Prefs/Time, window at screen 0,14 size 332x203. Screen 800x600, 24 bit grab.
Pens: dri `0 1 2 0 0 3 0 0 2 2 0 2`; flat pens text=2 line=3 back=0 outline=3 button=4 ptext=0.
Pen 3 = 7aa2f7 (accent) is both FILLPEN and pen_line. Any 7aa2f7 outline comes from pen_line.
MEASURED = pixel grab, uitree, gadprobe or STATUS. INFERRED = read from source, not proven on the rig.

## Table (screen coordinates, absolute)

| Element | Box | Gadget | Colours inside / 1 px border | 7aa2f7 outline? | Drawing function | Status |
|---|---|---|---|---|---|---|
| Year field | 54..106 x 32..45 | string id14, 60,35 41x8, "2026"; gadprobe T=104 SI=1 | fill 3f4551 512, text a9b1d6; border 3f4551 + corner blends | NO (ground fill) | c8_draw -> string_ground -> ground_box / raa_ground_frame_draw, flat.c ~856-861, ~1005; fill=1 role PLAIN | pixels MEASURED; counter ground +1 MEASURED; function INFERRED |
| Month button | 142..319 x 32..45 | button id15, 142,32 178x14 "November" | fill 3e4254 1868; border 3e4254 + blends | NO | c8_draw -> raa_frame_draw flat.c ~1017 -> flat_frame_draw flat_glyph.c:516, fill=1 PLAIN (pen_button) | pixels MEASURED; function INFERRED |
| Calendar outer box | 12..319 x 62..136 | custom id11 12,53 311x100, T=5 private class (not frameiclass) | interior 1a1b26; border 7aa2f7 528 | YES | app-drawn grid, not in the hook set (class is private); not proven | pixels MEASURED; function INFERRED |
| Calendar cells (35, 7 cols x 5 rows, 44 x 15 px) | grid lines y=62,76,91,106,121,136; x=12,55-56,99-100,143-144,187-188,231-232,275-276,319 | no gadget per cell | normal cell border 7aa2f7 116, interior 1a1b26 529 | YES, every cell | same as outer box (part of one custom gadget) | MEASURED pixels; function INFERRED |
| Selected day cell | 100..144 x 76..91 | none | interior 7aa2f7 527 (solid accent fill inside the 7aa2f7 outline) | YES (outline) plus accent fill | same | MEASURED |
| Hours slider frame | 126..319 x 154..164 | prop id13 130,156 186x7 plus button id13 126,154 194x11 | border 7aa2f7 406; interior 1a1b26 1676 + knob 7aa2f7 52 | YES | edges-only frame (fill=0) via c8_draw -> raa_frame_draw -> flat_frame_draw outline branch flat_glyph.c ~529-533; knob via RectFill hook ("sfill" in STATUS) | pixels MEASURED; function INFERRED |
| Minutes slider frame | 126..319 x 166..176 | prop id12 plus button id12 126,166 194x11 | same as Hours: border 7aa2f7 406 | YES | same as Hours | pixels MEASURED; function INFERRED |
| Date row | 12..319 x 180..193 | NO gadget (not in uitree, not in gadprobe) | border 7aa2f7 640 (pure 1 px); interior 1a1b26 3258, text a9b1d6 310, dim text 505368 92, 747994 12 | YES | not proven: either an edges-only frameiclass (fill=0) or an app-drawn Move/Draw box | pixels MEASURED; function UNVERIFIED |
| Save | 12..98 x 198..212 | button id2 87x14 | fill 7aa2f7 1017 (accent PRIMARY); border blends | NO (accent FILL) | flat_frame_draw fill=1 role PRIMARY gives pen_line fill | pixels MEASURED; role INFERRED |
| Use | 122..208 x 198..212 | button id8 | same as Save | NO (accent fill) | same | pixels MEASURED |
| Cancel | 233..319 x 198..212 | button id9 | fill 3e4254, rounded | NO | fill=1 role SECONDARY (face) | pixels MEASURED; role INFERRED |
| Window border / title | left x=0, right x=331, top rows 14..26, bottom row 216 | sys gadgets depth, zoom, drag | 7aa2f7 | YES (window chrome, not Prefs content) | Intuition window border, not frameiclass | MEASURED |
| Group box | none found | | | | | UNVERIFIED that none exists |

Distinguishing measurable facts:
- Date row vs string input frame: the date row has NO gadget (uitree/gadprobe) and a pure 1 px 7aa2f7 border; the year field has a string gadget, SI=1 and a 3f4551 ground fill with no 7aa2f7.
- Date row vs group box: no group box exists in this window. The date row is geometrically identical in construction to the slider frames (1 px 7aa2f7, interior 1a1b26), differing only by having no gadget under it.
- Slider frames each have a prop gadget plus a button gadget (126..319, 194x11) behind the 1 px outline; the date row has none.

## Hook attribution (STATUS deltas, Time closed vs open)

frame calls 328 -> 481 (+153); flat (fflat) 78 -> 115 (+37); ground 24 -> 25 (+1, year field); sg +38, ext +38; rectfill hits 58 -> 68; shits 108 -> 112; clone draws +3; dopus counters unchanged (DOpus d4 hooks not involved, MEASURED).
+37 flat frame draws exceeds the ~7 non-calendar frames; calendar cells probably also draw through frameiclass (35 + Hours + Minutes + date = 38 vs 37, UNRECONCILED). Per-element attribution by an isolated action was NOT done (UNVERIFIED).

## Every outline-capable frame path, counted from the dispatch

c8_draw (flat.c:972) order:
1. border_button_at (INSTALLER-BORDER-BUTTON): flat_tx_frame, no ring (flat.c ~996-1001).
2. string_ground >= 0: ground_box -> raa_ground_frame_draw (roundaa_core.c:178), fill=1, no outline (flat.c ~856-861).
3. smart_knob_of: raa_knob_frame_draw (roundaa_core.c:165), fill=1 knob pen, no outline (flat.c ~1011).
4. otherwise raa_frame_draw (roundaa_core.c:150, flat.c ~1017) -> raa_frame_box (:133) -> flat_frame_draw_round (flat_glyph.c:630) -> flat_frame_draw (flat_glyph.c:516).

Fill decision: flat_frame_fill (flat_core.c:763): answered case fill = (!edges_only || push); unanswered OM_GET fill = push.

flat_frame_draw (flat_glyph.c:516-540) is the only place this path draws an outline. Count: exactly ONE outline branch, fill==0 (flat_glyph.c ~529-533): 4 strips in s->pen_line (top, bottom, left and right strips of width flat_button_side). Branch fill==1 never outlines; colour by role (flat_core.h:540: PRIMARY 0, SECONDARY 1, PLAIN 2):
- PLAIN: face (pen_button, or pen_back if 0)
- pressed (ids==1): pen_text
- SECONDARY: face
- PRIMARY: pen_line as accent FILL

Second source: flat_dopus_box_draw (flat_glyph.c ~542, fill=0, PLAIN) through d4_flat_pens (flat.c:1175; d4_drawbox, d4_fieldbox, d4_fieldbody, d4_bevelbody ~1225-1275); flat_dopus_ring(el, recessed) decides ring or none. DOpus5 only, not reached in Time Prefs.

Time Prefs outlines come from the fill==0 branch (sliders, maybe date row) plus app-drawn lines (calendar, maybe date row). A fix that removes the outline in flat_frame_draw fill==0 must also cover app-drawn lines, which hooks cannot reach (UNVERIFIED which hook, if any, sees them).

## Rig end state
Time Prefs closed (Cancel), RAM:gadprobe and RAM:dripens deleted, amarchy-flat.main existed (227660 bytes) and is deleted. No restart. dripens was never run (pens taken from earlier agent record).

## agent 2: draw trace
Method: temporary diagnostic build (patch: scratchpad trace-diag.patch, reversed, nothing committed). A ring of 1536 records in the flatshared block, written by a wrapper around c8_draw (every IM_DRAW / IM_DRAWFRAME in a window narrower than 700 px) and by the Text and RectFill patch handlers, one shared sequence counter, dumped by STATUS lines "dtr seq kind x y w h ...". Coordinates are screen pixels (layer origin added). Window filter 332x203 = Time Preferences. Raw traces: 461 records for the open, 22 for one day click, 40 for one slider drag.
All c8 records in Time and Font answered nothing to OM_GET: IA_Recessed, IA_FrameType and IA_EdgesOnly gave 65535 (unanswered) every time, imp_State 0, method IM_DRAW (1) every time (IM_DRAWFRAME never seen).

Q1 Calendar (MEASURED): YES, through c8_draw, path outline (flat_frame_draw fill==0), IM_DRAW, state 0, attrs unanswered. 30 records, one per DAY CELL, each 44x15: x = 12,56,100,144,188,232,276 (pitch 44), y = 62,77,92,107,122 (pitch 15); row 5 has only x=12 and 56 (days 29 and 30). The other 5 of the 35 grid positions (row 5, x=100..276) have NO c8 record and no pen-3 strip (UNVERIFIED how/whether they are outlined; the census claim of 35 outlined cells is not supported by the draw calls). There is no separate record for the calendar outer box 12..319 x 62..136: its edges are the union of the cell strips (x=12 and x=319 columns, y=62 and y=136 rows, e.g. seq 21..25 left-top cell, seq 190..194 bottom-left cell). The box behind them is one RectFill 12,53-322,152 pen 0 (seq 13, JAM1, before everything else). Cell strips are 4 RectFills each (top, bottom, left, right) of pen 3 issued from inside c8_draw right after the frame record, so no app Move/Draw is involved.
Q2 Date row 12..319 x 180..193 (MEASURED): also c8_draw outline path, IM_DRAW, 308x14 at 12,180, state 0, attrs unanswered, no gadget. Strips: 4 RectFills pen 3 (seq 455..458). It is therefore indistinguishable in c8_draw from the cells and the slider frames except by size and position.
Q3 Sliders (MEASURED): Hours frame 126,154 194x11 and Minutes frame 126,166 194x11, c8_draw outline path (seq 210, 202). The prop container ground is a RectFill 130,156-315,162 pen 0 (seq 216, Minutes seq 208), the knob a RectFill 130,156-137,162 pen 1 (seq 217, Minutes 209 at 145,168-150,174) which the RectFill hook recolours (sfill); both come AFTER the frame record. Label "Hours"/"Minutes" Text comes before the container fill, after the frame.
Q4 Draw order (MEASURED, seq numbers from the open): day cell: frame record, 4 strips, then the day number Text (JAM1, FgPen 2, BgPen 0; seq 21..26). Selected day (11): accent fill RectFill 146,78-185,90 pen 3 comes BEFORE its own frame (seq 81, frame 82), then the number Text with FgPen 0 JAM1 after the frame (seq 87). Weekday names Text JAM1 fg 2 before the cells. Date row: frame and 4 strips first (seq 454..458), then RectFills 14,181-97,192 and 234,181-317,192 pen 0, then the date text Text len 17 at 98,189 JAM2 fg 2 bg 0 (seq 461): content AFTER the frame. Slider: frame, label Text, container fill, knob (all after the frame). So no content is painted over a frame in the order seen; the frame outline strips are never overdrawn by later content in these traces.
Q5 Redraws (MEASURED): one click on day 19 (cell 188,92) redrew: old selected cell interior RectFill pen 0 (seq 462), cell 11 frame + strips + number (463..468), accent fill 190,93-229,105 pen 3 (469), cell 19 frame + strips + number with FgPen 0 (470..475), then the DATE ROW again WITH its frame (476..483, text now at the same x). One slider drag (4 steps) redrew only the knob (RectFill old knob pen 0, new knob) and after EVERY step the date row again, each time frame, 4 strips, two pen 0 fills, text (seq 485, 495, 505, 516), the text length changing 17 to 16 and x 98 to 102. The slider frames and calendar cells were NOT redrawn by the drag. No frame-less redraw of the date row or any cell was seen. A 70 s idle wait produced no records (no clock tick redraw). Observation: the date row is the only element redrawn by every change, always with its frame.
Q6 Distinguishing in c8_draw (MEASURED, with the limit): IA_Recessed, IA_FrameType and IA_EdgesOnly are unanswered for every frame (cells, date row, slider frames, Font group box), imp_State is 0 for all, the method is IM_DRAW for all, and all take path outline (fill 0). They differ only by geometry and by what draws next: slider frame 194x11 (button gadget under it, id 12/13 in the uitree); calendar cell 44x15 (no gadget, pitch 44x15, 4 strips then one digit Text of len 1-2); calendar outer box has no c8 record; date row 308x14 (no gadget, followed by two pen 0 fills and a 17-character JAM2 text); real group box (SYS:Prefs/Font "Select Screen Text" group, window 402x146): IM_DRAW 376x35 at 14,46, state 0, attrs unanswered, preceded by its label Text (JAM2, len 14 at 146,40, seq 886; the label comes BEFORE the frame) and followed by three len-45 JAM2 lines (seq 892..894). Font buttons (376x14, 87x14) are fill 1 (path 5). Raw: seq 887 "1 14 46 376 35 0 4294967295 6 65535 1 ..." versus Time seq 454 "1 12 180 308 14 0 4294967295 6 65535 1 ...". Conclusion for a fix: the frame object gives no flag that tells a group box from a cell or the date row; a rule keyed on the frame must use size/gadget presence, or the app-side order (label Text immediately before the frame) which only the group box shows.
Not done / UNVERIFIED: the 5 unframed grid positions (row 5, x=100..276); which hook sees Time's calendar interior fill beyond RectFill 12,53-322,152; the class pointer of each frame (not recorded).

Rig 68 end state: unmodified main amarchy-flat, 233948 bytes, installed as UP-Term:bin/amarchy-flat and running (built from clean worktree); Time and Font closed; one lister; RAM helper files deleted. Worktree vtcon-prefs-no-outlines: git status --porcelain empty (build/ is git-ignored, 252 KB). Scratch left: 264 KB, no file over 10 MB.

## agent 3: implementation

Commit 5ed00566 on branch prefs-no-outlines (worktree /Users/spot/Code/UP-Term/vtcon-prefs-no-outlines, base 3167ee92). Not pushed.

Functions (worktree paths):
- amarchy/flat_core.c: flat_fc_add:769, flat_fc_class:785, flat_frame_class:798 (array form), flat_noline_pen:808, flat_gr_drop:819, flat_gr_add:829, flat_gr_match:837, flat_gr_clip:850 (band k 0/1/2). Types and defines in amarchy/flat_core.h (flat_gadbox, flat_fc, flat_grbox, flat_gr, FLAT_FC_*, FLAT_GK_*).
- amarchy/flat.c: noline_frame:974 (glue, called from c8_draw:1023 for !fill and imp_State 0, after border_button/string_ground/smart_knob, before raa_frame_draw), rf_gband:2337 and rf_gbox:2346 (called in flat_rf_c before rf_disabled/rf_scroller), Text patch block in flat_tx_c:1773, STATUS counter gboxes on the rectfill line.
- amarchy/flat_shared.h: flat_gr gr and ULONG gbhits added after gh (flat_state unchanged).

Detection: TRACK = borders a prop gadget (flat_string_frame tolerance); CELL = wholly inside and smaller than one OTHER gadget, not bordering it; BORDERED = borders one OTHER gadget (stock draw, unchanged); GROUP = interior contains gadgets (draws nothing); DISPLAY = no gadget relation (input ground). Pens: TRACK/DISPLAY input ground pen, CELL pen_line if centre pixel is pen_line else pen_button else pen_back.

Test: test_no_outlines (tests/test_flat_core.c:3573), run via ./build/vttest_host flat_core.
- Fail before: `tests/test_flat_core.c:3576:5: error: unknown type name 'flat_gr'; did you mean 'flat_gh'?`
- Pass after: `[OK] flat_core: 4324 checks, 0 failed`
- `make flat-budget` passes (first try failed: rf_gbox frame 92 > 80, fixed by splitting rf_gband).
- Cross build: `make amarchy-flat` ok, build/amiga/amarchy-flat 236916 bytes. Worktree build dir 180M (kept).

Found: BORDERED-OTHER unchanged (no measurement). Rule 5 known risk stands (content drawn before an edges-only no-gadget frame is lost). Worktree needed build/amiga/menu_icons.inc copied from main (python fontTools missing, regeneration fails). Rig 68 pixel proof not done.

## agent 5: STATUS sentinel strip

Source: print_status in amarchy/flat.c (the "dopus bevel" line is the only line carrying input-ground words; counted from the print code, 26 words on that line). Consumers searched: tests/, tools/, dist/, docs/, RULES.md, amarchy/ headers. No consumer outside flat.c / flat_shared.h was found for any word on the line (test_flat_core.c tests flat_dopus_status_line, a different line).

| word | counter | added by | consumers | verdict |
|---|---|---|---|---|
| calls= face= nopix= d5= | dbevel dface dnopix d5draws | s66 / DOPUS-BUTTON-NO-RING / DOPUS-LOCK-ICONIFY | none found in tests/rig (feature sentinels) | KEEP (doubtful: no reader found, but wiring proofs) |
| ground= | sground | be0f3280 / 55bd83c8 | none found; states the input ground is applied | KEEP (doubtful) |
| gs= | gsw.installed | 94513af0 / b62cfb37 | states the strgclass hook is installed | KEEP |
| gslot= gcode= | gslot, ggate->code | 9bd03f68 | comment: compare with tests/amiga/gadprobe; no automated reader | KEEP (doubtful) |
| selrgb= pensel= | copy.sel_rgb, copy.pen_sel | 3a43df53 | states the boot theme fields are filled | KEEP |
| sg= | sgseen | d8aaeaa6 | none | STRIP |
| ext= | sgext | d8aaeaa6 | none | STRIP |
| spen= sdep= ssel= | sgpen sgdep sgsel | 92fa7640 | none | STRIP |
| snoext= | sgnoext | 92fa7640 | none | STRIP |
| sscr= swscr= swb= slive= | sscr swscr swb slive | 3f9831d1 | none | STRIP |
| sm0..sm3= | smid[4], smn[4] | 3f9831d1 | none | STRIP |
| snogi= | snogi | 3f9831d1 | none | STRIP |

Stripped: 17 words (sg, ext, spen, sdep, ssel, snoext, sscr, swscr, swb, slive, sm0-sm3, snogi). Kept: 9 words on that line plus the other STATUS lines untouched (rectfill incl. gboxes=, sfill, etc.).
Worst-case line length (all 32-bit values at max, computed not measured): 437 before, 164 after, buffer 512.

## agent 6: rig 68 proof of 5ed00566

Method: binary /private/tmp/claude-501/-Users-spot-Code-UP-Term/0b22a8ab-72ff-4e4b-830c-d27cb207b8dd/scratchpad/amarchy-flat.5ed00566 (236916 bytes) put to UP-Term:bin/amarchy-flat on rig 68 (previous resident, 227660 bytes md5 80462b4b, kept at scratchpad/a6/amarchy-flat.prev), old instance broken, new one started with Run; ami.grab 800x600 24 bit, pixel reads in scratchpad/a6/*.py. Colours measured: window ground pen 0 = 1a1b26; input ground and push button face = 3e4254 (the "3f4551" of the brief is the same pen, #3E4254); text a9b1d6; accent 7aa2f7. No restart of rig 68 (window title keeps the old s71 purpose).
- A PASS: STATUS flat=on, `gboxes=` present; gboxes=0 before Time, 38 after Time opened, 65 after day click + slider drag.
- B PASS (at open): Hours 126,154 194x11 and Minutes 126,166 194x11: 0 pixels of 7aa2f7 on the 4 border lines (402 x 3e4254 + 4 corner pixels 1f2130); interior 3e4254 (1672 / 1686 px); corners 1f2130 differ from interior 3e4254 (1 px rounding); knob 7aa2f7 (52 / 38 px; Hours knob x130..137 y156..162).
- C PASS: 30 day cells (44x15 at x=12+44i, y=62+15j): 0 accent border px on the 29 unselected cells; interior 3e4254 = Cancel face (3e4254, 994 px); every cell has 17..55 text px a9b1d6; selected day 11 (cell 144,77): accent fill 512 interior + ring 94, number drawn in pen 0 (34 px 1a1b26); no 7aa2f7 anywhere else in the calendar box 12,53 311x100. Empty cells (row 4, x=100..276, 5 cells with no day): nothing drawn, plain window ground 1a1b26 (546 interior px each, no frame, no fill).
- D PASS (at open and after drag): Date row 12,180 308x14: border 0 accent px; interior 3e4254 (3266 / 3276 px); a9b1d6 text 301 / 289 px; 1a1b26 only in the 4 rounded corners, no ground rectangle inside.
- E PASS: Year 54,32 53x14 interior 3e4254 512 + text 100; Month 142,32 178x14 1868 + 224 text; Save 87x15 and Use accent fill 1017 / 1036; Cancel face 3e4254 994: same counts as the census, unchanged through click and drag.
- F FAIL (slider only): after clicking day 19 (cell 188,92): old cell 144,77 back to face 3e4254 512 + 34 text px, no accent border, new cell 188,92 accent 505 + number 41 px, 29 face cells, date row passes (3251 ground / 308 text). After dragging Hours knob (133,159 -> 260,159): STALE 1a1b26 knob-erase rectangles stay inside the ground track: 172 px of 1a1b26 in the Hours track at x=130..137, 157..164, 197..204, y=156..162 (8x7 each; the three old knob positions), interior now 3e4254 1504 + 1a1b26 172 + knob 52. Hours knob at new x~246..253 is accent. The erase RectFill of the old knob (pen 0) is not patched to the ground pen after the first container fill. Minutes slider untouched (not dragged). Date row after drag: PASS.
- G PASS: SYS:Prefs/Font group box 14,46 376x35: 0 accent px on the border (3e4254 798 + corners); interior fill 3e4254 10526 (the three gadget/line rows), text a9b1d6 1418 px, label "Select Screen Text" region 254 text px; Font window shows accent only in its Use/Save style buttons and gadget rows 84..154. Window closed again (Cancel).
- H REPORT: ScreenMode window (604x198): frames "Display Mode" 12,46 296x76 and Colors slider 142,157 166x11 have 0 accent border px; the listview 14,48 274x72 shows 7aa2f7 on its border only where the selected row fill (rows y=111..119, x=14..287) touches it (290 px), no other outline; listview text 2982 px and group label text 62 px intact; knobs accent (rows 74..80, 159..165). Nothing vanished that was seen. Directory Opus lister (209,14 443x573): no long accent lines; 7aa2f7 1257 px only as icon art (rows 83..103); ground 1a1b26; icons present. Not tested: Locale.
End state: rig 68 running, 5ed00566 resident (flat=on), Time Preferences open and front, nothing over it. The Hours track still shows the stale ghost squares from the drag (left for the eye). Scratch: scratchpad/a6 (pickles ~1.4 MB each, under 10 MB total? see below).

## agent 7: slider drag erase trace

Method: diagnostic build of 1f9131b9 (worktree removed afterwards, nothing committed): the agent 2 trace patch (ring of 1536 records, c8_draw / Text / RectFill) plus new records: every RectFill carries FgPen, DrawMode, AreaPtSz, the calling task NAME (ln_Name) and a key-by-key match verdict (bit0 FgPen == pen_back, bit1 flat on and not held, bit2 a record covers the rect with the SAME surface and task, bit3 a record covers the rect on the same surface but ANOTHER task, bit4 same task, other surface, bit5 flat_gr_match returned a box); noline_frame drop (kind 5) and add (kind 6) records with the task pointer and ring `next`. Rig 68, Time Prefs, knob 133,159 dragged to 260,159 (a: 3 jumps 160/200/260; b: 16 steps of 8 px). Raw dumps: scratchpad d7-open.txt, d7-drag.txt, d7-drag2.txt, d7-slider.txt.

Trace records of one knob move (drag a, seq numbers; rect in screen pixels; "rec" = bit3 set; "same" = bit2):
- seq 1690 RF 157,156-164,162 pen 254 (new knob, JAM2, AreaPtSz 0) task input.device flags 0b01010 (enabled, rec of another task present, FgPen not pen_back, no match)
- seq 1691 RF 130,156-137,162 pen 0 (= pen_back) JAM2 AreaPtSz 0, task input.device, flags 0b01011: record covering the rect EXISTS (the Hours track, added at open by task "Background CLI"), pen_back yes, enabled yes, same-task NO -> flat_gr_match = 0 -> stock RectFill writes 1a1b26 at 130..137. STALE.
- seq 1735/1736: knob at 197, erase 157,156-164,162 pen 0, input.device, 0b01011 -> no match. STALE (157).
- seq 1780/1781: knob 257, erase 197,156-204,162 pen 0, input.device, 0b01011 -> no match. STALE (197).
- seq 1825..1827 (button up, redrawn by the Prefs task "Background CLI"): RF 253,156-260,162 pen 1 flags 0b100110 and RF 261,156-264,162 pen 0 flags 0b100111: bit2 and bit5 set, match hit, drawn in the ground pen. Clean.
- Between the drag moves the date row is redrawn each time by the Prefs task (c8 path 6 at 12,180 308x14, then drop 1 live record, 4 strips, add at ring next 33..37, two pen 0 fills with flags 0b100111 = match hit, text). These adds advance the ring by one each; the Hours track record was not dropped or overwritten in either drag (bit3 still set at the last erase, seq 2366, after 16 steps with `next` wrapped 0..47). No frame draw of the slider happened during the drag, so no flat_gr_drop hit it (the date row drop rect 12,180-319,193 does not overlap the slider).
- Drag b: 16 erases (pen 0, input.device, flags 0b01011), x = 130,138,...,250 all unmatched; pixel read after: 1a1b26 contiguous in the track rows 157..161 from x=130 to x=257.

Answer 1: candidate (a). The erase is a RectFill through the patched vector (flat_rf_c runs, rf_gbox is reached, AreaPtSz 0, FgPen == pen_back, rect inside the record), but flat_gr_match compares `b->task == task` and the task key differs: the record was added by the Prefs process (task name "Background CLI", pointer 0x10620398), while the prop gadget drag is redrawn by input.device (Intuition's input handler runs the gadget's refresh/erase in its own task context, FindTask(NULL) = input.device). Surface and rect are the same. (b) ruled out (record present at every erase, flags bit3), (c) ruled out (the pixels are written by the RectFill itself; no other primitive was needed).
Answer 2: nothing repaints the other positions; they are not repainted, and the stale count equals the number of knob positions visited: agent 6's script (and drag a) jumps the mouse 133 -> 160 -> 200 -> 260, so the knob stood at 130, 157, 197, 257 and the erases of the first three stayed (the last erase happens at mouse up in the Prefs task, which matches). A real drag with many steps (drag b) leaves every old position stale: one contiguous 1a1b26 band 130..257.
Level of the fix: the ground record key must not include the calling task (the surface plus rect is the identity of the box; the record is per window surface, not per drawing task), fixed in flat_gr_match / flat_gr_drop / flat_gr_add, with the knob erase issued from input.device then matching.
Side note (UNVERIFIED consequence): the FgPen 254 knob RectFill by input.device is not in the ground pen either; whether it is handled by the scroller hook was not measured.

Rig 68 end state: plain 5ed00566 binary (236916 bytes, UP-Term:bin/amarchy-flat, STATUS flat=on, no dtr lines) resident, UP-Term:bin/amarchy-flat.diag deleted, SYS:Prefs/Time open and active ("Time Preferences" 332x203 at 0,14). Scratch: diag worktree removed (git worktree remove --force), scratchpad about 1.2 MB, nothing over 10 MB.

## agent 8: ground record task key fix

- Commit 778e7e3f on prefs-no-outlines.
- Fail-before: tests/test_flat_core.c:3602:46: error: too few arguments to function call, expected 8, have 7 (test changed first, signature still had task).
- Pass: [OK] flat_core: 4326 checks, 0 failed
- flat-budget: PASS (_flat_rf_c headroom 64, _flat_tx_c headroom 68, _c8_disp_stk headroom 328).
- build/amiga/amarchy-flat: 235528 bytes; copy at the session scratchpad as amarchy-flat.a8.
- Unexpected: a blanket text replace first also touched flat_gh_match and flat_tx_pen/flat_gh_text calls; caught by the compiler and restored. No other callers of flat_gr_* exist outside flat.c and the test.

## agent 9: rig 68 proof of 778e7e3f

Start state: rig 68 was DOWN (fs-uae exited cleanly 11 Nov 01:07, idle watchdog; agent a9 #1 died before any install). `rig.py start --max --purpose "s72 session: slider drag and menu proof of 778e7e3f"` cloned a fresh sys.hdf: resident amarchy-flat was the hdf's 227660-byte build and ENV/ENVARC:Amarchy/prefs held only the comment line (no theme, no `flat-system-look`), so Time drew stock (frame flat=0, gboxes=0, 414868 outlines, no 3e4254 grounds). Restored scratchpad/bak/ENV(ARC)_Amarchy_prefs (`amarchy-gui-theme = tokyo-night`, `flat-system-look = on`) and cold-restarted with `rig.py reboot --max --purpose ...` (keep-sys). Side effect of `rig.py start`: its gc removed build/rig3 (idle 463 min, no emulator; not ours). Method otherwise as agents 6/7; scripts in scratchpad/a9 (lib.py, s3.py `<knob y> <start x> <dx>`, m6.py, base.py). Note: after a fresh boot the knobs sit at the current time (Hours knob x184..191, Minutes x230..235), not at x130; a press at 133 is a track page-jump, not a drag, so each drag starts on the knob.
- 1 INSTALL PASS: Break C of the resident, `ami.py put` of amarchy-flat.a8 to UP-Term:bin/amarchy-flat (List: 235528 bytes), Run; after the reboot still 235528, STATUS `flat=on`.
- 2 STATUS smoke PASS (with a note): `dopus bevel calls=0 face=0 nopix=0 d5=0 ground=0 gs=1 gslot=0x1000c458 gcode=0x1002b57c selrgb=3e4254 pensel=252` (112 bytes; after the flat reboot: `face=65 nopix=0 d5=14 ground=20`, gcode=0x1005d9dc). None of sg ext spen sdep ssel snoext sscr swscr swb slive sm0..sm3 snogi as a word anywhere in the 16-line STATUS. `gboxes=` is NOT on the dopus bevel line; it is on the `rectfill` line (gboxes=0 before Time, 38 with Time open after the flat boot, 208 at the end).
- 3 SLIDER Hours PASS: knob dragged from 188,159 in 16 steps of +8 px, button held; knob went x184..191 -> x308..315. Interior x130..315 rows 156..162: 1a1b26 = 4 at open, 4 mid-drag (button down), 4 after release; the 4 are always the knob's own rounded corners (at open (184/191,156/162), mid-drag (308/315,156/162)), so stale window ground = 0. Interior 3e4254 1246, knob 7aa2f7 52 in all three grabs. Mid-drag knob colour: 7aa2f7 (row 159: 8 px 7aa2f7 at x308..315, rest 3e4254 178); no pen 254 colour seen.
- 4 SLIDER Minutes PASS: from 232,171 in 16 steps of -8 px; knob x230..235 -> x130..135. 1a1b26 = 4 (knob corners only) at open, mid-drag, after release; 3e4254 1260, knob 7aa2f7 38; mid-drag row 171: 6 px 7aa2f7, 180 3e4254.
- 5 No collateral PASS: track borders 0 px 7aa2f7 (rows 154/164 and 166/176, x126..319) before and after; calendar box 12,53 311x100 identical before/after (3e4254 17464, 1a1b26 10997, a9b1d6 1553, 7aa2f7 606 = selected day 512 + ring 94 as agent 6); day cell 12,62 interior 3e4254 529 + a9b1d6 17; date row 13,181 306x12 3e4254 3270..3277, a9b1d6 288 -> 306 (clock text changes with the time), date border accent 0.
- 6 MENU PASS: RMB held at 10,5 then 12,6: changed bbox x3..90 y0..27 (1571 px); strip rows 0..13 1a1b26 9904, a9b1d6 804, e5e4d9 411, 564e29 81 (the last two only while the menu is up: title highlight presumably, UNVERIFIED which element); menu part over the window (y14..27): 1a1b26 657, a9b1d6 328, 7aa2f7 247, 0 px 3e4254. Slider tracks, cells, date row identical during the menu. After release: 0 window px (0,14 332x203) differ from before.
- 7 DCTelnet NOT RUN: DCTelnet is not on rig 68 (fresh clone; no #?DCT#? in RAM: or SYS:Utilities; the s69 bisect copied DCTelnet.020 into RAM:DCT from an .lha, i.e. an install, which this brief forbids). Archives exist on the host (~/Downloads/DCTelnet-PETSCII/DCTelnet20.lha, ~/Downloads/DCTelnet-release/DCTelnet-1.9.1-PETSCII-RTG-2026-09-28.lha).
End state: rig 68 up (purpose "s72 session: slider drag and menu proof of 778e7e3f"), a8 binary resident (235528, flat=on), flat tokyo-night desk (prefs restored as above; Sys palette/font/pattern prefs not touched), SYS:Prefs/Time open (process 12) and raised, both buttons released, no DCTelnet. No screen grabs written to disk (all in memory).
