# DOpus dialogs flat: inventory and intercept research (s71, agent 39, rig 68, patcher 7241d8d1)

Research only. No source edits. Budget reached (~45 calls): List Format is fully measured; Environment, MakeDir (earlier measurement reused) and a Settings window are UNVERIFIED (see end).

## Q1. Source on disk, and how flat.c finds 0x166d8
- Not on disk. /Users/spot/Code/UP-Term/vtcon/vendor/dopus5-5.104 holds only two lha binaries (3.5 MB). The thoughts files only quote offsets.
- Shallow clone of public GitHub mheyer32/dopus5allamigas (37 MB, deleted after reading). CAVEAT: open-source tree, NOT proven identical to the 5.104 binary. Everything marked [src] is from that tree: source/Library/button_class.c, image_class.c, listview_class.c, gui.c.
- The bevel at 0x166d8 is `listview_border(cl, rp, pens, box, state, single)` [src listview_class.c:1393]: 2 or 4 Move/Draw lines (SHINEPEN / SHADOWPEN, swapped for IDS_SELECTED, THIN or THICK).
- flat.c does NOT scan for it. Per-build table row flat_core.c:139 `{73, 8, {0x780fd46b,0xba2cf172,0xbdaa116a}, 0xde34}`: version 73.8 plus three flat_sum_reloc sums plus offset 0xde34 below the DrawBox body (0x2450c - 0xde34 = 0x166d8). Gated on the 8-byte prologue `48e7 3832 41ef 0020` (flat_body_prologue, flat_core.c:195); trampoline from flat_body_tramp_build (flat_core.c:227); hook installed flat.c:2703-2724. Unknown build: bevel stays stock. See s66-evidence/0c-dopus-gadget-outline.md:126-146.

## Q2. Who calls the bevel, and what discriminates a button
Callers of listview_border [src]; all pass the same kinds of arguments, so the hook cannot tell them apart by arguments:
- button_class.c:937 frame gadget (BUTTONF_FRAME: group boxes, info fields)
- button_class.c:1001, :1053 gauge
- button_class.c:1079 button AND check gadget (same branch; BUTTONF_CHECK only changes fill pen, adds tick image)
- image_class.c:113 scroller arrow images; listview_class.c:1240/1250; palette_class.c:621
- Field wells go through draw_field_box (gui.c:60), the hooked FieldBox, not the bevel.
- s66 binary notes: button default path calls bevel at 0x19370 (dispatcher 0x19950, GM_RENDER 0x18b7c), other in-class caller 0x19012, dopusiclass checkbox image 0x1a9da, palette 0x15d7a. Differs from [src] for checks. UNVERIFIED which is right for 5.104.

Button draw order [src button_class.c:1073-1100, 1225-1250]: (1) listview_border, (2) interior RectFill: pens[FILLPEN] if selected and not check, else pens[BACKGROUNDPEN], inset 1-2 px, (3) label Move/Text, (4) optional image. Pressed = GFLG_SELECTED = recessed==1 in the hook. Disabled is not passed to the bevel.
Consequence: a rounded fill painted in the bevel hook is overpainted by step 2 (except ring/corners outside the inset). The label comes after, so it survives.

Recommended discriminator: caller_return, already received by d4_bevelbody (flat.c:1065), compared to the button-render call site, stored as a per-build offset from the bevel body in the same table row (flat_core.c:139) and gated like the prologue. Same pattern flat.c already uses. s66 gives call site 0x19370 vs bevel 0x166d8 = +0x2c98 (return address = call site + instruction length; UNVERIFIED). Excludes group boxes and info fields (other site), field wells (FieldBox), arrows and palette (other sites).
Known leak: :1079 is shared with check gadgets; checks would also round. Splitting needs gadget flag BUTTONF_CHECK, not visible in the hook.
Fallback: match rect against the window gadget list (flat.c:760-770 button_gadget_of). Fragile: read_ui reported a gadget position not equal to its drawn box.

## Q3. Reuse for a filled rounded DOpus button
Existing path: c8_skip (flat.c:939) -> c8_draw (flat.c:840) -> raa_frame_draw (flat.c:881), on the private stack via flat_on_stack / c8_draw_on (384-byte budget, flat.c:842). Fill/role: flat_frame_fill (flat_core.c:722), flat_button_role (flat_core.c:749); colours flat_glyph.c:524-527 (primary pen_line accent, secondary/plain pen_button 3e4254, pressed pen_text). Radius raa_radius (roundaa_core.c:6). Label: flat_tx_arm / flat_tx_frame (flat.c:889-892) plus Text patch flat_tx_text (flat_tx.s) for dark pressed label. Also d4_flat_pens (flat.c:991), flat_dopus_box_draw (flat_glyph.c:536), flat_dopus_ring (flat_core.c:317, BEVEL returns 1).
Plan:
1. In d4_bevelbody at the button call site: call raa_frame_draw(&st, state, x, y, w, h, fill=1, role, c2_fill, c2_read, c2_rgb, raa_load(rp), rp, NULL) via c8_draw_on. Role: no gadget list in the hook, so secondary (pen_button) for all, or primary via a gadget match.
2. Neutralise the following interior RectFill: reuse the RectFill patch (flat_rf_c, flat.c:2051) with an arm like flat_tx_arm (task + box): next RectFill inside the box is dropped or drawn in face pen.
3. Pressed: arm flat_tx_arm when recessed==1.
4. Use st.side for non-square pixel aspect.
Disabled look: DOpus ghosts the label itself; UNVERIFIED how it reads.

## Q4. Input ground
- Patcher has no input-ground concept. Greps for string, strgclass, StringExtend, pen_input, pen_field, pen_edit in flat.c, flat_core.c/.h, flat_glyph.c: nothing. String and integer gadgets are recessed frameiclass frames drawn flat, unfilled (flat_frame_fill edges-only, flat_core.c:722); interior = stock pen = screen ground 1a1b26. 3e4254 is pen_button (flat_core.h:30) for buttons, cycles, arrows. So "same input ground as other flat inputs" is probably 1a1b26; UNVERIFIED, ask owner.
- DOpus string interior: dopusstrgclass [src button_class.c:843-927]; border via draw_field_box (hooked FieldBox, d4_fieldbox / d4_fieldbody flat.c:1038-1056); margin RectFill with StringExtend Pens[1] (ActivePens[1] when selected); text and cursor by Intuition strgclass with the same pens.
- Recommendation: keep FieldBox hook as is (no ring). Do not fill at FieldBox time: the hook has only rp and rect, and strgclass refreshes repaint with Pens[1]. First measure which pen the interior uses on rig 68; if it already is 1a1b26, nothing to do. Otherwise the only robust place is the gadget StringExtend pens, unreachable from the FieldBox hook. UNVERIFIED on binary.

## Q5. Inventory (rig 68, List Format only)
STATUS before to while open: dopus calls 574 -> 610, bevel 124 -> 156 (+32), face 447 -> 451, frame hooked calls 47 -> 60 (+13), flat 10 -> 13 (+3); after Cancel bevel 158. Colours: ground 1a1b26, accent 7aa2f7, text a9b1d6, face 3e4254. Window 344,178 274x244.

| Element kind | Drawn by | Measured look | Verdict |
|---|---|---|---|
| Window edge | Intuition patch | 1 px accent rect x344-617 y178-421 | FLAT |
| Buttons Use/Save/Cancel (72x17 at y401-417) | dopusbuttongclass, bevel :1079 | 1 px accent outline, ground interior, square | PARTIAL (needs fill + round) |
| Group box x350-611 y194-398 | BUTTONF_FRAME bevel :937 / DrawBox | 1 px accent outline, ground | FLAT (frame, not button) |
| List views x357-460, x469-604, y212-277 | listview class | 1 px accent outline, ground, a9b1d6 text | FLAT |
| Scroller tracks x449-456, x593-600, y215-254 | propgclass | accent-filled (228 px) | FLAT (corners not measured) |
| Scroller arrows 17x12 at y257-268 | dopusiclass, bevel image_class.c:113 | accent outline, ground, glyph, square | PARTIAL |
| Check-like boxes x469-494 y298-344, x579-604 y314-344 | button class check branch | accent outline, ground, tick pixels | PARTIAL (square) |
| Value/cycle boxes x470-598 y280-297 | frameiclass fill | 3e4254 fill, antialiased corners | FLAT |
| Text/labels | Text | a9b1d6 on ground | FLAT |
| String/integer field, palette, slider, radio, lister parts | not in this window | not measured | UNVERIFIED |

Count in List Format: FLAT 6 kinds, PARTIAL 3 kinds (buttons, arrow buttons, check boxes), RAW 0 observed.

## Recommended intercept per non-FLAT kind
- Buttons: bevel hook, call-site discriminator, raa_frame_draw fill, RectFill arm, label arm (Q2, Q3).
- Scroller arrows (image_class.c:113): second call-site row in the same table, or keep as outlined cells (owner call).
- Check boxes: leave, unless owner wants round; shared call site means they round together with buttons unless split.
- Fields: no ring; ground per Q4.

## Risks
1. Call-site offsets come from s66 notes and a non-5.104 source tree. A wrong row rounds group boxes or fields. Verify on rig 68 by logging caller_return for a button and a group box before building.
2. Draw order BORDER, FILL, LABEL: the interior RectFill overwrites a fill made in the hook; needs the RectFill arm. 384-byte private stack: run via c8_draw_on.
3. Shared :1079 site for buttons and checks; d4_flat_pens ReadPixel sees the stale interior; input ground unspecified and strgclass repaints with its own pens.
Also: non-square pixels (st.side), disabled state not passed.

## Unverified
- Environment pages, MakeDir re-measure, Options/Buttons editor, palette, sliders, radio, string fields: not measured. Right-Amiga 4 did not open Environment.
- Which colour the owner means by "other flat inputs".
- Return-address separation on the 5.104 binary.
- Disabled and pressed looks (not driven).
- Lister part draw path.

## Left behind
Rig 68 running, DOpus screen front (via `address 'DOPUS.1' 'dopus front'`), no Environment window open. Stuck RequestChoice process 13 and modal 's71' (132x75 at 0,0) still present, did not block `command Configure`. Scratch: a39-map.py, a39-win.py, a39-st0.txt (tiny). Clone deleted. No screenshots kept.
