# DOpus lister path field border (s71 agent a20, rig 68, flat d588f8e9)

Finding: no bug in coverage. The path field's coloured border is flat's own outline, drawn by the FieldBox body hook. The hook IS installed.

STATUS (flat resident): `dopus=73.8 patched=7` (mask 7 = DrawBox|FieldBox|Bevel all patched), `dopus bevel calls=0 face=3331 d5=1118`, frame=hooked.
Lister refresh (`dopus refresh lister`): calls 4923->4995, flat 4923->4995, face 3331->3403 (+72 each; all no-ring button boxes). The path field was not redrawn by it (ring calls would raise calls/flat without face).

Pixels (800x600 screen grab, front lister field, via ImageMagick txt: dump; PIL absent):
- Flat: field box x=186..548, y=528..542 (363x15). 1 px outline all four sides, colour #7AA2F7 (flat pen_line, same colour as the lister window frame and active scroller knob). Interior #1A1B26 (window background), label text #414868. Margin around the box: #1A1B26.
- Stock (Break C + `dopus refresh all`): same box area contains ONLY #1A1B26 and the #414868 text: no outline pixels (stock shine and shadow pens equal the background colour on this theme). Stock draws the same pixels in no colour.
- After restart: row 528 identical to flat run (outline back, 369 blue px on that row incl. window frame).

Draw call: DOpus Library/button_class.c:900 draw_field_box -> Library/gui.c:67-90 (shine/shadow Move/Draw L lines), reached through dopus5.library FieldBox body, hooked by flat d4_fieldbody (amarchy/flat.c:976) -> d4_flat (:949) -> d4_flat_pens (:909): flat_dopus_decide (flat_core.c:281) returns SKIP because shine pen == back pen -> stock skipped -> flat_dopus_ring(FIELDBOX,0) = 1 (flat_core.c:302, ring kept; only non-recessed DrawBox buttons get no ring) -> flat_dopus_box_draw (flat_glyph.c:529) -> flat_frame_draw ring branch flat_glyph.c:522-525 with s->pen_line.
So the survey candidates (hook missing, RectFill gate, no handling) are ruled out: the hook runs; the outline is intended output (header comment flat.c:17-20 stale). Interior fill (button_class.c:935-960 RectFills, Pens[1]) is bg and unchanged, correct.

Fix location (view): where the field ring is chosen, flat_dopus_ring (flat_core.c:302) for FLAT_EL_FIELDBOX / the pen d4_flat_pens gives it (pen_line); owner decision needed: no ring (reuse the FLAT-BUTTON-NO-RING path: st.pen_line = ReadPixel interior pen, as d4_flat_pens :931-935 does for buttons) or a muted pen. Not measured: how GadTools string fields (frameiclass) look in the same session.

## Fix dopus-field-no-ring (s71 agent 23)

Branch dopus-field-no-ring in the worktree /Users/spot/Code/UP-Term/vtcon-dopus-field-no-ring, commit e8c0dc0c, based on main d588f8e9. Owner decision of 2026-11-10: string fields get no ring at all, scope all FieldBox fields.

Changes. flat_core.c flat_dopus_ring (line 317): FLAT_EL_FIELDBOX returns 0; recessed DrawBox and bevel stay 1. New pure function flat_dopus_face_pen(read, pen_back) in flat_core.c/.h: read when >= 0, else the DrawInfo background pen. flat.c d4_flat_pens (about line 938) uses it, so a failed ReadPixel can no longer leave the accent default of pen_line. New counter dnopix in flat_shared.h next to dface, printed in STATUS on the "dopus bevel calls=... face=..." line as " nopix=<n>". No test or tool parses that line (grep for face= in tests/ and tools/ found nothing). The flat.c header comment now says the path field body is hooked (d4_fieldbody); comments at flat_core.h and above d4_flat_pens record the reversal. Note: the info-field accent frame is explained by the -1 read fallback, but whether ReadPixel really returned -1 there is still unmeasured; nopix= in STATUS on the rig will show it.

Fail before (ring change reverted by hand, new tests in place): "FAIL tests/test_flat_core.c:569: flat_dopus_ring(FLAT_EL_FIELDBOX, 0) = 1, want 0" and "[FAIL] flat_core: 4198 checks, 1 failed". Pass after: "[OK] flat_core: 4198 checks, 0 failed"; flat_hook_locks (2 tests) and flat_clone_stack (6 tests) OK. make flat-budget rc 0, PASS; tightest headroom lines: _flat_rf_c 64, _flat_tx_c 72, _c2_disp_reg 68, _c2_disp_stk 76, _d5_disp_reg 80.

Binary: /Users/spot/Code/UP-Term/vtcon-dopus-field-no-ring/build/amiga/amarchy-flat, 225608 bytes, md5 411468a063f82f692e24b4f323f006d5. Not yet run on a rig.

## Rig 68 proof e8c0dc0c (s71 agent 25)

Binary RAM:amarchy-flat 225608 bytes, md5 411468a0...6f082 verified on the host before the push; Amiga-side List shows 225608. Started with `Run >NIL: RAM:amarchy-flat`. Stale RAM copies were already gone. The rig had 19 DOpus listers (15 stacked DH0: leftovers); 17 were closed with Wait 1 between, leaving the rear DH0: lister (0/0/444/574) and the front DH1: lister (182/93/385/460).

1. STATUS: flat=on calls=7, dopus=73.8 patched=7, `dopus bevel calls=0 face=0 nopix=0 d5=0` before any redraw. PASS.
2. FAIL. After `dopus refresh all` + Wait 1 (and again after 10 more refreshes): info frames rear 4,14,345,26 and front 186,107,468,119 are #7AA2F7 on all four sides, every pixel (rear 342/342 top and bottom, 13/13 sides; front 283/283, 13/13). Interior #1A1B26 plus text. face rose 0 -> 37 -> 74 -> 444, nopix stayed 0 throughout.
5. Because nopix=0 the ReadPixel did not return -1: the sample is not failing, so the accent frame in the info field comes from a different cause (the sample is taken before the area fill, or the info frame is not drawn through the d4_flat_pens path; not separable from pixels). The fix does not remove the info-field border. Frame colour seen: #7AA2F7.
3. PASS. Front path field text rows 531..538 (x from 191), rear path field text rows 552..561 (x from 9): the rows above and below (530/539, 551/562) and the left edge are only #1A1B26 (front: the only #7AA2F7 in x184..549,y520..544 are 2 pixels at x=544,y=526/539, part of the scroller knob); no frame pixels. The field redraws with `dopus refresh all`. (Whether the redraw was by the new code is shown by face counts rising, not by a per-field counter.)
4. Partial. Rear title strip contains only #545A77 and #1A1B26 (flat). Toolbar buttons are grey icon bitmaps (#B3B3B3/#CACACA/#EDEDED ramps, 1 px #B3B3B3 border inside the icon image); no baseline grab of an earlier build, so "no new stock bevel" cannot be separated from icon content: NOT MEASURED. SRCE/DEST faces and a recessed toolbar button: NOT MEASURED (not located).
6. PASS. 10 x (`dopus refresh all` + Wait 2) over about 20 s: loop beat 148 -> 170, stage=wait, refused=none, no guru, wasabid answers, flat=on.

Final state: test build resident (process 12 RAM:amarchy-flat), DOpus with exactly two listers (rear DH0: top-left, front DH1: source), last `dopus refresh all` done.

## Environment and requester fields, eye-check setup (s71 agent 36)
- Rig 68, RAM:amarchy-flat 227660 bytes, STATUS flat=on (calls 1848 -> 2215), refused=none. DOpus had no lister open; `lister new` made one (source lister, set to RAM: via `rx "options results; address 'DOPUS.1'; 'lister query source'; h=result; 'lister set' h 'path RAM:'"`; its file list drew empty, unexplained).
- B, requester (`rx "address 'DOPUS.1' 'command MakeDir'"` -> "Directory Opus Request", "Enter directory name"): field rows y306..324, x378..590 in a 800x600 grab. Colours in the field area: 1a1b26 (ground, 4194 px), e5e4d9 (cursor, 66 px), 7aa2f7 (404 px). The 7aa2f7 pixels are only full-width horizontal lines at y304/305 (bottom of the prompt text box above) and y325 (top of the button row); no left/right/top/bottom frame hugs the field. Field ground 1a1b26 == panel 1a1b26: field is invisible as a box. Cursor visible: solid block x385..390, y310..320, e5e4d9. MEASURED: frame NO.
- A, Environment (Settings > Environment..., key right-Amiga 4; it did not open by `command Environment`; `command Configure` opened the List Format window instead): pages checked Directories (integer fields "20", "107"), CLI Launching (string fields Title "Directory Opus Output", Device "CON:", integer Stack 8192, Max. CLI 512). Pixels around Title/Device (x330..600, y236..272): 1a1b26 9267, 414868 315 (ghosted text), a9b1d6 138 (labels); no 7aa2f7. Panel colour 1a1b26 == field ground. These fields look ghosted (414868 text), no cursor. MEASURED: frame NO. Locale page has no string fields; Backgrounds has ghosted path fields (not measured).
- Coexistence: NO. While the requester is open, keys go to the requester (right-Amiga 4 typed "4" into its field); the lister shows BUSY and menus are unreachable. Environment, List Format and requesters all make the lister BUSY.
- Mouse: `ami.py click` is not a command; use `python3 -c "import ami; ami.click_px(x,y)"` from tools/rig. Cancel of a modal window by click_px works.
- Owner steps for B after Environment: click Cancel in Environment (569,404), then click the MakeDir button in the right-hand button column (x754,y225), or `rx "address 'DOPUS.1' 'command MakeDir'"`; requester appears at screen centre-ish (375..587, 254..347). Cancel button 558,333.
- Screenshots a36-*.png deleted.
