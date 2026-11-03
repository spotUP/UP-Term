---
date: 2026-11-03
topic: S32 scrollbar prop invisible on OS 3.2 with amarchy-flat on (build 3880f642) - code-reading hunt
status: partial - one code-proven defect in the knob pen lifecycle, final colour not measured (no rig used, per brief)
---

# Scrollbar prop hunt (code reading only, no build, no rig)

Owner report: on OS 3.2 with amarchy-flat on, the scroller arrows show, the prop gadget (track / knob) does not.

## How the prop is drawn under flat

- Flat pens make shine == shadow == fill == background == DK_BG (desk_core.c:101-110, desk_pens_flat). The STOCK 3.2
  prop render (track dither, framed knob) therefore comes out all in pen 0: without the RectFill hook the whole prop is
  invisible by construction. Arrows are drawn by the sysiclass hook (clone / block-glyph fallback), so they show.
- The only thing that colours the prop is the graphics.library RectFill patch: flat_rf_c (amarchy/flat.c:640-718),
  gated on enabled, not held, rp->Layer->Window, and the caller inside Intuition's ROM (irom_lo/hi = FindResident
  rt_MatchTag..rt_EndSkip, flat.c:729-730). scroller_pen (flat.c:593-617) classifies each fill: inside the knob
  rect -> flat_knob_pen, anything else in the container -> pen_back (flat_core.c:530-540).
- Track = pen_back = the window border pen, BY DESIGN (44a7a108 "track stays pen_back", RL6 rig 76 measured track
  000000). So the track is never visible against the border; the knob is the only visible part of a flat scroller.
  If the owner means "the track/bar is not visible", that is the current design, not a bug.
- Knob pen: flat_knob_pen (flat_core.c:446-451): pen_knob (depth 4..8, non-zero, < 2^depth), else pen_dimtext
  (DK_MUTED = pen 1, depth >= 3), else flat_button_pen. pen indices: back = DK_BG 0, dimtext = DK_MUTED 1,
  line = DK_ACCENT 3, button = DK_SELECTION 4 (desk_core.h:18, desk_core.c:115-148). By INDEX the knob never equals
  pen_back. It can only vanish by COLOUR.

## Code-proven defect: pen_knob is obtained once, at the wrong time, and never refreshed

- pen_knob (and pen_dim) are obtained only in dim_refresh (flat.c:1225-1262, ObtainBestPen on the Workbench ColorMap
  with the RGB of pen_back mixed 60 % toward pen_dimtext).
- dim_refresh runs only (a) once in main, flat.c:1395, right after the first install attempt, and (b) on Ctrl-D,
  flat.c:1411-1417.
- (a) On the installed boot path `amarchy-flat BOOT` runs from S:User-Startup BEFORE LoadWB (s32 start-look.md).
  ipd_lock_wb_screen (iconpen_dos.c:95-103) never opens a closed Workbench -> NULL -> dim_refresh does nothing,
  pen_knob stays 0.
- The S32-START-LOOK retry (flat.c:1402-1405) re-runs install_all + pens_sync when LoadWB opens the screen, but NOT
  dim_refresh.
- (b) Ctrl-D comes from desk_dos.c:512-519 only when the flat_state CHANGED (changed_only memcmp). amarchy-flat
  seeds r->st itself from the same prefs at start (flat.c:1355-1360, desk_flat_state), so the desk start apply finds
  no change and sends no Ctrl-D. Result: after a boot, pen_knob is never obtained; knob = pen 1 (DK_MUTED).
- Workbench close/reopen (screenmode change, theme reopen): the pen_tm branch (flat.c:1401-1406) never releases or
  re-obtains the knob/dim pens. g_knob_pen is an index on the OLD ColorMap; on the new screen that index has whatever
  colour the new palette gives it (can equal the surface), and ReleasePen later goes to the wrong ColorMap.
- A theme palette change with an unchanged flat_state (same pen indices) likewise sends no Ctrl-D: a pen_knob that
  ObtainBestPen matched to an EXISTING shared pen keeps that pen, which the theme apply may recolour to the surface.

## Not proven (needs one rig measurement)

Whether the owner's invisible knob is (1) the stale/never-obtained knob pen above giving a colour equal or near the
surface, (2) low contrast by design on a dark theme (rig 72 measured knob 222222 on track 000000; the 60 % mix of
000000 toward a dark muted grey is close to black), or (3) the hook not reached at all (shits not moving: caller gate
or rf_install failed -> stock render in all-pen-0).

### Measurement that settles it (owner rig, after a normal boot, one drawer with a scroller open)
1. `UP-Term:bin/amarchy-flat STATUS`: read `rectfill installed=`, `shits=` (before and after opening the drawer),
   `knob=`, `pens ... back= ...` and the `sfill` ring lines (x0 y0 x1 y1 knobx knoby knobw knobh cuts).
   - installed=no or shits not moving -> cause (3): hunt rf_install / irom gate, not the pen.
   - knob=0 -> boot-path defect confirmed (knob = pen 1).
2. GetRGB32 (or a pixel read of the screenshot) of pen 0, pen 1 and the `knob=` pen, and the pixel colour inside the
   knob rect from the sfill line vs the track. Knob colour == or ~= pen 0 colour -> cause (1); a distinct but dark
   colour (e.g. 222222 on 000000) -> cause (2), a design / contrast decision for the owner.
3. Then `amarchy-flat DOPUS` (sends Ctrl-D -> dim_refresh) and repeat 1-2: if the knob appears, cause (1) is proven.

## Proposed root-cause fix (for cause 1)

Make the knob/dim pens part of the Workbench screen watch instead of a start/Ctrl-D side effect: keep a key of the
screen they were obtained on (screen pointer, depth, RGB of pen_back and pen_dimtext); on every pen_tm tick, and after
a successful install_all retry, when the key differs from the current one: dim_release on the old ColorMap (store the
ColorMap with the pen), then dim_refresh. Decision as a pure function in flat_core (e.g.
flat_pen_refresh_due(const flat_pen_key *held, const flat_pen_key *now)), the Amiga side only reads the key.

## Proposed host regression test

tests/test_flat_core.c, next to test_boot_before_workbench_retries_install (~line 1461) and test_knob_pen (~1049):
`test_knob_pen_refreshes_with_screen`:
- held key empty (boot: no screen) + now key = screen up -> due 1 (fails on today's code: no such refresh exists);
- same screen, same RGB -> due 0;
- same screen, surface or muted RGB changed (theme palette) -> due 1;
- other screen pointer or depth (Workbench reopened) -> due 1;
- held kill switch -> due 0.
Reachability: one wired check that the pen_tm path calls it (STATUS knob= non-zero after a boot-order start), via the
existing rig drift / STATUS reading, not per assertion.
