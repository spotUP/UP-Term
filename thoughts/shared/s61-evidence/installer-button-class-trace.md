# Installer button class trace (rig 98, DOpus 5.104 Installer, AmigaOS 3.2, page 1)

Measure only, no product edits. Probes: cprobe (class chain, Border dump, refresh), fprobe (frameiclass dispatcher hook), tprobe (RectFill/Text/Move/Draw + frameiclass method order during RefreshGList of one gadget). Sources were in the session scratchpad (small, not committed).

## 1. Class chain
- Buttons 90/91/100/1 are NOT BOOPSI objects and NOT ReAction. Gadget type 5 here is a plain struct Gadget with GadgetRender and SelectRender pointing at two distinct struct Border chains (5-point outlines), GadgetText NULL. No button.gadget or bevel.image is installed. Installer is 43.3 (30.04.96). So there is no class ID or cl_Super chain for the buttons or for their render "objects" (they are Border structs).
- Intuition draws the Border renders by creating a temporary frameiclass object (OM_NEW 0x101), sending IM_DRAWFRAME 0x202, OM_GET 0x104, then OM_DISPOSE 0x102. Caller seen by fprobe: intuition.library+0x6c14. The temp object (e.g. 0x1023fb14) is neither GadgetRender nor SelectRender nor the gadget.
- That is why c8_skip does not treat it as a push button: gadget_of_image needs render == select == obj. Hypothesis (unverified: not stepped through c8_skip): the call reaches the swapped dispatcher with fill=0 and the push-button test fails, so the ring is drawn.

## 2. Ring draw call
Refresh of gadget 90 (box 8,174 238x16), STATUS deltas: rectfill calls +5, hits +0, shits +0, text calls +1, frame=hooked calls +4 (flat +1).
tprobe order (event kind was not recorded by the probe; kinds inferred from fields, so partly unverified):
1. RectFill 8,174-245,189 pen 0: Intuition body erase of the whole gadget box.
2. Move 81,185 then Text n=20 pen 2: the label, drawn by the Installer/Intuition from the gadget before the frame.
3. frameiclass OM_NEW, IM_DRAWFRAME, OM_GET.
4. Four RectFill 1-px strips pen 3: top 8,174-245,174; bottom 8,189-245,189; left 8,174-8,189; right 245,174-245,189. This is the accent ring. It is RectFill (LVO -306) issued inside IM_DRAWFRAME, but the flat RectFill hook does not count it (hits 0, shits 0), so its ROM-caller/one-strip gate does not match these four strips.
5. OM_DISPOSE.

## 3. Label order and hook point
- The label is drawn BEFORE the frame. A rounded fill drawn at frame time would erase the label.
- Smallest hook consistent with flat.c: extend the existing c8 frameiclass swap and the RectFill hook as a pair.
  a. Recognise the Border-render gadget: in c8_skip, when the IM_DRAWFRAME box matches a window gadget (type 5, render != select, Border render, GadgetText NULL), treat it as a push button: skip the ring (suppress the four pen-3 strips) and do NOT fill there.
  b. Draw the rounded solid fill at step 1 instead: in the RectFill hook, a pen-0 fill whose rectangle equals such a gadget's box gets replaced by flat_frame_fill (rounded), so the Installer's own Text later draws over it. Alternative: fill in c8 then redraw the label by hand (needs gadget label text, which the gadget does not carry: GadgetText NULL), so (b) is preferred.
- Unverified: the 1 frame call fprobe saw versus the +4 counter delta; whether the pen-0 erase comes from the same ROM caller for all four buttons (only 90 fully traced; 91/100/1 share the 238x16 geometry and flags).
