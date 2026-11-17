# DOpus lister scrollers vs Patina flat knob pen

## DOpus code (source/Program/lister_dual.c:994-1125; Include/dopus/common.h:143-170; Library/listview_class.c:214-250)
- Scroller: NewObject("propgclass", PGA_NewLook, PGA_Borderless, ... DOPUS_PROP_KNOB_TAG(knob)=PGA_KnobImage). Vertical one gets knob = frameiclass FRAME_PROPKNOB (only when Intuition >= 47.44). Horizontal one (lister_dual.c:1065) gets NO knob image -> Intuition stock knob.
- Arrows: sysiclass LEFTIMAGE+a (SYSIA_DrawInfo = GUI->draw_info, display.c:508) wrapped in buttongclass GA_Image (lister_dual.c:996-1110). Standard classes, no custom BOOPSI class, no own pens. Gadgets added with AddGList (:897, :1131).
- No pen set anywhere: all pens come from GUI->draw_info (screen DrawInfo) = Intuition/frameiclass stock FILLPEN (blue).

## Why Patina misses them
- Vertical knob: drawn by frameiclass IM_DRAWFRAME, routed to smart_knob_of (flat.c:762-775, called flat.c:1107). Its test (flat.c:770) demands GadgetType == GTYP_PROPGADGET && GFLG_GADGIMAGE && GadgetRender image with NextImage==obj and AUTOKNOB|SMARTKNOBIMAGE (flat_smart_knob, flat_glyph.c:703). DOpus's gadget is GTYP_CUSTOMGADGET (propgclass boopsi); the PGA_KnobImage lives in propgclass instance data, not GadgetRender/NextImage. Test fails -> stock frame draws in FILLPEN.
- Horizontal knob/containers: scroller_pen RectFill path (flat.c:2062-2080, gadget_has_propinfo flat.c:1993 does accept CUSTOMGADGET via class walk) should recolour, but needs AUTOKNOB box via prop_knob (flat.c:2000) and a PropInfo; PGA_Borderless/PGA_NewLook boopsi knob may not issue a RectFill in that shape. UNVERIFIED (no run).
- Arrows: c2_draw (flat.c:655-680) uses p->knob = flat_knob_state_pen (flat.c:412) for any recognised sysiclass image, and img_lookup records app-made arrows (DOPUS-ARROW-WHICH). Code path should already give knob pen; why they stay blue is NOT determined from source (candidates: draw goes through a path where flat_clone_route/c2_clone is not taken, or arrow images created before Patina swapped the sysiclass hook, or imp_RPort->Layer->Window NULL in dual-lister draw). UNVERIFIED; needs a run with the RL6/fflat counters, not done (read-only).

## Fix options
1. Patina (flat.c): extend smart_knob_of to accept CUSTOMGADGET with gadget_class_has(g,"propgclass") and match obj against the gadget's PGA_KnobImage (OM_GET PGA_KnobImage is not safe in graphics context; instead record frameiclass FRAME_PROPKNOB objects at OM_NEW like img_note_new does for arrows, then test membership). Cost: ~30 lines + core rule in flat_glyph.c + unit test; risk: low-medium (draw-task stack budget, make flat-budget). Fixes every boopsi prop app, not just DOpus. Arrows need separate diagnosis.
2. DOpus source: swap FRAME_PROPKNOB for nothing (horizontal already has none) or make scroller a GTYP_PROPGADGET - not possible with propgclass. Taking a pen: no such API on propgclass/sysiclass. Cost high, forks DOpus, fixes nothing for other apps. Not recommended.
3. DOpus lister prefs / ARexx pens: DOpus has no per-scroller pen setting (grep found none); screen pens are shared with Workbench, so changing FILLPEN would recolour everything. Not viable.

## Recommendation
Option 1, Patina flat patch (the layer that owns the look). First step: one rig diagnostic to learn which of knob/arrows/containers actually fall through (fflat counter and RL6 note) before coding, since the arrow cause is not proven from source.
