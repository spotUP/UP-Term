---
date: 2026-11-01
topic: Amarchy flat look - flat FILLED icon set instead of Lucide line icons (source swap, theme-pen model unchanged)
status: draft (research done; owner picks the set, section 3 question)
ledger: thoughts/spot/todos.md AM-ICONS-FILLED-SET (s28)
related: AM-LUCIDE-ICONS (thoughts/shared/plans/2026-10-31-amarchy-lucide-icons.md), AM-ICON-NOBORDER
---

# Amarchy filled icon set (AM-ICONS-FILLED-SET)

## Goal and finish line

Same product as AM-LUCIDE-ICONS (every Workbench `.info` image replaced, colours from ~16 reserved
screen pens that follow the theme live, works on 3.1 and 3.2, deep screens themed, shallow screens
fold to shared pens, flat off restores the original bytes), but the glyphs are solid filled shapes
instead of 2 px strokes. Nothing about the on-Amiga side changes except the data file. Binding
decisions of AM-LUCIDE-ICONS stay: only image data is replaced, tooltypes, default tool, stack,
position and icon type are kept.

Done means: the table `dist/lucide-icons` (renamed, F3) builds from the chosen filled set with zero
unknown names, the host tests of section 5 are green, the reachability test of section 6 passes on a
3.2 rig and a 3.1 rig, and every row of section 4 has its proof.

## 1. Pipeline today (file:line)

- Source is an icon FONT, not SVG. `Makefile:401-425` fetches `lucide-static` 1.66.0 from the npm
  registry (SHA-256 pinned), extracts `lucide.ttf` + `codepoints.json` into `build/lucide/`;
  `$(LUCIDE_DAT)` runs `python3 tools/gen_lucide_icons.py dist/lucide-icons lucide.ttf lucide.dat`.
- Table parse: `tools/gen_lucide_icons.py:44-67` (`match = name`, unknown name aborts, 81 rows, 51
  distinct names today). Name -> code point from `codepoints.json` (`:115-124`).
- Rasteriser: pure python, no SVG library. `tools/gen_menu_icons.py:68-105` flattens the glyph
  outline with fontTools, scan-converts with non-zero winding at RES=64 x SUB=4 (`:26-28`, 4 percent
  blank edge, glyph fitted and centred by its bounding box), giving a 64x64 coverage raster.
  `gen_emoji.area` averages it down to the icon size (`gen_lucide_icons.py:77-81`).
- Sizes: `VARIANTS = ((32,32),(32,16))` (`:23`), square and tall (hires non-laced) variants, depth 6
  (`DEPTH`, PlanePick 0x3F).
- Pens: coverage is quantised to 4 levels (`level()` `:70-74`: transparent under 20 percent, then
  33/66/100) and each level maps to one pen: normal image uses 0 (transparent), 32 (FG 33), 37 (FG 66),
  33 (FG 100); selected image uses 42/38/34 plus pen 43 as the SELBG plate (`:30-34`, `images()`
  `:94-104`). So an icon is ONE tone with antialiasing steps. Pen roles 35, 36, 39, 40, 41, 45 (ACCENT,
  MUTED) and spares 44, 46, 47 are reserved by the pen plan but unused by the glyph images.
- Planar pack: `mkicon.planes` (`tools/mkicon.py`), then `pack()` (`:127-143`) writes `lucide.dat`
  (header `LUCI`, version 1, name index, 2 images x 2 sizes per icon). The Amiga side reads it with
  `li_dat_open` (`amarchy/iconfile_core.h:96`, `amarchy/icons.c:190`), path
  `UP-Term:Amarchy/Icons/lucide.dat` (`amarchy/iconapply_core.h:21`).

### What changes for filled glyphs

- A filled glyph is mostly level 3 (solid FG), antialias only on its rim, so the 4-level quantiser
  already handles it. No SVG rasteriser is needed if the set ships a font (Phosphor, Material Symbols,
  Remix, Font Awesome, MDI all do). Cutouts inside a filled glyph (the hole of a gear, the slot of a
  drive) come out as transparent: non-zero winding with the font's reverse-wound inner contour, no
  code change.
- Two tones ARE possible with the pens that already exist, in two ways:
  1. Cutout tone: a flood fill from the 64x64 border marks background; enclosed transparent regions
     are "detail" and get a second pen (for example MUTED 33 percent, pen 40) instead of showing the
     window background. This gives a fill pen plus a detail pen from any filled font, no new pens.
  2. Phosphor only: the Duotone weight has a second layer at 20 percent opacity under the line shape.
     That is a line-art set with a tint, not a filled set; not used.
  Recommendation: ship single tone first (F4), add the cutout tone as a switch (F7) only if the
  owner finds the holes too empty on the rig. The pen plan needs no change for tone 2 because pens
  40/41/45 are already obtained.
- Quantiser thresholds (`FIRST_LEVEL` 0.20, `LEVELS` 0.50/0.835) were tuned for 2 px strokes. Filled
  glyphs put more pixels at coverage near 1, so the ramp should be re-tuned on the rig (F5), nothing
  structural.
- Minimum feature size: at 32 px a 256-grid glyph has 1 unit = 0.125 px; Phosphor Fill cutouts and
  gaps are at least 12 units (1.5 px) wide, the 32x16 tall variant halves the vertical resolution.
  Thin cutouts (the keyboard keys, the printer paper) may close up on the tall variant; check in F5.

## 2. Candidate sets compared

All numbers measured on 2026-11-01 from the npm tarballs listed in the Source column (package file
lists, not the vendors' marketing counts). "Filled icons" = files of the filled style. "Hits" = of the
51 distinct names in `dist/lucide-icons`, how many have a filled equivalent in that set: exact (same
object) + substitute (nearest object) / total; "miss" = nothing usable.

| Set | Licence (ships in a public repo?) | Filled icons | Exact + substitute | Miss | Font file for the current pipeline | Source |
|---|---|---|---|---|---|---|
| Phosphor, Fill weight | MIT, yes | 1512 | 44 + 7 = 51 | 0 | yes, `Phosphor-Fill.ttf` + `style.css` code points in `@phosphor-icons/web` 2.1.2 (icons from `@phosphor-icons/core` 2.1.1) | https://registry.npmjs.org/@phosphor-icons/core/-/core-2.1.1.tgz ; https://registry.npmjs.org/@phosphor-icons/web/-/web-2.1.2.tgz ; https://phosphoricons.com |
| Material Symbols, Fill=1 | Apache-2.0, yes (NOTICE required) | 3937 | 43 + 8 = 51 | 0 | variable font; static SVGs (`-fill` files) in `@material-symbols/svg-400` 0.48.2; font needs instancing at FILL=1 | https://registry.npmjs.org/@material-symbols/svg-400/-/svg-400-0.48.2.tgz ; https://fonts.google.com/icons |
| Material Design Icons (Pictogrammers, extra candidate) | Apache-2.0 (brand icons excluded), yes | 7447 (mostly solid, `-outline` variants separate) | 48 + 3 = 51 | 0 | yes, `@mdi/font`; icon data in `@iconify-json/mdi` 1.2.3 | https://registry.npmjs.org/@iconify-json/mdi/-/mdi-1.2.3.tgz ; https://pictogrammers.com/library/mdi/ |
| Font Awesome Free, Solid | CC BY 4.0 icons + OFL 1.1 fonts + MIT code; yes, attribution required | 2001 | 43 + 7 = 50 | 1 (usb) | yes, `fa-solid-900.ttf` | https://registry.npmjs.org/@fortawesome/fontawesome-free/-/fontawesome-free-7.3.1.tgz ; https://fontawesome.com/license/free |
| Remix Icon, -fill | Remix Icon License v1.0 (Jan 2026; custom, npm metadata still says Apache-2.0): free use and redistribution as part of a larger product, standalone icon-pack redistribution forbidden | 1539 | 41 + 9 = 50 | 1 (type) | yes, `remixicon.ttf` | https://registry.npmjs.org/remixicon/-/remixicon-4.9.1.tgz ; https://remixicon.com |
| Tabler, filled | MIT, yes | 1054 | 26 + 8 = 34 | 17 | font has a filled set but only 1054 glyphs | https://registry.npmjs.org/@tabler/icons/-/icons-3.50.0.tgz ; https://tabler.io/icons |

Misses by set (names from `dist/lucide-icons`):
- Tabler (17): cable, cpu, file-spreadsheet, film, hammer, hard-drive, languages, memory-stick, plug,
  printer, rocket, shapes, square-terminal, terminal, toolbox, type, wrench. Ruled out: the filled
  set has no tools, terminals, hardware or media, exactly what Workbench needs.
- Font Awesome: usb (nearest `plug`). Remix: type (no font or text glyph in the -fill list).
- Phosphor, Material Symbols, MDI: none; their 7 / 8 / 3 substitutes are listed per set below for
  Phosphor, and the same objects are the weak ones in the other two (monitor-cog, notebook-pen,
  cable, hard-drive-download).

Not tried (not filled, or no license fit): Lucide filled is not offered upstream; Heroicons Solid
(MIT, only ~300 icons, no terminal/hardware set), Ionicons filled (MIT, ~1300) and Carbon were not
counted because Phosphor already covers 51 of 51.

## 3. Recommendation and decision

Recommend Phosphor Fill.

- Coverage: 51 of 51 with 0 misses, 44 exact. Only 7 are substitutes, and 6 of those 7 differ by a
  detail (an arrow, a cog, a pen) that a 32 px icon cannot show anyway.
- Licence: MIT, no attribution file needed beyond the notice; Remix has a custom no-standalone-pack
  clause, Font Awesome needs CC BY 4.0 attribution, Material Symbols and MDI need Apache NOTICE.
  Same ship path as Lucide (`dist/lucide/LICENSE`).
- Pipeline: ships a font plus a code point table, so `gen_menu_icons.raster` is reused unchanged
  (section 1). Material Symbols would need the variable font instanced at FILL=1 (extra tool) or an
  SVG rasteriser; Font Awesome and Remix would work as well.
- Look at Workbench size: Phosphor Fill is built from rounded, chunky shapes on a 256 grid with
  cutouts of 12+ units, which survive an average to 32 px and to 32x16; it is a single weight
  family, so mixed drawers (disk, tool, project, prefs) look uniform. MDI is 24 grid with thinner
  cutouts (about 1 px at 32 px) and mixed community authorship; Material Symbols has the best
  small-size hinting but a generic, squarer look; Font Awesome Solid is the most legible but heavy.
- Headroom: 1512 fill icons leave room for type fallbacks and more named drawers later.

Owner decision (the only open one): Phosphor Fill (recommended) or MDI (same 51 of 51, 48 exact,
Apache-2.0, bigger catalogue, thinner details). The look at 32 px is checked on the rig in F5 before
the table is final; the choice is reversible there because only the table names and the source
change.

Other choices made here (veto if wrong): single tone first, cutout tone as an option (F7); keep the
`LUCI` container and bump its version rather than rename the file (F6); keep sizes 32x32 and 32x16.

## 4. Pipeline changes as phased steps (checklist, 12 rows)

- F1 Pin the Phosphor font in the Makefile (`@phosphor-icons/web` 2.1.2, `Phosphor-Fill.ttf`,
  `style.css`) next to the Lucide block (`Makefile:401-425`) with SHA-256 for tarball, font and CSS;
  extract the code points from `style.css` (`.ph-fill.ph-NAME:before { content: "\eXXX" }`, 1530
  entries seen) into `build/phosphor/codepoints.json`, in the same name -> code point shape the
  generator already reads. Done: `make phosphor` produces both files twice-identical.
- F2 Licence and credit: `dist/phosphor/LICENSE` (MIT), `SOURCE.txt` in the style of
  `dist/lucide/SOURCE.txt`, README credit, `dist/LICENSES.txt` row, kit copy next to
  `Makefile:1713-1714`, manifest entry in `tests/test_dist_manifest.py`. Remove the Lucide lines
  from kit and manifest in the same commit. Done: `make dist-check` green.
- F3 Table: write the name mapping of section 7 into `dist/lucide-icons` (the row keys `match =`
  stay; right-hand names change from Lucide to Phosphor names; git mv the file to
  `dist/icon-set` only if the owner wants, otherwise keep the name and fix the header comment).
  Done: generator accepts it, 81 rows, 51 names, no unknown name.
- F4 Generator `tools/gen_lucide_icons.py`: take the font and code point file as arguments (already
  the case), change the strings that say Lucide, no raster change. Done: `lucide.dat` builds; the
  existing test file `tests/test_gen_lucide_icons.py` passes with the Phosphor font.
- F5 Tune the quantiser (`FIRST_LEVEL`, `LEVELS`) against filled glyphs and look at all 51 at
  32x32 and 32x16 with `tools/preview_lucide_icons.py`; check thin cutouts on the tall variant.
  Done: preview sheet in `thoughts/shared/research/s28-evidence/`, owner look recorded.
- F6 Container: bump `VERSION` in `pack()` to 2 only if the record layout changes (it does not in
  the single-tone case), so the Amiga reader and the files stay compatible. If any layout
  change is made, `li_dat_open` (`iconfile_core`) and its test move together. Done: reader test
  green, old and new dat each load or are refused on purpose.
- F7 Optional second tone: in `images()` classify enclosed transparent regions (flood fill from the
  border on the 64x64 raster) and give them the detail pen (MUTED 33 percent = pen 40 for normal, a
  SELFG 33 pen for selected). Behind a table switch (`; tone2 = on` or a per-row flag), default off.
  Done: unit test on a glyph with a hole (gear): hole pixels carry pen 40 only when on.
- F8 Rename text only: user-visible strings and comments "Lucide" -> "icon set" in
  `amarchy/icons.c`, `amarchy/iconapply_core.h`, the installer text (`dist/install.dos`,
  `dist/parts.txt`), `tests/test_dist_installer.py`. File name `lucide.dat` is kept (no on-disk
  migration); decision recorded in the ledger. Done: grep for "Lucide" outside the history shows
  only the credit and the old plan.
- F9 Regenerate `dist/Amarchy/Icons/lucide.dat`; the dist build copies it. Done: kit contains the
  new dat, md5 recorded.
- F10 Host suite: all of section 5 green via `make test ONLY=...`.
- F11 Reachability test (section 6) on a 3.2 rig and a 3.1 rig. Done: report under
  `thoughts/shared/research/s28-evidence/`.
- F12 Owner screen check of the Workbench drawers (filled look) and ledger tick. Done: owner OK.

## 5. Host tests

- `tests/test_gen_lucide_icons.py` (ONLY=gen_lucide_icons): table parse with Phosphor names, unknown
  name fails, quantise levels, plate, planar round trip, 32x16 variant, dat layout; the font tests use
  the pinned `build/phosphor/` files (skipped without them, REQUIRE=1 fails instead).
- New pin test: the 51 names of `dist/lucide-icons` all resolve in the Phosphor code point file
  (guards against a font bump dropping a name); the substitute rows are listed in the test so a
  silent swap shows.
- New tone test (F7): hole pixel pen with the switch on and off.
- Unchanged and rerun: `tests/test_iconfile.c` (ONLY=iconfile: parse, splice, NewIcon/FORM ICON strip,
  restore byte for byte on every `.info` of the rig trees), `tests/test_iconpen.c`,
  `tests/test_iconapply.c`, `tests/test_dist_manifest.py`.
- Pen check: every pen index the generator can emit is in 32..47 or 0; assert against the role table
  (so no stray index lands on a screen pen the theme does not own).

## 6. Rig check (one reachability test, real entry point)

Reuse `tests/rig/test_lucide_reach.py` and `tools/rig/amarchy_icons_restore_rig.py`; nothing new is
written for the set swap beyond the expected images.

1. Start a rig: `UPTERM_RIG=<n> python3 tools/rig/rig.py start --max --os 32 --purpose "Amarchy s28: filled icons 3.2 reach"` (3.1: no `--os`, purpose "... 3.1 reach").
2. Flat on through the Amarchy menu path. Assert `amarchy-icons STATUS` shows `written > 0` and
   `pens-held = 16` (sentinel that the new code ran), `ENVARC:Sys/def_drawer.info` image bytes equal
   the generator output for Phosphor `folder-fill` (this is the number the spec states), and a pixel
   read at the RAM: icon shows a pen in 33..47.
3. Theme switch: pen 33 RGB equals `ip_colours` of the new theme, `.info` file dates unchanged (colours
   follow the theme with no rewrite).
4. Flat off: file equals the original, see section 7a. Read every on-screen dialog text and quote it.
5. Stop the rig when done. A1200 only last, as an owner check.

## 7. Name mapping, Phosphor Fill (51 of 51)

Phosphor icon names are the `-fill` file stems (`NAME-fill.svg`, font class `ph-fill ph-NAME`).
Substitutes (7 rows) are the nearest object; the note says what is lost.

| Lucide name | Phosphor Fill name | Kind | Note |
|---|---|---|---|
| app-window | app-window | exact |  |
| archive | archive | exact |  |
| book-open | book-open | exact |  |
| cable | plugs | substitute | plugs = two plugs; no cable glyph |
| calculator | calculator | exact |  |
| clock | clock | exact |  |
| cog | gear-six | exact |  |
| copy | copy | exact |  |
| cpu | cpu | exact |  |
| disc | disc | exact |  |
| disc-3 | vinyl-record | exact | vinyl-record; disc is used for disc |
| eye | eye | exact |  |
| file | file | exact |  |
| file-code | file-code | exact |  |
| file-spreadsheet | file-xls | exact |  |
| file-text | file-text | exact |  |
| film | film-strip | exact |  |
| folder | folder | exact |  |
| hammer | hammer | exact |  |
| hard-drive | hard-drive | exact |  |
| hard-drive-download | hard-drives | substitute | no download variant; plain drives, arrow dropped |
| image | image | exact |  |
| key-round | key | exact |  |
| keyboard | keyboard | exact |  |
| languages | translate | exact |  |
| layout-dashboard | squares-four | substitute | four squares, not the 1+3 dashboard split |
| layout-grid | grid-four | exact |  |
| memory-stick | memory | substitute | RAM chip; same object, different drawing |
| menu | list | substitute | list bars; Phosphor has no hamburger-only name |
| monitor | monitor | exact |  |
| monitor-cog | monitor | substitute | plain monitor, cog dropped |
| mouse-pointer-2 | cursor | exact |  |
| music | music-notes | exact |  |
| notebook-pen | notebook | substitute | notebook without the pen |
| palette | palette | exact |  |
| plug | plug | exact |  |
| printer | printer | exact |  |
| puzzle | puzzle-piece | exact |  |
| rocket | rocket | exact |  |
| scroll-text | scroll | exact |  |
| settings | gear | exact |  |
| shapes | shapes | exact |  |
| sliders-horizontal | sliders-horizontal | exact |  |
| square-terminal | terminal-window | exact |  |
| terminal | terminal | exact |  |
| toolbox | toolbox | exact |  |
| trash-2 | trash | exact |  |
| type | text-aa | exact |  |
| usb | usb | exact |  |
| volume-2 | speaker-high | exact |  |
| wrench | wrench | exact |  |

Other sets' mappings for the same 51 names (for a change of choice) are reproducible with the
verification script logic recorded in section 2; the MDI row set differs mainly in: archive,
cog = `cog`, hard-drive = `harddisk`, layout-dashboard = `view-dashboard`, layout-grid = `view-grid`,
file-text = `file-document`, trash-2 = `trash-can`, terminal = `console-line`, cable = `ethernet-cable`.

## 7a. RESTORE byte-for-byte guarantee (kept)

The set swap changes only the bytes inside `lucide.dat`. The guarantee lives elsewhere and is not
touched: `amarchy/iconfile_core.c` rewrites the image data of a `.info` and stores the original
bytes in the backup (`SYS:Storage/Icons/Amarchy`) with the manifest line written by
`amarchy/iconapply_core.c`; RESTORE splices the stored original back. Proofs that must stay green and
are rerun as part of F10/F11:
- host: the restore splice test in `tests/test_iconfile.c` over every `.info` of `build/rig/os-31` and
  `build/rig/os-322` (267 files) returns original bytes;
- rig: `tools/rig/amarchy_icons_restore_rig.py` APPLY then RESTORE with md5 of every `.info` on all
  volumes (last recorded 235 of 235 identical, commit a25d9ef3); run it once with the Phosphor dat and
  keep the count in the F11 report. Anything not identical is a failed plan, not a note.
- A glyph the new set draws larger or smaller never changes the stored original: nothing in RESTORE
  reads `lucide.dat`.

## 7b. Pen hold when flat is off (coordinator input, hunt measurement)

Measured by the hunt: the glyph pens 32 / 37 / 33 (`tools/gen_lucide_icons.py:38`) receive theme
colours only while `amarchy-flat` holds them (`amarchy/flat.c:852-881`, `ObtainPen` PEN_EXCLUSIVE).
Without `amarchy-flat` they keep the screen's default palette (ffffff / ffffff / 888888), so
planes that draw "foreground" in pen 33 render grey on white and the icons look inverted. A filled
set makes this worse than Lucide: the solid body is level 3 (pen 33), so the whole glyph is the
wrong colour, not only its 2 px stroke. Rasterising is unchanged (`:77-104`, 4 levels; selected
image draws pen 43 on a rounded square, `:102`), so the fix is about WHO owns the pens, not the
raster.

Invariant: an icon image that names pens 32..47 is only on disk while some process holds those pens
with theme colours.

Options for where the hold lives:
- H1 (recommended): the pen hold moves out of `amarchy-flat` into the icon applier's own long-lived
  holder (`amarchy/iconpen_core.c`, the `ip_colours` table is already there). Flat and icons each take
  a reference; the pens are released when the last reference goes. APPLY starts the holder, RESTORE
  stops it after the originals are spliced back. Icons are themed whether flat is on or off, and
  flat off keeps the icons. Cost: a holder process or a shared library-level refcount, plus a
  handoff test that flat can start and stop while icons stay themed.
- H2: icons require flat. APPLY refuses when `amarchy-flat` is not running, and flat off runs RESTORE
  first. No new process, but the icons disappear whenever the owner turns flat off, which is the
  state the hunt found inverted today.
- H3: write shared-pen indices (the fold table used on shallow screens) into the dat when flat is off.
  Rejected: needs a second image set and a rewrite on every flat toggle, against the binding
  decision "no file rewrite on theme change".

Plan rows (added to section 4):
- F13 Decide H1 or H2 (owner, below). Done: decision in the ledger row.
- F14 (H1) Move the `ObtainPen` block of `amarchy/flat.c:852-881` behind a refcounted holder in
  `iconpen_core`; flat calls acquire/release; the icon applier acquires on APPLY and releases on
  RESTORE. Done: host test of acquire/release counts and `ip_colours` application.
- F15 Rig: with flat OFF, APPLY, read pen 33 / 37 / 32 RGB (`amarchy-icons STATUS` pens-held 16) and
  assert they equal the theme's `ip_colours`, not ffffff / ffffff / 888888; toggle flat on and off and
  re-read; RESTORE and assert `.info` md5 identical and pens released. This is part of the one
  reachability test of section 6, as an extra step, not a new test.
- RESTORE guarantee: unchanged. The holder only owns pens; it never reads or writes `.info` bytes.

Owner decision: H1 (recommended: icons stay themed with flat off) or H2 (icons only with flat).

## Risks

- Phosphor Fill cutouts may close on the 32x16 variant (F5 look).
- Seven substitute icons are visibly not the Lucide object; the owner may prefer a nearer MDI glyph for
  one or two (cable, notebook-pen, monitor-cog). Single-row table change.
- The `@phosphor-icons/web` CSS format could change on a bump; the pin test (section 5) fails first.
- Materials in `thoughts/shared/research/s28-evidence/` for the look are added in F5, not here.
