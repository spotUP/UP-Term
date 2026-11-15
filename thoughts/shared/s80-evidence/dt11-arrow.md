# DT-11 ToolbarArrow flat hook - evidence

Answer: UNKNOWN (not finished). Test and implementation applied in the worktree
(both patch scripts printed "ok"). Host test, flat-budget, Amiga build and rig
check are pending: buildq held 10 entries from other sessions ahead of ours.

## Matcher choice and why
- DOpus source: lister_toolbar.c:202 calls
  `RenderImage(rp, GUI->toolbar_arrow_image, 0, 0, tags)` with IM_Rectangle,
  IM_ClipBoundary (0 borderless, else 2), IM_NoIconRemap; release redraw adds
  IM_State 0, IM_Erase 0. main.c:1308 opens "DOpus5:Images/ToolbarArrow.image".
- Library/images.c L_RenderImage: IM_Erase fills the whole rect with erase pen,
  clip default 2, image centred, returns 1.
- Image_Data starts with struct Node; ln_Name (offset 10) holds the open path.
- Choice: match basename of ln_Name (after last ':' or '/') ==
  "ToolbarArrow.image", case-insensitive. NULL name or no IM_Rectangle: pass
  through. Why: the only stable identity DOpus keeps for the image; pointer or
  size matching would depend on GUI struct layout or image file content.
- Still to confirm on rig: Node layout on the 5.104 vendor binary (the new
  STATUS `arrow=` counter must go above 0 when a lister opens).

## Gate data (5.104 vendor dopus5.library, md5 d688c41685282604c6e95c6f893f6d00)
- RenderImage = LVO -654 (index 108), jump target 0x10d88, prologue
  `4fef ff64 48e7 3f3e` (lea -156(sp); movem): no body-hook prologue, so LVO swap.
- flat_sum_reloc first 64 bytes: DrawBox 780fd46b, FieldBox ba2cf172 (both
  equal the rig-measured table, so the tool is validated), RenderImage f8b63678.

## Test (tests/test_flat_core.c, uncommitted in worktree)
- New test_dopus_arrow(): FLAT_EL_RENDERIMAGE==3, FLAT_LVO_RENDERIMAGE==-654,
  gate 15 with 4 sums, flipped RenderImage sum gives 7, no body prologue,
  flat_dopus_arrow_name cases, flat_glyph_dleft goldens (13x9 side1: 14 rects
  "<<"; side2: 6 rects; w=4 / h=4 / NULL fill: 0). Status-line expectations
  updated to 4 sums.
- Fail-before: by construction. Before the patches, grep found none of
  FLAT_EL_RENDERIMAGE, FLAT_LVO_RENDERIMAGE, flat_dopus_arrow_name,
  flat_glyph_dleft in patina/, so the new test cannot compile at HEAD. The
  queued fail-before build never ran (buildq entry gone, log holds only WAIT).
- Hand check of flat_glyph_dleft against the goldens: 13x9 side 1 gives 14
  rects, side 2 gives 6.
- Pass-after: see "Results" below.

## Implementation (dt11-patches/, applied)
- patch_core.py: flat_core.h/.c enum + LVO + 5.104 sum[3] + name matcher +
  4-sum status; flat_glyph.h/.c flat_glyph_dleft.
- patch_flat.py: flat_shared.h `darrow`; flat.c d4_tag walk, d4_arrow (erase
  rect with erase pen, draw glyph in st->pen_text, restore pen), d4_renderimage
  reg wrapper calling saved LVO otherwise, LVO swap for el 3, STATUS `arrow=`.
- sum.c: scratch tool for flat_sum_reloc.

## Remaining
1. Wait for fail-before log, record errors, run both patch scripts, rebuild
   host test via buildq, run flat_core suite.
2. make flat-budget, then buildq make amiga; record md5.
3. Rig (not 7/81), purpose "up-term-e8: DT-11 arrow check": STATUS sums show
   f8b63678 and arrow>0; read arrow pixels; switch theme once; re-read; stop rig.

## Known limits (accepted risks)
- The hook ignores IM_State: the pressed arrow draws the same glyph.
- Without IM_Erase the inset rect is filled with pen 0 (stock image is opaque,
  so the area is fully covered either way).
- Match is by name only: a user theme that replaces ToolbarArrow.image under
  the same name also draws flat.

## Results
- Host test (make test-one SUITE=flat_core): PENDING
- make flat-budget: PENDING
- make amiga, patina-flat md5: PENDING
- Rig: PENDING. Pass = STATUS sums contain f8b63678, patched=15, arrow>0.
  Pixel read: eye check owed (no nested image agent allowed).

## Remaining (in order)
1. buildq: `make test-one SUITE=flat_core` (nice 19). Fix failures, re-run only flat_core.
2. buildq: `make worktree-ready`, `make flat-budget`, `make -j2 amiga`; record patina-flat md5.
3. Rig (free, not 7/81), purpose "up-term-e8: DT-11 arrow check": install, open a
   DOpus lister, read STATUS (pass criteria above), eye check of the arrow, stop rig.
4. Fill Results, drop "WIP:" from the commit subject (amend or follow-up commit).
Stopped because: tool budget hit while buildq was blocked (10 entries from
other sessions ahead; running Selecta build at 0% CPU for 35+ min). Our queue
entries were withdrawn.

## Cleanup
- No rig started. Scratch: scratchpad/dt11 (small, under 1 MB).

## S82 rig check
Verdict: FAIL (arrow counter stayed 0). Rig 83, purpose "up-term-e8: DT-11 arrow rig check", DOpus 5.104 booted as Workbench,
build md5 of patina-flat 9076dbb6fdb61d57ab0805472f8f05c4 installed (rig.py install, kit shadow copy at boot), Patina running.
Gate half PASSES: sums contain f8b63678 and patched=15. Three listers open (DeviceList NEW, lister new; default toolbar
PROGDIR:Buttons/Toolbar), one resized and fully refreshed: arrow stayed 0, no crash, no guru.
STATUS line (final):
  dopus=73.8 patched=15 calls=122 flat=108 sums=780fd46b,ba2cf172,bdaa116a,f8b63678
  dopus bevel calls=0 face=108 nopix=0 d5=24 arrow=0 ground=50 gs=1 gslot=0x1000c458 gcode=0x10828dbc selrgb=3e4254 pensel=12
Screenshot (deleted): listers show a single "<" and a down arrow in the title row (SRCE/DEST), no "<<" glyph at the toolbar.
Open: no trigger found that makes DOpus call RenderImage with ToolbarArrow.image on this screen; either the default toolbar
never draws the arrow here (maybe only for an overflowing/multi-page toolbar) or the name match (ln_Name offset 10) does not
hit. Next: log RenderImage names seen by d4_renderimage (counter of all calls, last name) to tell which.
