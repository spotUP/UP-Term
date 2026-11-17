---
date: 2026-11-09
topic: DOPUS-SCROLLER-LOOK, pixel measurement and root cause (s69)
status: cause proven, fix NOT implemented (mechanism choice open)
---

# DOpus lister arrows under flat: measured on rig 68

Rig 68: Kickstart 40.68 / Workbench 40.42 (OS 3.1), 800x600x24 RTG, DOpus 5.104 as Workbench,
amarchy-flat from local main 7b99fe6e (VTC copy = build/amiga/amarchy-flat, same size, 21:08/21:09),
STATUS flat=on, clone draws=43 fallback=0. ENVARC:Amarchy/flat-starting absent.

Screen DrawInfo (VTC dripens, real screen pens):
DETAIL 0, BLOCK 1, TEXT 1, SHINE 2, SHADOW 1, FILL 3, FILLTEXT 1, BACKGROUND 0.
Palette: pen0 1A1B26 (26,27,38), pen1 414868 (65,72,104), pen2 A9B1D6 (169,177,214), pen3 7AA2F7 (122,162,247).

## Measurements (ami.grab, every pixel counted, script, no eyeballing)

| Gadget (window active) | gadget type (uitree) | ground | glyph | edges |
|---|---|---|---|---|
| DOpus lister up/down arrow, 634,555 / 634,566, 18x11 | custom (buttongclass + sysiclass) | (122,162,247) pen 3 FILL, 266 px | (65,72,104) pen 1 SHADOW/TEXT, glyph part of 76 px | top/left (169,177,214) pen 2 SHINE, bottom (65,72,104) pen 1 SHADOW: full ROM bevel |
| DOpus lister left/right arrow, 602,577 / 618,577, 16x10 | custom | pen 3, 228 px | pen 1 | pen 2 / pen 1 bevel |
| MultiView window up/down arrow, 782,555, 18x11 (app window, not DOpus) | custom | pen 3 | pen 1 | pen 2 / pen 1 bevel: PIXEL-IDENTICAL to the DOpus arrow |
| DOpus lister size gadget, 634,577, 18x10 (same window) | sys:size (GTYP_SYSGADGET) | (26,27,38) pen 0, 106 px | (122,162,247) pen 3, 73 px incl. ring | 1 px accent outer ring only, no bevel = flat look |

So on the same window the flat-drawn system gadget is "light glyph on dark ground" (the PASS look), and the
arrows next to it are the plain ROM render with the screen's real pens (FILL ground of an active border
image, SHINE/SHADOW bevel). Flat never paints a bevel (flat_clone_map peels bevel rings, flat_glyph.c),
so the bevel alone proves flat did not draw these arrows. The owner's "(26,27,38) glyph" was not
reproduced: measured glyph is pen 1 (65,72,104); ground matches the report.

## Painting code path (why flat skips them)

1. sysiclass IM_DRAW enters flat's C2 hook: c2_disp_reg -> c2_draw (amarchy/flat.c:566).
2. flat.c:581: `GetAttr(SYSIA_Which, obj, &which) == 0` -- Kickstart 3.1 sysiclass does not answer OM_GET,
   so it falls to c2_which_31 (flat.c:553).
3. c2_which_31 walks w->FirstGadget and calls flat_gadget_which (amarchy/flat_core.c:350). That returns -1
   unless the gadget has GTYP_SYSGADGET (0x8000) and a GTYP_WDEPTH/WZOOM/SIZING/CLOSE type
   (flat_which_from_gtype, flat_core.c:337). DOpus lister arrows (lister_dual.c:985-1080: sysiclass image in a
   buttongclass gadget, GTYP_CUSTOMGADGET) have neither; arrows have no system gadget type at all.
4. -> NOWHICH -> c2_draw returns 0 -> c2_disp_reg calls the saved ROM dispatcher (flat.c:594) -> ROM draws
   with the real DrawInfo: FILLPEN ground, SHADOW glyph, SHINE/SHADOW bevel. That is the "inverted" look.

Why 4d339c52 changed nothing: it edits c2_clone / clone_ground, which run only after step 2-3 resolve a
Which. On OS 3.1 no app-created arrow ever gets there. This is design-doc candidate (D), not (A).
On OS 3.2 (rig 66, s66) sysiclass answers OM_GET and the lister arrows were already flat.
Same defect hits EVERY app-created border arrow on OS 3.1 (MultiView measured; WB 3.1 drawer windows and
any GadTools/BOOPSI app use the same buttongclass+sysiclass construction: inferred, not measured).

## Fix: not implemented (mechanism decision)

Violated invariant: flat must know the Which of every sysiclass image it is asked to draw; on 3.1 it only
knows it for Intuition's own system gadgets. Fix level: flat C2 hook (flat.c), not DOpus, not the clone.
Candidates (both in c2, both host-testable through flat_core):
- (1) Record at creation: the C2 hook also sees OM_NEW / OM_DISPOSE of sysiclass (same dispatcher). Store
  (object, SYSIA_Which, SYSIA_Size) from the OM_NEW taglist in a small table in flatshared; c2_which_31
  consults it before the gadget walk. Public API only. Gap: images created before amarchy-flat started are
  not in the table (on rig 68 DOpus process 12 started before amarchy-flat process 13; lister windows that
  stay open from before flat start would stay ROM-drawn until reopened).
- (2) Read Which from the 3.1 sysiclass instance data (INST_DATA offset, measured once with a probe that
  creates sysiclass objects for Which 0..13 and dumps their instance bytes), gated on intuition.library
  V40 and validated per object. Covers pre-existing objects; depends on private ROM layout.
Recommended: (1), plus (2) only if owner wants pre-existing windows covered, or ensure flat starts before
DOpus in the boot order. Regression test: host test that a custom-type gadget with a recorded UPIMAGE
image resolves to FLAT_W_UP (fails on main: flat_gadget_which returns -1), plus the rig re-measure:
DOpus arrow ground pen 0 (26,27,38), glyph pen 3 (122,162,247), no bevel = size gadget values above.

Before/after: before = table above. After = not measured (no fix built). No worktree created, no commit.

## s69 fix

- Commit 757ece1e on branch dopus-arrow-which, worktree /Users/spot/Code/UP-Term/vtcon-dopus-arrow. Not merged, not pushed.
- Change: flat's sysiclass hook records SYSIA_Which/SYSIA_Size per object at OM_NEW (32-slot table, Forbid-guarded, forgotten at OM_DISPOSE, full table falls back silently). c2_which_31 checks the table before flat_gadget_which. Images created before flat started stay ROM-drawn.
- Tests: `make test ONLY=flat_core` 2974 checks, 0 failed (new DOPUS-ARROW-WHICH block: four arrows + size, non-arrow not recorded, dispose removes one entry, full table falls back). `make amarchy-flat` rc=0. `make flat-budget` PASS (_c2_disp_reg 24, headroom 68; _c2_disp_stk 16, headroom 76).
- Revert check: host test covers the table only. The c2_which_31 lookup is device code, not host-testable. Without the three table functions the test does not compile (pre-fix = 7b99fe6e).
- Rig 68: new binary copied to build/rig68/vtc/amarchy-flat (old one backed up in session scratchpad as amarchy-flat.old). `rig.py reboot` ran, then wasabid never answered (ami.py ping times out, connect timeout). Watchdog log last line: "idle 15 min: stopping rig 68". Serial log shows shell prompt.
- After-measurement: NOT DONE (unverified). PASS target: ground pen 0 (26,27,38), glyph pen 3 (122,162,247), 1px accent ring, no bevel. IPrefs System Request text: not recorded.

## s69 main ac162ea5 rig measure

Rig 68 (stop removes build/rig68, so it was set up again, `rig.py dopus` installed DOpus 5.104, `rig.py start --purpose "s69 session: DOpus scroller arrow eye check"`; wasabid answered after ~60 s, no ping timeout this time). build/amiga/amarchy-flat built from main ac162ea5 (make amarchy-flat rc=0) is byte-identical (cmp) to build/rig68/vtc/amarchy-flat; the start log showed "refreshed amarchy-flat". ENVARC:Amarchy/flat-starting deleted. Boot order: DOpus (process 11) started BEFORE amarchy-flat (process 13), so two new Device List listers were opened via `command DeviceList NEW` after flat ran. MultiView S:Startup-Sequence opened afterwards.

Pixel sampling (script, ami.grab, gadget boxes from uitree), pens: p0 (26,27,38), p1 (65,72,104), p2 (169,177,214), p3 (122,162,247).

| Part | ground | glyph | p2 pixels |
|---|---|---|---|
| NEW DOpus lister up arrow 426,542 18x11 | p0 165 px | p3 33 px | 0 |
| NEW DOpus lister down arrow 426,553 18x11 | p0 165 px | p3 33 px | 0 |
| NEW lister size gadget 426,564 18x10 | p0 107 px | p3 73 px | 0 |
| OLD lister (opened before flat) up/down 634,555 / 634,566 | p0 165 px | p1 33 px (ROM glyph) | 0 |
| OLD lister size gadget 634,577 | p0 127 | p1 53 | 0 |
| MultiView up 782,568 / down 782,579 | p0 165 | p3 33 | 0 |
| MultiView size 782,590 | p0 107 | p3 73 | 0 |

Verdict: ground p0 and glyph p3 PASS for lister created after flat (up, down) and MultiView; no p2 highlight anywhere, no ROM bevel. The pre-flat lister keeps the ROM glyph pen p1 (expected: images created before flat started are not recorded). Ring: the right-edge column (x+17) of each arrow is p3; top/left/bottom ring rows were not confirmed in the 18x11 box (the size gadget has its p3 ring on right column and bottom row), so "1px accent ring on all sides" is UNVERIFIED for the arrows.
IPrefs System Request: none appeared on this fresh rig (uitree lists no requester, ami.requesters() empty); text not recorded because there was none.
Rig state: rig 68 running, flat on, MultiView (S:Startup-Sequence) front, two new Device List listers plus the old one behind. Watchdog stops the rig after 15 idle minutes.

## s69 arrow glyph aspect

Rig 68 restarted (watchdog had stopped it): `UPTERM_RIG=68 rig.py start --purpose "s69 session: arrow glyph aspect measure"`, up after ~58 s. Procs: DOpus 11 before amarchy-flat 13. New Device List lister (`rx "ADDRESS DOPUS.1; command DeviceList NEW"`) and MultiView S:Startup-Sequence opened after flat. Measured by script (ami.grab, pen 3 = 122,162,247 counted, no screenshots saved).

Screen: uitree `S 0 800x600 depth=24` (RTG, the DOpus-as-Workbench screen). STATUS (VTC:amarchy-flat STATUS): `side=1 looks=2` = flat_side_px(Resolution) for the Workbench screen = 1, i.e. Resolution ry <= rx (square pixels). Mode ID and the raw DisplayInfo Resolution ticks: NOT readable by any rig/ami command (STATUS prints only the derived side); ENV:Sys/screenmode.prefs bytes decode to DisplayID 0x50041303 (unverified as the live mode). c2_glyph uses imp_DrInfo->dri_Resolution; the STATUS count and the C2 count come from the same flat_side_px, so they only differ if imp_DrInfo is NULL (side 1 fallback), which cannot be seen from outside.

Glyph boxes (box w x h; glyph = pen 3 pixels, the right column x+17 on 18-wide boxes and the bottom row of the size/left/right boxes are the ring, not glyph):

| Source | box | p3 px | glyph bbox (ring excluded) |
|---|---|---|---|
| DOpus lister up (new, after flat) | 18x11 | 33 | 10x5 (cols 4..13, rows 3..7) |
| DOpus lister down | 18x11 | 33 | 10x5 |
| DOpus lister left | 16x10 | 30 | 6x5 (cols 5..10, rows 2..6) |
| DOpus lister right | 16x10 | 30 | 6x5 (cols 5..10, rows 2..6) |
| DOpus lister size (Intuition sys:size) | 18x10 | 73 | triangle 11x6 (cols 4..14, rows 2..7) |
| Pre-existing lister "Ram Disk" (arrows+size) | same | same | identical to new lister, pixel for pixel |
| MultiView up/down/left/right/size | same boxes as above | 33/33/30/30/73 | identical to the lister, pixel for pixel |
| Workbench drawer window arrows | none: DOpus replaces Workbench on the rig (OpenWorkbenchObject fails, wbrun_rig.py), so no drawer window exists |
| Intuition system gadgets on 3.1 | only depth, zoom, close, size, drag exist as system gadgets; there is NO system arrow gadget on 3.1 |

ASCII (# = pen 3, right ring column kept; identical in lister and MultiView):

```
up 18x11               down 18x11             size 18x10
.................#    .................#    .................#
.................#    .................#    .................#
.................#    .................#    ............###..#
........##.......#    ....##......##...#    ..........#####..#
.......####......#    .....###..###....#    ........#######..#
......######.....#    ......######.....#    ......#########..#
.....###..###....#    .......####......#    ....###########..#
....##......##...#    ........##.......#    ....###########..#
.................#    .................#    .................#
.................#    .................#    ##################
.................#    .................#

left 16x10             right 16x10
................       ................
................       ................
.........##.....      .....##.........
.......###......      ......###.......
.....####.......      .......####.....
.......###......      ......###.......
.........##.....      .....##.........
................       ................
................       ................
################       ################
```

Shape facts: up/down arms move 1 column per row (45 degrees on square pixels: 10 cols x 5 rows). Left/right arms move 2 columns per row (6 cols x 5 rows) and the size triangle also 2 columns per row. So within one screen, up/down are drawn in the side=1 proportion and left/right plus size in the side=2 (tall pixel, 2 columns per row) proportion. On a square-pixel screen the left/right arrows are therefore flat (wide arms), the up/down are the 45 degree reference. Verdict: DOpus and MultiView glyphs are the SAME pixels, so both use the same side value; the side=1 vs side=2 split is between arrow orientations, not between apps. Whether the up/down or left/right pair is the "wrong" one cannot be settled from pixels alone: no Workbench reference arrow exists on 3.1 to compare. The compared reference is the c2_glyph source (flat_glyph.c): check which glyph tables use g->side for columns per row (the size triangle's 2 columns per row at STATUS side=1 suggests the size/left/right tables are authored for side=2 and not scaled).

## s69 bottom scrollbar aspect

Rig 68 was running (purpose "s69 session: arrow glyph aspect measure"), flat on, STATUS side=1, screen 800x600 depth 24. Measured with ami.grab pixel dumps and the uitree (no screenshots saved). Lister = active "Ram Disk" lister W3 (209,14 443x573), MultiView = W0 (0,14 800x586).

| Part | DOpus lister | MultiView |
|---|---|---|
| right border / vertical scroller strip width | 18 (arrows x634..652) | 18 (arrows at x782) |
| bottom border / horizontal scroller strip height | 10 (y577..587) | 10 (y590) |
| vertical prop gadget | 10 wide (margin 4 each side) x 523 | 10 x 538 |
| horizontal prop gadget | 6 high (margin 2 each side) x 387 | 6 x 746 |
| knob thickness | vertical 10 px, horizontal 6 px | 10 / 6 (same) |
| arrows | up/down 18x11, left/right 16x10, size 18x10 | same boxes |

Knob ends (# = knob, . = track), DOpus lister, identical in MultiView:

```
vertical knob (10 wide), top end      bottom end
 ......######.....   row0 6 wide       ##########  
 .....########....   row1 8 wide       .########.   (8 wide)
 ....##########...   row2 10 wide      ..######..   (6 wide)
 (2 rows deep, inset 2 then 1)         (corner + 2 neighbours per corner)

horizontal knob (6 high), left end    right end
 .........####      row0 (inset 1)     ####........   (inset 1)
 ........#####      rows1-4 full       ####
 ........#####                         ...
 ........#####                         
 ........#####
 .........####      row5 (inset 1)
 (1 px cut per corner only)
```

Verdict (b) for the strip and knob THICKNESS, (a) for the knob END rounding.

1. Thickness: vertical strip 18 wide vs horizontal strip 10 high (1.8:1), knob 10 vs 6. On square pixels the strips should match. These sizes are not drawn by flat: the borders are the window's BorderRight 18 / BorderBottom 10 set by Intuition at OpenWindow, and the arrow/size boxes (18x11, 16x10, 18x10) are the 3.1 ROM sysiclass SYSISIZE_MEDRES images (flat.c:512, :546 treat MEDRES as the ROM default). MultiView, DOpus and the sys:size gadget all get the identical numbers, which a flat hook never set. DOpus puts the prop gadget in the strip with fixed margins (4 / 2). ROM MEDRES is authored for non-square (tall) pixels: 2 columns per row. The screen mode's DrawInfo resolution (side=1) is not consulted by Intuition for these sizes.
2. Knob end rounding: flat code, ignores side and orientation. flat_button_corner (amarchy/flat_glyph.c:451-466) picks `tall = h >= FLAT_ROUND_TALL (12, flat_glyph.h:87)` from the knob HEIGHT only. A vertical knob (height >> 12) gets 3 cut pixels per corner (round, 2 rows deep), a horizontal knob (height 6) always gets 1 cut pixel per corner (flat_scroller_cut_at, flat_glyph.c:489, called from flat.c:1399-1412 / :1689-1724). So the same knob rounds differently depending only on orientation: the bottom scroller looks square/boxy next to the round side one. g->side / flat_side_px is not used anywhere in the knob path (grep: flat_side_px appears only at flat.c:588 glyphs, flat.c:1448 borders, flat_core.c:1040).

Fix direction:
- (a) knob caps: choose the cap from the knob's thickness (min(kw,kh)) and the pixel side, not from h: drop the h>=12 test for a thickness test that gives the same cap on both orientations (flat_button_corner gets kw/kh/side; host test in flat_core with a horizontal 6-high knob and a vertical 10-wide knob expecting the same cut shape per axis). Cap shape alone will not equalise thickness.
- (b) thickness 18 vs 10: needs the border/box sizes changed at the source. A flat hook can only change sizes by intercepting sysiclass OM_NEW SYSIA_Size (flat.c:508-520 already sees it) for images created AFTER flat starts, and that moves arrow/size box sizes only; the window BorderRight/BorderBottom and DOpus's prop margins come from the screen (WBorRight/WBorBottom from the screen mode flags) and DOpus layout, which flat cannot change from the C2 draw hook. Likely knob: flat has to change nothing for the border; the clean option is to draw the horizontal knob/strip within the 10 px strip so its thickness equals what the vertical one shows (restricting the vertical knob inside its 18 strip to the same 6 px would need Intuition/DOpus layout) or to accept the system strip sizes and draw the vertical knob thinner than its gadget box (flat paints the prop fill, so it could paint a centred 6-wide knob in the 10 wide vertical gadget: unverified, not tried). Owner decision needed.
- Unverified: exact SYSISIZE for HIRES/LACE variants and which flag of the RTG screen picks MEDRES (no source of the ROM here).
