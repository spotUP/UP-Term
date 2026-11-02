---
date: 2026-11-02
topic: sysiclass window-gadget clone returns empty grid on AmigaOS 3.2
status: cause unproven, probe built
---

# Glyph clone empty on 3.2

## Question
Why does the clone in amarchy/flat.c clone_render (161-195) read an all-zero grid after
the saved sysiclass IM_DRAW on OS 3.2, so amarchy/flat_glyph.c flat_clone_draw (~206-209)
returns FLAT_CLONE_FAIL and the 99fa0fe5 block-glyph fallback runs?

## Known before this session
- 3.1 rig 38: clone works (draws=5 fallback=0). 3.2: draws=0 fallback=46 (thoughts/spot/todos.md:3659, :3679).
- 2026-11-02-flat-gadgets-invisible.md:70-95: four passes (layerless offscreen + marker DrawInfo,
  layered rastport, layered + real DrawInfo, offset 0,0 + real DrawInfo) all read zero=13 rows of 0.
  ReadPixel readback works; dispatcher returns 1.
- So DrawInfo pens, layers and offset are already ruled out as the single cause.

## Clone setup (flat.c)
- clone_ensure 129-150: AllocBitMap(w,h,8,BMF_CLEAR,NULL): 8 planes, no friend bitmap. InitRastPort, no Layer.
- clone_render 161-195: copies impDraw and DrawInfo, dri_Pens = marker pens (i+1), imp_RPort = &cl_rp,
  imp_Offset = -LeftEdge/-TopEdge, SetRast 0, calls saved dispatcher (a0 hook, a2 obj, a1 msg on 3.2).

## Open hypotheses (to discriminate with the probe)
1. 3.2 sysiclass draws through screen-friend / screen-depth assumptions (bitmap depth or friend differs).
2. The 3.2 window-gadget images are not drawn by IM_DRAW of the object we hook (Intuition draws a
   cached/pre-rendered image tied to the screen; our call into the saved dispatcher renders nothing).
3. Re-entry: 3.2 sysiclass calls sysiclass again inside IM_DRAW; c2_draw re-enters with imp_RPort=&cl_rp.

## Probe
thoughts/shared/research/s30-evidence/glyph-clone-clprobe.c (build: vc command from `make -n -B build/amiga/dripens`): NewObject sysiclass
DEPTHIMAGE/CLOSEIMAGE with screen DrawInfo; DrawImageState into B 8-plane no-friend, C screen-depth
friend, D screen-depth no friend, E 8-plane marker DrawInfo, G screen RastPort control; then the
GadgetRender images of a real window's system gadgets (B-E). Prints pixel grid and nonzero count.
Run with amarchy-flat NOT active.

## Result
Not run. Rig 55 never answered: Amiberry hung at boot (App Nap throttle, 0.1% CPU); fs-uae start
hit the kit clone slowly and a 500 s timeout killed it; the third start was stopped by the
coordinator at budget. No probe output, no fix, no rig pixel proof.

## Next step
1. Boot one OS 3.2 rig (fs-uae), STATUS shows amarchy-flat not active.
2. Put and run glyph-clone-clprobe; read which of B/C/D/E/G and the WIN gadget images are nonzero.
   - G nonzero, B-E zero: offscreen target is the problem (check C friend vs D depth).
   - NewObject images draw but WIN gadget images do not: Intuition-owned window images are
     cached/drawn outside IM_DRAW on 3.2 (hypothesis 2); clone must use a fresh NewObject sysiclass image.
   - All zero including G: probe draw path itself wrong; recheck.
3. Then rerun with amarchy-flat active to test re-entry (hypothesis 3).
