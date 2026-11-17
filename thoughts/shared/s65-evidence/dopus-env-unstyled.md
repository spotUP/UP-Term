# DOpus Environment window unstyled under amarchy-flat (s65, rig 65, KS 3.2, 640x256, DOpus 5.104)

STATUS: PARTIAL. Miss pinned by code reading plus rig measure; the themed (flat-pens) state was NOT reproduced on the rig:
the theme pick (amarchy_desk_rig d.pick, vantablack row) froze the rig (wasabid dead 150 s, rig 65 restarted). Unverified: what the window looks like under flat pens.

## 1. What draws the window (measured, ami.py uitree)
All Environment gadgets are `custom` (BOOPSI, dopus5.library button_class / dopusiclass / dopus listview and string-field classes), not GadTools, not sys images.
Only the 3 sys gadgets (depth, close, drag) are system. Window 26,33 588x189 on 640x256 screen.

## 2. Patch paths
- frameiclass hook (flat.c:601-780 c8_skip, flat_core.c flat_frame_decide): only IM_DRAWFRAME on a real frameiclass object; it paints flat fill + outline itself (flat_frame_draw). This is why Installer/system buttons get flat fills.
- DOpus LVO patch FLAT-D4 (flat.c:795-835 d4_flat/d4_drawbox/d4_fieldbox; decision flat_core.c:194-209 flat_dopus_decide): under flat pens (shine==back, or same RGB) it returns FLAT_DRAW_SKIP, i.e. DrawBox (-696) and DrawFieldBox (-702) draw NOTHING. No outline, no fill is drawn in their place (header flat.c:14-20 says so: "a routine that draws nothing").
  This is the erasing path: DOpus bevels are dropped, and nothing replaces them (the frameiclass route's flat_frame_draw is not reachable from it).
- Not routed anywhere: draws inside dopus5.library that bypass the LVO (flat.c:18-20: path field button_class.c:900, scroller, sort arrow). Those keep the stock one-sided L or nothing.
- borderblank (amarchy/borderblank_*.c): window borders, not gadget boxes; not involved in the gadget frames. No bottom-border handling found: grep BorderBottom/bottom in amarchy/flat*.c gives only flat.c:1115,1294,1564 (GRELBOTTOM gadget math) and :1644 (rf_side, passes w->BorderBottom to flat_side_fill_pen) - no code that draws or blanks a window's bottom edge.

## 3. Flat ON vs OFF (rig, pens NOT flat: amarchy-flat started, theme not picked)
- OFF (screenshot read, not kept): bevelled frames on every field, listview, buttons, checkbox boxes (drawn as raised/tick), bottom window border line visible at y~222.
- ON, stock pens (dopus-env-flat-on-nopens.png): identical. STATUS `dopus=73.8 patched=3 calls=0 flat=0`: the Environment window made ZERO DrawBox/DrawFieldBox LVO calls, so the d4 patch is not reached by this window at all with non-flat pens (it also returns STOCK when pens differ). Bottom border present in both.
- Window bottom = 33+189 = 222 < 256: window is NOT off screen. Owner's "bottom chopped" = border/bevel issue, not geometry.
- The owner's unstyled look needs flat pens (shine==back). With them, d4_flat -> SKIP for every DrawBox/DrawFieldBox call the window makes (expected: all its frames, since stock pens show them via these calls or inside-library draws), nothing drawn instead. Expected, unverified.

## 4. Proposed fix level (no code)
Root cause level: flat_dopus_decide's FLAT_DRAW_SKIP in d4_drawbox/d4_fieldbox (flat.c:817-835). Under flat pens it must draw the flat result (box fill + 1 px outline, as c8_skip/flat_frame_draw does for frameiclass), not nothing; field vs button vs recessed (flat_dopus_decide's `recessed` is ignored, flat_core.c:196) selecting the fill role. Then verify checkbox/listview/string fields: those drawn inside the library bypass the LVO, so either patch their internal callers' entry (button_class draw path) or hook dopusiclass IM_DRAW, as the file header already lists as unreachable. Bottom border: needs a separate look in the themed state; see the theme-requester border agent.
Next step to confirm: reproduce with the theme applied (not via the pick that froze the rig; write the flat state/prefs and pens directly) and read STATUS dopus calls/flat counters while opening Environment.

## Found
- Theme pick with flat running and DOpus on the Workbench screen froze rig 65 (wasabid silent 150 s). Not investigated; possible separate bug.
