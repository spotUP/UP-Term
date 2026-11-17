# S85: inactive DOpus lister title = black block, no text (vantablack, F9 hotkey, OS 3.1)

Code reading only, no rig. Premise corrected by coordinator: live theme vantablack (F9, gt_dos_select path, DOpus open), not starship.

## Who paints the block

- The span (right of close gadget .. left of right-hand cluster, full bar height) is exactly the flat title band:
  `tx_span` (patina/flat.c:1860-1888) folds the system/title gadgets into bx0..bx1, `flat_title_decide`
  (patina/flat_core.c:1219-1242) sets by0=0, by1=bar_h-1, and `tx_title_draw` (patina/flat.c:1891-1911) RectFills it
  JAM2 with `band_pen` through the saved RectFill, then sets APen=text_pen, BPen=bg_pen=band_pen for Intuition's title Text.
- Inactive window (flat_core.c:1236-1239): `band_pen = flat_frame_pen(s, depth, 0)` = `flat_inactive_pen` (flat_core.c:1148-1153)
  = **pen_inbar** (depth 4..8), and `text_pen = pen_ptext` = **DK_BG = pen 0** (desk_core.c:122, desk_pen_of_role(GR_SURFACE)).
- Under vantablack pen 0 = #000000 (theme background #000000). So the title text is drawn in black by design; it is invisible
  whenever the band pen also shows black. Text pen == band colour, not text pen number == band pen number.
- Stock Intuition fill is not the block: the RectFill hook never recolours an inactive drag-bar fill (no flat_frame_pen use in
  flat.c outside c2_get_pens:411 and the title), and a stock fill would cover the bar under the gadgets too.

## Why the band pen shows black (theme data is NOT the cause)

- Vantablack has no `inactive` key (31-line file); GR_INACTIVE = gt_mix(bg #000000, fg #ffffff, GT_INACTIVE_PCT 45)
  = ~#737373 (gthemes_core.c:226/262, desk_core.c:177-182). ObtainBestPen of that on the vantablack palette gives a mid grey,
  never black. With a correct pen the inactive title would be black text on ~#737373, readable.
- pen_inbar is a SHARED ObtainBestPen pen (fp_obtain, flat.c:3652-3657), re-obtained on every desk apply and palette change:
  CTRL_D -> dim_release + pens_sync + dim_refresh -> pens_run(0) -> flat_pens_refresh_on (flat.c:3932-3940, 3723-3760,
  flat_core.c:1297-1330), and the 1 s watch pens_run(1) -> flat_pens_follow_on on a palsig change (flat_core.c:1342-1359).
  flat_pens_refresh_on releases inbar/dim/knob/sel first, then obtains inbar, knob, sel (#1a1a1a selection under vantablack),
  dim (button pen x 60%), and icon pens are synced (pens_sync) around it.
- **Nothing redraws the window frames after the pen numbers change.** No RefreshWindowFrame anywhere in flat.c (grep: 0 hits).
  The inactive lister's band pixels stay in the OLD pen_inbar number. Once released, that index is free and the next
  ObtainBestPen/allocation (sel #1a1a1a, dim ~#0f0f0f, icon pens, DOpus's own pens after dopus_theme_send) can take it and
  SetRGB it to a near-black/black colour. Result: band shows black, text (pen 0, black) vanishes into it.
- The active lister looks right because activation makes Intuition redraw its frame with the current pens; an inactive window is
  not redrawn until something refreshes its frame. Gadget boxes showing a different (grey) tone than the band fits this: they
  were redrawn (DOpus refreshes its title-bar gadgets after the theme send) while the title band was not.

Cause class: stale pixels in a released/reassigned shared pen (lifecycle bug), not stale theme data, not a role-map error.

## Same on the GUI (patina-gui) apply?

Yes, expected: the GUI pick and Apply go through the same function, gt_dos_select (patina/leaf.c:334, :546, :792) ->
gt_dos_apply_saved, so the same CTRL_D pen re-obtain with no frame refresh. Only gt_dos_save (edit save, gthemes_dos.c:370-377)
adds gt_dos_reset_ask (Workbench reset confirm); a Keep there reopens/redraws, which would hide the bug. Unconfirmed on rig.

## Proposed fix

Layer: patina-flat pen lifecycle (patina/flat.c main task + flat_core.c pure logic).
1. flat_pens_refresh_on / flat_pens_follow_on report whether any held pen NUMBER changed (inbar, knob, sel, dim) vs the previous
   slot (old numbers captured before release).
2. When it did, pens_run walks the Workbench screen's windows (LockIBase-safe list walk, main task) and calls
   RefreshWindowFrame on each, so every frame, band and gadget box is redrawn in the new pens. (Alternative, smaller but weaker:
   keep the old pen obtained until after the refresh, i.e. release after obtain, so its index cannot be reassigned meanwhile.
   Do both: obtain-new-then-release-old, then refresh frames.)
Optional hardening: inactive text pen contrast guard in flat_title_decide (pick pen_text when pen_ptext vs band contrast is
below the floor) - not the cause here, but would have kept text readable.

## Host test that would catch it

tests (flat_core host suite): fake flat_pen_ops with a 16-pen colour table where obtain allocates the lowest free pen.
Refresh once (inbar = pen A), then change the palette and refresh again so sel/dim are obtained after inbar's release.
Assert (a) the new refresh reports "pens moved" (frames must be refreshed) whenever A != new inbar, and (b) the old inbar index A
is not handed to sel/dim/knob in the same refresh (obtain-before-release). Fails on current code: release-then-obtain lets
A be reused and nothing reports the move.

## Not confirmed without the rig

- Which pen index pen_inbar held before/after the F9 apply, and what reclaimed it (sel, dim, icon pens or DOpus). Check:
  flat status line `inbar=` (flat.c:3171) before and after F9, and the colour register of the band pixel's pen index.
- The owner's "readable near-white text" on the active lister conflicts with flat's active text pen (pen 0 = black on accent
  #8d8d8d); either that window was drawn stock or the description is off. Worth a pixel read of the active title text pen.
- The "~#3A3A3C" gadget-box tone is starship's selection; under vantablack no role has that colour, so that measurement may
  come from the earlier (starship-premise) description.
