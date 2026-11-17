# DOpus dialog bevel callers (s71, agent 40, rig 68, diagnostic build on branch dopus-dialog-flat)

## Part A: reference looks of flat Workbench Prefs gadgets - NOT OBTAINED (unverified)
Rig 68 booted, boot patcher Broken, stock main build (227660 B, 7241d8d1) run from RAM:amarchy-flat: STATUS `flat=on`, but `frame=hooked calls` 31 -> 146 with `flat=0` and `dopus ... flat=0`, `pens-held 16 refs=1 screens=0 applies=0`. Sound and Time Prefs therefore drew STOCK bevels (2 px a9b1d6 shine top/left, 2 px 414868 shadow bottom/right, ground 1a1b26, square corners, no fill; string/integer field in Time: same 2 px bevel, interior 1a1b26; check boxes in Sound: same bevel, 26x11; cycle 'Sound Type'/'November': same bevel + glyph). Those are NOT flat looks. The flat decision (flat_frame_decide / flat_dopus_decide) skips to flat only when the screen's DrawInfo pens are the flat pens; on this boot after Break + Run the screen pens were not applied (screens=0 applies=0). I did not find how the earlier agents got flat pens (the Amarchy theme apply), so no filled-rounded reference was measured. Next agent: find the pen-apply step first (STATUS must show screens>=1 applies>=1, then `frame ... flat` must rise when a Prefs window opens).
Sources that decide (from reading, not measured): button fill/radius c8_draw flat.c:840 -> raa_frame_draw flat.c:881, radius raa_radius roundaa_core.c:6, fill flat_frame_fill flat_core.c:722, colours flat_glyph.c:524-527 (secondary/plain pen_button, primary pen_line, pressed pen_text). Strings/ints: flat_frame_fill edges-only (no fill) flat_core.c:722. Check boxes and cycles: not examined.

## Part B: caller map (return address minus the bevel body address, dsw/dbody[FLAT_EL_BEVEL])
Diagnostic: d4_bevelbody records up to 16 distinct {off, w, h, recessed, thin, count}; STATUS prints `bevcaller off=0x.. w= h= rec= thin= n=`. The offset is caller_return - r->dbody[FLAT_EL_BEVEL] (the bevel body, 0x166d8 in 5.104), so 0x2c9e = call site 0x19370 + 6 (jsr length): matches the s66 note.
Entries accumulate across windows (not reset; counts are totals), attributed by what opened.
| Window | off | w x h | rec | thin | n | Element (by rect vs screen gadget list) |
|---|---|---|---|---|---|---|
| MakeDir requester (214x93 at 323,254) | 0x2c9e | 74x17, 59x17, 47x17 | 0 | 1 | 1 each (47x17 rec=1 once after click) | the 3 buttons; rec=1 = pressed Cancel |
| List Format (274x244 at 293,178) | 0x2c9e | 72x17 | 0 | 1 | 3 | Use/Save/Cancel (3 buttons, listed dopusbuttongclass) |
| | 0x2c9e | 116x17 | 0 | 1 | 4 | the two 116x17 buttons (dopusbuttongclass) |
| | 0x2c9e | 26x15 | 0 | 1 | 5 | the five check boxes (listed `gadgetclass`, 26x15) |
| | 0x4308 | 16x10 | 0 | 1 | 12 | scroller arrow buttons (4 gadgets 16x10, buttongclass; image_class bevel) |
| | 0x0c26 | 88x66, 120x66 | 0 | 1 | 2 each | the two list views (outer rect 104x66 / 136x66 minus scroller 16) |
| | 0x0c4a | 16x46 | 0 | 1 | 4 | scroller tracks (propgclass 8x42 + frame) |
| | (none) | | | | | the two string fields (dopusstrgclass 128x11): no bevel call; they go through FieldBox |
Environment, lister (toolbar, info, path): NOT opened (budget). Group boxes: List Format has no separate frame bevel; no offset for them was observed (unverified).

## Verdict on the discriminator
- 0x2c9e is NOT used only by buttons: the five 26x15 check boxes use the SAME offset (0x2c9e, rec=0, thin=1). Buttons: h=17 (and dialog buttons 47/59/74/72/116 x 17); check boxes: 26x15. Offset cannot split them; rect size can (check box 26x15 is fixed, but a size rule is fragile) or the gadget class (dopusbuttongclass vs gadgetclass) through a gadget-list match as flat.c:760-770 does.
- Scroller arrows 0x4308, list views 0x0c26, scroller tracks 0x0c4a differ from 0x2c9e. Fields: no bevel call at all. So hooking caller 0x2c9e rounds buttons AND check boxes unless split by class/size.
- All calls: thin=1; recessed 1 only for pressed (47x17 pressed once).

## Field pen (dopus string field)
Not measured (budget). Counter evidence only: the two dopusstrgclass fields did not call the bevel hook. Whether strgclass draws it: unverified.

## Source answers
- Arming the interior RectFill: model flat_rf_c (flat.c ~2051): add an arm like flat_tx_arm {surface (layer else bitmap), task, box} set from d4_bevelbody at 0x2c9e; flat_rf_c drops/redirects the next RectFill inside the box (the DOpus interior pens[BACKGROUNDPEN]/FILLPEN fill, drawn after the bevel).
- raa_frame_draw via c8_draw_on from d4_bevelbody: c8_draw_on takes (Object *obj, Msg msg) (flat.c:~842, c8_skip passes the frameiclass impDraw); d4_bevelbody has no Object/Msg, only rp, pens, rect, recessed, thin, caller_return. So not callable as is: either build a local impDraw-like struct (imp_RPort, imp_DrInfo from pens, bounds) -- the 384 byte hook stack forbids locals, so use the shared record, or add a c8-style entry taking (rp, x, y, w, h, fill, role, recessed) that runs raa_frame_draw (needs: flat_state, state, ox, oy, w, h, fill, role, c2_fill, c2_read, c2_rgb, raa_load(rp), rp, NULL) on the private stack via flat_on_stack. All of those except role/fill are derivable from rp and g_r. Unverified by compile.

## Commit, build, checks
- Worktree /Users/spot/Code/UP-Term/vtcon-dopus-dialog-flat, branch dopus-dialog-flat. Diagnostic build 228412 B at vtcon-dopus-dialog-flat/build/amiga/amarchy-flat; resident on rig 68.
- Commit 839b4526. `make flat-budget` PASS. `make test ONLY=flat_core`: PASS, 4214 checks 0 failed (generated menu_icons.inc was copied from the main checkout build/amiga because fontTools is missing).

## Unverified
Part A numbers (all), Environment, lister, group boxes, field pen, Environment opening method (right-Amiga 4 untried this run; `command Configure` works for List Format).

## Flat state, reference looks, Environment map (s71 agent 41)
Rig 68, purpose "s71 session: flat reference looks and DOpus Environment map". Scripts in scratchpad a41-dump.py / a41-cen.py (grab census).

### Step 1: the flat state
- Fresh `rig.py reboot`, nothing else: `dripens` shows pens `0 1 2 0 0 3 0 0 2 2 0 2` (SHINE=BACKGROUND=0 = flat set), boot patcher (UP-Term:bin/amarchy-flat, Process 13) STATUS flat=on, `frame=hooked` flat count rises when Time Prefs opens (0 -> 38 of 153). So the flat pens come from the BOOT desk apply (Amarchy start, ENV:AMARCHY_DESK_START_WRITES=3, PLAN=22), not from the patcher. `screens=0 applies=0` is NORMAL: pens_plan counts only slot >= 1 screens (iconpen_dos.c:289), Workbench is slot 0; do not use it as the pass criterion. Use `RAM:dripens` (SHINE pen == BACKGROUND pen) and `frame flat` rising.
- Break of Process 13 + Run of the diagnostic RAM:amarchy-flat (228412 B) leaves the pens flat (dripens identical before and after). So the documented Break/Run procedure is NOT the cause of the earlier failure. The previous boot had dripens stock `0 1 1 2 1 3 1 0 2 1 2 1` with ENV:AMARCHY_DESK_START_WRITES=1 (this boot: 3): the boot desk apply only partly landed that time (palette.prefs written 17:44:56, live pens never changed). Cause unknown (unverified; looks like a race/partial apply at Amarchy start, a real intermittent defect candidate). Fix for the measurer: after reboot run `RAM:dripens`; if pens are `0 1 1 2 ...` reboot again before measuring.
- State now: diagnostic binary running, flat=on, pens flat.

### Part A: reference looks (Workbench Prefs, flat, measured by pixel census)
Ground 1a1b26 (window), accent 7aa2f7, text a9b1d6, secondary fill 3e4254, disabled fill 0e0e14.
- Button (Save/Use/Cancel 87x14): fill 7aa2f7, NO outline, corners rounded radius ~4 (corner pixels 1a1b26 / 30344b / 5570aa antialias over 3 px), label 1a1b26. Disabled button: fill 0e0e14, label 414868, same rounding. Pressed look: not driven (unverified).
- Integer field (Time "2026", box 54..107 x 32..45): 1 px outline 7aa2f7, interior 1a1b26 = window ground (NO own ground), square corners, text a9b1d6. Cursor not captured (field not active; unverified).
- Cycle gadget (Time "November" 178x14, Sound "Sound Type"): fill 3e4254, no outline, rounded radius ~4 (1f2130/30344b antialias), label a9b1d6, no arrow glyph seen.
- Check box (Sound, 26x11): NO box at all, no outline, no fill; selected = an a9b1d6 tick of ~10x6 px only; unselected = nothing visible (invisible box).
- Slider (prop, Sound volume): 7aa2f7 knob on 1a1b26 trough.

### Own-UI input with its own ground
None found. gthemes_core.c:566 defines the token "text-field" ("string gadget fill", group GTG_BACKGROUND) and :567 "text-cursor" (GTG_ACCENT), but no dist/amarchy-themes/*.theme sets text-field and nothing else in amarchy/ prefs/ names it; the only measured input (Workbench Time integer field) has the window ground 1a1b26 plus a 7aa2f7 outline. So "a bg colour like our other inputs" has no existing reference: the owner needs to pick (suggest the token text-field / 0e0e14 darker or 3e4254 secondary).

### Step 3 Environment: NOT DONE (over budget, ~50 calls)
- `rexx "ADDRESS DOPUS.1; command Configure"` opens List Format (same bevcaller entries as before: 0x2c9e 26x15 n=5, 116x17 n=4, 72x17 n=3, 0x4308, 0x0c26, 0x0c4a), not Environment. Right-Amiga 4 (ami.key(0x04,0x80)) sent with DOpus screen in front: no new bevcaller entries, no Environment window confirmed. Menu path by mouse not tried. No new caller offsets, no element verdicts for Environment, lister toolbar/info/path: unverified.
- End state: rig 68 up, DOpus screen in front with List Format open (Environment not open).
