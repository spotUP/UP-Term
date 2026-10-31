# FLAT-BUTTON-FILL route measurement (s28, rig 8, gprobe.c)

Caveat: screen palette was stock (flat pens not applied): `frame=hooked calls=0 flat=0`. Attributes and draw order below are Intuition's own; the flat fill/pressed look was NOT exercised. Logs: button-32.log, button-31.log, gprobe.c.

## Q1 attributes per kind (3.2, Kickstart 47.115)
- BUTTON: `F obj=10128694 state=0 box=10,20 0x0 eo=0 ft=1 rec=0 mine=1 fg=1`
- CYCLE identical: `F obj=101289cc state=0 box=260,20 0x0 eo=0 ft=1 rec=0`
- Listview scroller arrows identical (~15x8): `F obj=10128f44 state=0 box=134,116 0x0 eo=0 ft=1 rec=0`
- STRING: `F obj=10127ec4 ... box=130,20 eo=-1 ft=2 rec=0`; LISTVIEW frame `eo=-1 ft=1`; SLIDER frame `eo=-1 ft=1`
- Bevel box: `F obj=101294ac state=0 box=200,140 0x0 eo=-1 ft=1 rec=0`; recessed one `rec=-1` (IA_Recessed unanswered, colours swapped instead)
- Slider/scroller knob is IM_DRAWFRAME: `W obj=100597ec state=0 box=264,82 9x10 eo=0 ft=5 rec=0`
- CHECKBOX, MX: no frameiclass use (RectFill + Text only).
- (-1 = GetAttr returned 0, unanswered.)

## Q2 order and pen
- Frame drawn first, label Text right after. Normal: `T fg=1 bg=0 drmd=0 n=6 xy=36,30 "Button"` (JAM1, pen 1).
- Pressed: `F obj=10128694 state=1 ...`; stock fill `R 12,21 107,34 fg=3`; then label again `T fg=1 bg=0 drmd=0 n=6 xy=36,30 "Button"`. Label pen stays 1 on press.
- One redraw pair per press (F state=1, rects, T; release F state=0, rects, T; then GADGETUP). No redraw during hold. BUTTON_KIND sends no GADGETDOWN.
- Consequence: dark label on pressed accent fill cannot be set in the frame hook; Intuition sets pen 1 at Text after it. Needs a graphics Text patch, or accept pen 1 on accent (pick accent contrast accordingly).
- Buttons have no GadgetText (text=0): label drawn inside render path.

## Q3 distinguishing property
- 3.2: IA_EdgesOnly answered and == 0 marks button, CYCLE and scroller arrows; all other frameiclass uses leave it unanswered. This equals the existing gate in flat.c. No attribute, size or hook-visible property separates button from CYCLE/arrows (box 100x16 vs ~15x8 is the only hint).

## Q4 OS 3.1 (Kickstart 40.68, Workbench 40.42)
- Every frame logs `eo=-1 ft=-1 rec=-1`, buttons included: `F obj=10058864 state=0 box=10,20 4096x-17232 eo=-1 ft=-1 rec=-1`. So on 3.1 the gate `GetAttr(IA_EdgesOnly)==0` NEVER fills a button. Not distinguishable by attributes at all.
- Dimensions `4096x-17232` is garbage: my stack-convention wrapper may misparse args on 3.1 (unverified), so the -1 may be partly a wrapper artefact. imp_State and box origin look sane.
- Draw order same: `F obj=10058864 ... T ... xy=44,31 "Button"`. Label drawn after frame.
- Presses on 3.1 MISSED (I used window-relative coords from 3.2; 3.1 has bt=14, bl=4: button centre is about 84,62). Pressed state on 3.1 not measured.
- Only route left on 3.1: a different test, e.g. box origin/size from the draw call plus Text-follows-frame, or patch at Text-after-frame level.

## 3.1 rerun (rig 8, Kickstart 40.68, Workbench 40.42, stock palette, flat not active; log button-31-rerun.log, gprobe.c fixed)
- Wrapper: the h_SubEntry stack-convention wrapper was RIGHT (obj/state/box sane in both runs). The garbage `4096x-17232` was imp_Dimensions: on 3.1 IM_DRAW does not carry it (message ends before it). Fix: log im->Width/Height. Now `F obj=1002b714 state=0 box=10,20 100x16`. flat.c already falls back to im->Width when imp_Dimensions.Width is 0, but 3.1 may return junk, not 0: check `d->imp_Dimensions` use on 3.1 (flat_conv / c8_skip line ~178).
- Also found: the old probe never restored the frameiclass slot at exit, so a second run crashed the rig (wasabi reset). Fixed in gprobe.c (static slot, restored).
- Q1: GetAttr(IA_EdgesOnly/IA_FrameType/IA_Recessed) is unanswered on 3.1 for EVERY frame: `eo=-1 ft=-1 rec=-1` on BUTTON (`F obj=1002b0dc state=0 box=10,20 100x16`), CYCLE (`box=260,20 100x16`), listview scroller arrows (`box=134,117 16x11`, `box=134,106 16x11`, `box=134,80 16x26`), LISTVIEW (`box=10,80 124x48`), SLIDER (`box=260,80 100x14`), bevel boxes (`box=200,140 80x30`, `300,140`). So the flat.c gate `GetAttr(IA_EdgesOnly)==0` never fills on 3.1 (same as before); frameiclass V40 does not answer OM_GET.
- Q1 other property on 3.1: no attribute or size separates them (CYCLE and BUTTON are both 100x16; BUTTON/CYCLE use IM_DRAW like all). Measured pattern from gprobe.pos (previous run, same code): BUTTON and CYCLE gadgets have GadgetRender == SelectRender == the frame object, flags 8006; STRING has a border (select=0, flags 8204); LISTVIEW/SLIDER select=0 flags 8007. Gadget-lookup (like flat.c does for sysiclass on 3.1: window gadget whose GadgetRender is this image) with render==select marks button and cycle frames. Scroller arrows and bevel boxes are not window gadgets in that list (arrows only visible via uitree); not measured whether the lookup hits them. Unverified inside the hook.
- Q2 pressed (mouse down held 3 s, left button = button index 0 in ami.script; index 1 was why presses missed; screen 800x600, window 20,20, button centre 80,48):
  `F obj=1002b0dc state=1 box=10,20 100x16 eo=-1 ...` then `R 10,20 108,20 fg=1`, `R 10,21 10,35 fg=1`, `R 11,21 11,34 fg=1`, `R 109,20 109,34 fg=2`, `R 108,21 108,34 fg=2`, `R 11,35 109,35 fg=2`, `R 12,21 107,34 fg=3` (fill pen 3), then `T fg=1 bg=0 drmd=0 n=6 xy=44,31 "Button"` (label AFTER frame, pen 1, JAM1). Release: `F state=0`, rects (shine/shadow swap back, fill pen 0), same `T` again, then `M GADGETUP`. One redraw pair each, none during the hold. Same for CYCLE (`F state=1 box=260,20`, fill `R 262,21 357,34 fg=3`, `T ... "One"`; after release the label changes to "Two" in a third pair).
- Identical to 3.2 order and pens; difference on 3.1 is only the missing OM_GET and missing imp_Dimensions.
