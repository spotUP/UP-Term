# border-hunt (s27): who draws the outer window border lines, and where to hook the ring
Probe: scratchpad/bprobe/bprobe.c (not committed). SetFunction on graphics RectFill(-306)/Move(-240)/Draw(-246)/SetAPen(-342)
and intuition DrawImageState(-618); logs every call whose rp->Layer is window A's layer (or Layer->Window == A), with nesting
inside DrawImageState. Window A 220x90 (close, depth, zoom, size, drag), window B overlaps. Phases: 1 RefreshWindowFrame(A),
2 ActivateWindow(B) (A deactivates), 3 ActivateWindow(A), 4 ChangeWindowBox bigger, 5 WindowToBack, 6 WindowToFront.
Raw: bprobe/out32.txt (rig 8, intuition V47.53, amarchy-flat running with ring), bprobe/out31.txt (rig 8 --os 31, V40.85, stock).
Pen column in the logs is NOT reliable (Intuition sets pens without SetAPen); ignore it. out32 coords are offset by -20 (probe bug, fixed for 3.1).

## Measured sequence per redraw (V, both OSes)
3.2 RefreshWindowFrame: 8 bevel RectFills -> 3 fills (left/right side, title bar) -> title Move/Text -> 4x DrawImageState
  (each: gadget box fills, glyph, then our c2 ring 4 RectFills 0,0..219,0 / 0,89..219,89 / 0,1..0,88 / 219,1..219,88)
  -> AGAIN the 8 bevel RectFills: outer 0,0-219,0 ; 0,1-0,89 ; 219,1-219,88 ; 1,89-219,89 ; inner 3,13-202,13 ; 3,14-3,88 ; 202,14-202,87 ; 4,88-202,88.
3.2 deactivate / activate: fills -> title -> 4x DrawImageState (+ring) -> the same 8 bevel RectFills LAST.
3.1 RefreshWindowFrame: 8 bevel RectFills -> fills -> title -> same 8 bevel RectFills LAST. No DrawImageState calls on 3.1
  (gadget images are rendered internally, not through the LVO).
3.1 deactivate / activate: fills -> title -> 8 bevel RectFills LAST.
=> The outer lines are drawn by Intuition-internal border code (no public LVO, no BOOPSI class) through the graphics
   RectFill LVO, one 1 px RectFill per edge, and they are the LAST calls of every frame redraw on 3.1 and 3.2. That is why the
   ring drawn inside sysiclass IM_DRAW is erased.
Phases 4-6: ZERO calls on both OSes. Probable cause (U): ChangeWindowBox was clamped (no WA_MaxWidth/Height given, max = open
  size), and depth changes of SMART_REFRESH windows restore from backing store without any border redraw. Not measured:
  resize with a real size change, SIMPLE_REFRESH damage refresh, window open (OpenWindow happened before the patch).

## Hook point
Only hook that sees every redraw on both OSes: graphics RectFill LVO (SetFunction GfxBase -306). Activate/deactivate run
inside the input handler and never pass a public Intuition LVO, so OpenWindow/RefreshWindowFrame patches miss them.
Selector without ROM addresses: rp->Layer->Window = w, !(w->Flags & WFLG_BORDERLESS), rect is exactly one of the 4 outer
edge rects in layer coords (y0==y1==0; x0==x1==0; x0==x1==w->Width-1; y0==y1==w->Height-1) -> replace it with a RectFill of
the same rect in pen_outline (or call original, then refill). Optional tighter gate: caller return address inside
intuition's code range from FindResident("intuition.library") (rt_MatchTag..rt_EndSkip), portable 3.1/3.2.
Reuse: the LVO patch path of D4 (flat.c d4_lvo / flat_jmp_target, ~420-432) applied to GfxBase, same save/restore discipline.
Owner history: p.md excluded a frame-draw hook, outer-edge-route.md rejected a caller-range RectFill filter as "3.2 ROM addresses";
the geometry selector removes the ROM-address objection, but a hot-path RectFill patch is an owner decision.
