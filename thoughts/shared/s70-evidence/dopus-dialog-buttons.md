# DOpus dialog buttons: why outlined (s71, agent 38, rig 68, patcher build 7241d8d1)

## Cause (one sentence)
DOpus 5 Environment and requester buttons are drawn by dopus5.library's internal static bevel (5.104 0x166d8, used by dopusbuttongclass / dopusiclass frame images, not by DrawBox), which the patcher handles in `d4_bevelbody` (amarchy/flat.c:1065) -> `d4_flat_pens(FLAT_EL_BEVEL)` (flat.c:991) -> `flat_dopus_box_draw` (flat_glyph.c:536) = `flat_frame_draw(fill=0, role=PLAIN)`: a square 1 px pen_line outline, interior left to DOpus. The filled rounded buttons come from the frameiclass hook `c8_skip` -> `c8_draw` (flat.c:939 / 840) -> `raa_frame_draw(... fill, role ...)` (flat.c:881), reached only for the Intuition frameiclass DRAW/DRAWFRAME methods (GadTools / Workbench / system requesters / Installer); DOpus never calls frameiclass for these buttons.

## Draw paths (entry hook, function, file:line)
Outlined (DOpus):
- hook install: flat.c:2703-2724 (`d4_bevelbody` for FLAT_EL_BEVEL, patched in dopus5 body found by prologue `flat_body_prologue`, flat_core.c:195; `d4_drawbox`/`d4_fieldbody` for DrawBox/FieldBox).
- `d4_bevelbody(caller_ret, a, rp, pens, rect(x,y,w,h), recessed, thin)` flat.c:1065 -> `d4_flat_pens` flat.c:991. Decision `flat_dopus_decide` (flat_core.c:296): SKIP = draw flat.
- `flat_dopus_ring(el, recessed)` (flat_core.c:317): FIELDBOX 0; raised DrawBox 0; BEVEL returns 1 (ring kept) -> outline pen = pen_line (accent). Non-ring cases take the interior pen via ReadPixel (flat.c:1006-1011).
- `flat_dopus_box_draw` flat_glyph.c:536 -> `flat_frame_draw` with fill=0: flat_glyph.c:520-531 draws four 1 px lines (top/bottom/left/right, sw columns) in s->pen_line, nothing else. No corner cut, no interior fill, no label work.
Filled rounded (reference):
- `c8_skip` flat.c:939 (frameiclass DRAW/DRAWFRAME only) -> `c8_draw` flat.c:840. `fill = flat_frame_fill(...)` from IA_EdgesOnly / `push_gadget_of` / `button_gadget_of` (flat.c:717-770, `flat_button_gadget` flat_core.c:772: render==select==frame image). Role via `role_of` -> `flat_button_role` (flat_core.c:749). `raa_frame_draw(&st, state, ox, oy, w, h, fill, role, c2_fill, ...)` flat.c:881: rounded box, radius `raa_radius(h)` (roundaa_core.c:6), antialiased corners (raa_load palette). Fill colour: `flat_frame_draw` fill branch flat_glyph.c:524-527 (primary = pen_line accent, secondary/plain = pen_button face, pressed = pen_text), label pen via `flat_tx_arm` / Text patch.
- Installer border buttons: `border_button_at` + rounded fill at the erase in `flat_rf_c` (flat.c:2051), tag INSTALLER-BORDER-BUTTON (flat.c:788, 868).

## Rig measurements (screen 800x600, panel ground 1a1b26)
- Environment Cancel (CLI Launching page): box x520..619, y396..412. Outline 1 px 7aa2f7 (accent); interior 1a1b26 = ground (no fill); corner pixels square (520,396 and 619,412 are outline). Text a9b1d6.
- MakeDir requester: three buttons y325..341: Ok x381..454, middle x463..~523, Cancel x534..587. Same: 1 px 7aa2f7 outline, interior 1a1b26, square corners.
- Counters (STATUS, rig 68): closing Environment then opening MakeDir moved `dopus calls` 563 -> 570 (+7), `dopus bevel calls` 119 -> 122 (+3 = the three buttons), `face` +3, `d5` +2, `frame=hooked calls` 30 -> 30 and `flat` 7 -> 7 (unchanged: DOpus buttons never hit the frameiclass path). Environment open itself: bevel 117 -> 119 across the Cancel click.
- Reference: RequestChoice (system requester) moved `frame calls` 30 -> 47 (+17) and `flat` 7 -> 10 (+3, three frameiclass fills), `dopus bevel` +2 only (d5 +4 background).
- Filled-rounded PIXEL measurement NOT obtained (unverified): the RequestChoice window did not appear on the grabbed front screen (grab is the one 800x600 screen; PUBSCREEN DOPUS.1 and amiga-M did not show it). Process 13 (C:RequestChoice) is still alive, hidden; I could not break it (Break 13 C / ALL rc 0, still listed).
  Colours the filled path should produce come from flat_glyph.c:524-527 and the earlier rig-63 notes, not from this run.

## Fix intercept point
Intercept `d4_bevelbody` (flat.c:1065) for BEVEL calls that are dopusbuttongclass / dopusiclass BUTTON frames: replace `flat_dopus_box_draw` by the frameiclass draw: `raa_frame_draw(&st, state, x, y, w, h, fill=1, role, c2_fill, c2_read, c2_rgb, raa_load(rp), rp, NULL)` (same call as flat.c:881), then arm the label (`flat_tx_arm` / `flat_tx_frame`, flat.c:889-892) so a pressed label is dark.
Arguments available in the hook: rp, pens (DrawInfo pen array), rect x,y,w,h (WORDs), recessed (==1 means pressed/selected: `recessed == 1` in `d4_bevelbody`), thin (ULONG, unexamined). Disabled state is NOT passed (DOpus ghosts the label itself afterwards; `flat_button_gadget`/disabled-dim logic `pen_dim`, flat.c:1860 rf_disabled, would need the gadget flags: unverified whether obtainable from rp->Layer->Window gadget list by box match, as `button_gadget_of` does, flat.c:760-770).
Draw order (unverified, from stock bevel body only draws edges): stock DOpus draws the box (bevel) and then the label; a rounded fill painted by the bevel hook after DOpus has already painted label or interior would erase it. Needs a rig check: whether the interior/label is drawn before or after the bevel call (dface 443/446 uses ReadPixel at x0+side,y0+1 = the interior is already painted when the bevel hook runs, i.e. the box face is drawn BEFORE the bevel, and the label probably as well). If so the fill must repaint the label (re-Text) or only recolour the interior pen (like a RectFill recolour, flat_rf_c) and clip to the rounded shape instead of a full fill.

## Risks
- The same static bevel serves non-button DOpus frames (dopusiclass frames: groups, info fields, lister borders, dividers; `recessed` 1 = sunken field or pressed button). A button cannot be told from a group box by the hook arguments alone. Discriminators to verify: owning gadget (match rect against rp->Layer->Window gadget list, GadgetType/class, as flat.c:760-770 does for frameiclass), `recessed==0` raised plus size (height ~17 px), or hooking the dopusbuttongclass dispatcher instead of the bevel body and arming a flag for the nested bevel call.
- Fields (FieldBox) and raised DrawBox: owner s71 decision is no ring there, so only BEVEL-from-buttonclass must change.
- AGA/depth and non-square pixel aspect: use `st.side` columns as in `d4_flat_pens`.
- Hook runs on foreign stack with a 384 byte budget (flat.c:842 note): raa_frame_draw must run via the private stack (`flat_on_stack`, c8_draw_on).

## Unverified
- Pixel measurement of a filled rounded reference on rig 68 (see above).
- Whether dopusbuttongclass is the exact caller (inferred from the comment at flat.c:1058-1064 and the +3 bevel counter for 3 requester buttons).
- Label/interior draw order relative to the bevel call.

## Left behind
Rig 68: DOpus Environment open (CLI Launching page, buttons visible); process 13 C:RequestChoice hidden and alive. Scratch scripts a38-*.py in the session scratchpad (tiny); no screenshots taken.
