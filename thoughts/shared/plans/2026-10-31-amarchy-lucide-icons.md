---
date: 2026-10-31
topic: Amarchy flat look - Lucide icon set for every Workbench icon, colours follow the theme live
status: ready (owner answers 2026-10-31 folded in; Q5 re-check open)
ledger: thoughts/spot/todos.md AM-LUCIDE-ICONS (s28)
related: FLAT-BACKDROP-SAME-AS-WINDOW, AM-ICON-NOBORDER, RIG9-FLAT-GAP
---

# Amarchy Lucide icons (AM-LUCIDE-ICONS)

## Goal and finish line

With the flat look on, every icon the Workbench shows is a Lucide line icon: the
Workbench defaults (disk, drawer, tool, project, trashcan, RAM), SYS:System,
SYS:Utilities, SYS:Prefs, the Amarchy programs, and any other `.info` on any
writable volume through a type fallback. Only the image data changes. Tooltypes,
default tool, stack, position, drawer data and icon type stay byte for byte (one
owner question below about NewIcon tooltypes). The icon colours come from ~16
reserved screen pens that Amarchy sets per theme, so a theme switch recolours
every icon at once with no file rewrite. Flat off restores the original images.
Works on OS 3.1 and 3.2.

Done means: the reachability test (section 6) passes on a 3.2 rig and a 3.1
rig, the rig report (section 7) is written, and every checklist row below has
its proof (commit or report path).

## 1. Reuse (searched first)

| Need | Reused | Where |
|------|--------|-------|
| Vector glyph to coverage raster | `Flatten` pen (curves to lines), `raster()` RES=64 SUB=4 EDGE=0.04 | tools/gen_menu_icons.py |
| Coverage averaging to icon size, page packing | `glyph_bytes`, `page_bytes` | tools/gen_emoji.py |
| Icon id table format `id = name` | dist/menu-icons pattern | dist/menu-icons |
| Pinned font fetch with SHA-256 | Makefile `SYMBOLS_TTF` rule (line 11, 337, 371) | Makefile |
| Aspect fit on non-square pixels | `ic_fit`, `mi_width` / `mg_cols` | amarchy/icon_core.c, amarchy/menu_icon.c |
| Planar packer, DiskObject layout | `planes(art, depth)`, header pack | tools/mkicon.py |
| Atomic file replace (tmp, Close check, rename) | write path ~325-336 | amarchy/icon_gfx.c, amarchy/index.c |
| Theme colours and live CLUT write | `wb_pens_apply` (SetRGB32, x0x01010101), `gt_scale`, gtheme fields | amarchy/gthemes_dos.c |
| Theme switch and preview call sites | `gt_dos_select`, `pv_set` / `desk_preview_theme` | amarchy/gthemes_dos.c |
| Flat on/off apply, pen sync, start of resident flat | `desk_dos_apply`, `dk_flat_pens`, `dk_flat_sync` (`amarchy-flat BOOT` via `pl_start`) | amarchy/desk_dos.c |
| Flat pen roles | `desk_pens_for(flat, pens[DK_SLOTS])`, `desk_font_pens` | amarchy/desk_core.c |
| Resident holder with semaphore and STATUS | `amarchy-flat` (FLAT_SEM_NAME "Amarchy.Flat") | amarchy/flat.c |
| Workbench close/open notification | screennotify.library use from acd33353 | handler |
| CLI with `__stack` + `$STACK` cookie | install/upicon.c pattern | install/upicon.c |
| Flat config keys | `AM_FLAT`, `AM_FLATGAD` | amarchy/amconf_core.h |

Nothing in the tree parses or splices a `.info` file in place (upicon.c uses
GetDiskObject/PutDiskObject, which rewrites the whole file and drops unknown
chunks). The splicer in L4 is the one new mechanism; it is a pure function and
host-tested.

## 2. Host build pipeline

- Source: the `lucide-static` npm package icon font (`lucide.ttf`, glyph names
  equal icon names). Pin one version, fetch in the Makefile next to
  `SYMBOLS_TTF` with a SHA-256 check into `build/lucide/`. Same pipeline as the
  menu icons, so no SVG renderer dependency.
- Licence: Lucide is ISC; it contains Feather icons under MIT. Ship both notices
  in `dist/licences/lucide.txt`, credit Lucide in README and in the Amarchy docs,
  list the file in the dist manifest (`tests/test_dist_manifest.py`).
- Table `dist/lucide-icons`: one row per target.
  `match = lucide-name` where match is a path (`SYS:Prefs/Font`), a default
  (`def_disk`, `def_drawer`, `def_tool`, `def_project`, `def_trashcan`,
  `def_RAM`, every `def_*` present in os-322 `Prefs/Env-Archive/Sys`), or a
  type fallback (`type:disk`, `type:drawer`, `type:tool`, `type:project`,
  `type:garbage`, `type:kick`, `type:device`). Unknown Lucide name fails the
  build.
- Generator `tools/gen_lucide_icons.py` (imports gen_menu_icons and gen_emoji,
  no copy): glyph -> coverage at 64 with SUB=4 -> averaged to icon size ->
  quantised to 4 levels (0 transparent under 20%, then 33/66/100) -> pen index
  per role (section 3) -> planar via mkicon `planes()`.
- Two images per icon: normal (glyph tones on pen 0) and selected (glyph in
  SELFG on a SELBG rounded square, so the antialias edge has a known
  background).
- Sizes: 32x32 for square-pixel modes and 32x16 for hires non-laced
  (pixel aspect 2:1, `ic_fit` rule). The applier picks by the Workbench screen
  aspect at apply time and re-applies when the mode changes aspect.
- Depth 6, PlanePick 0x3F. Output `dist/Amarchy/Icons/lucide.dat`: a header, the
  name index, then per icon two images per size (planar bytes). Built by
  `make`, listed in the dist manifest, installed to `UP-Term:Amarchy/Icons/`.

## 3. Pen reservation

- Range: 16 fixed pens 32..47 on the Workbench screen. Slot = 32 + index, and
  the low 3 bits are chosen so a fold to a shallow screen lands on a sensible
  shared pen. Draft role table (L5 pins it in a host test):

  | Pen | Role | Fold (depth 2/3) |
  |-----|------|------------------|
  | 32 | FG 33% | 0 |
  | 33 | FG 100% | 1 (text) |
  | 34 | SELFG 100% | 2 (shine) |
  | 35 | ACCENT 100% | 3 (fill) |
  | 36 | ACCENT 33% | 4 |
  | 37 | FG 66% | 5 |
  | 38 | SELFG 66% | 6 |
  | 39 | ACCENT 66% | 7 |
  | 40 | MUTED 33% | 8 |
  | 41 | MUTED 100% | 9 |
  | 42 | SELFG 33% | 10 |
  | 43 | SELBG | 11 |
  | 44, 46, 47 | spare | 12, 14, 15 |
  | 45 | MUTED 66% | 13 |

  Pens 4..7 fold roles are checked against `desk_pens_for(flat)` in L5 so a
  depth-3 fold does not land on the window title pen.
- Depth rules: depth 2 uses `value & 3`, depth 3 `& 7`, depth 4 and 5 map
  32..39 to 0..7 and 40..47 to 8..15 (Amarchy sets 8..15 itself on those
  depths). Depth 6 EHB: pens 32..63 are half-bright copies of 0..31, so fold
  as depth 5. Depth 6 non-EHB, 7 and 8: full themed range.
- Obtain: `ObtainPen(cm, n, r, g, b, PEN_EXCLUSIVE)` for each pen at boot by the
  resident holder (`amarchy-flat`, which already lives for the whole session).
  Not ObtainBestPen: we need fixed numbers because the files store indices.
  If a pen is taken, the holder logs it in STATUS and the applier uses the
  fold table for that depth (icons stay readable, not themed).
- Re-obtain after a Workbench screen close/open (screennotify, the same message
  acd33353 parks on), and release on flat off.
- Backdrop: FLAT-BACKDROP-SAME-AS-WINDOW gives the backdrop its own pen. One
  function `desk_pen_plan(depth, plan*)` in desk_core.c returns the backdrop pen
  and the icon range; a host test asserts no overlap with each other or with
  `dk_flat_pens` slots for depths 2..8. L0e reads the landed backdrop commit
  first.
  L0e done (slice A): the backdrop pen is pen 5 (DK_MUTED), commit 3561616f on branch
  worktree-agent-a19eb3932992a25c3 (not on main yet when recorded). Pens 4 and 5 are taken.
- Theme mapping: new `amarchy/iconpen_core.c`:
  `ip_colours(const struct gtheme *, uint8_t rgb[16][3])`, from gtheme `fg`,
  `accent`, `muted`, `selection`, `bg` with linear blends against `bg` for the
  33/66 levels. Pure, host-tested. `amarchy/iconpen_dos.c` does
  obtain/SetRGB32/release and reuses `wb_pens_apply`'s 8-bit to 32-bit scale.

## 4. On-Amiga applier

- New CLI `UP-Term:bin/amarchy-icons` with `APPLY`, `RESTORE`, `STATUS`
  (`__stack 16384` and `$STACK` cookie). `desk_dos_apply` starts it with
  `pl_start` after `dk_flat_sync` when flat turns on, and with `RESTORE` when
  flat turns off.
- Target order: Workbench defaults first (`ENVARC:Sys/def_*.info` then the same
  in `ENV:Sys`), then the table paths (SYS:System, SYS:Utilities, SYS:Prefs,
  SYS:Tools, Amarchy drawers, root `disk.info` of each volume), then all other
  `.info` files on each volume through `type:` fallback by `do_Type`.
- Volume walk: `LockDosList(LDF_VOLUMES|LDF_READ)` to copy names, unlock, then
  walk each. Skip write-protected (`Info` `id_DiskState` not
  `ID_VALIDATED`), skip `.uaem` sidecars, skip files it cannot lock
  `EXCLUSIVE_LOCK` (counted, retried on the next APPLY; this is the "never
  mid-write" rule), skip the backup drawer.
- Image-only rewrite: pure `li_parse` / `li_splice` in
  `amarchy/iconfile_core.c`. Layout parsed: DiskObject 78 bytes, OldDrawerData
  56 bytes when `do_DrawerData`, Image 1 header + data, Image 2 when
  `SelectRender`, DefaultTool, ToolTypes, ToolWindow, drawer data extension 6
  bytes, optional `FORM ICON` tail. Splice replaces only the Image headers and
  plane data and the gadget fields bound to them (Width, Height, Flags
  `GADGHIMAGE`, SelectRender present); every other byte is copied from the
  source. Not PutDiskObject.
- OS 3.5 colour chunk and NewIcons: both would win over the planar image on 3.2.
  The `FORM ICON` tail is dropped. The NewIcon block (the line
  `*** DON'T EDIT THE FOLLOWING LINES!! ***` and its `IM1=` / `IM2=` tooltypes)
  is removed from the tooltype array; all other tooltypes keep their bytes and
  order. Both go to the backup and come back on RESTORE. This is the one
  exception to "tooltypes byte for byte" (owner question Q1).
- Backup (owner Q2): `SYS:Storage/Icons/Amarchy/<volume name>/<path>.info`, plus
  `manifest` lines `path ; crc of original file ; crc of written images`.
  Not `T:amarchy-revert` (RAM, lost at reboot).
- Idempotence: an icon whose current image crc equals the manifest's written
  crc is skipped; a different crc means the user changed it, so the backup is
  refreshed before the write.
- Restore: splice the backed-up images (and FORM ICON / NewIcon lines) into the
  current file, so later Snapshot or Information edits survive. Whole-file copy
  only when the current file does not parse. Restored entries leave the
  manifest; the drawer is deleted when empty.
- Atomic write: write `<name>.info.amt` in the same drawer, Close checked,
  Rename old to `.amo`, Rename new to the name, Delete `.amo`. A leftover
  `.amt` / `.amo` is resolved on the next run (keep the one that parses, `.amo`
  first).
- STATUS prints counters: `written`, `skipped-locked`, `skipped-ro`,
  `restored`, `pens-held`, `pens-folded`. These are the sentinel.

## 5. Live theme update and Workbench redraw

- `gt_dos_select` calls `ip_dos_apply(theme)` after `desk_dos_apply` /
  `wb_pens_apply`; `pv_set` (theme picker preview) calls it too, and the cancel
  path restores the previous colours.
- On screens up to 8 bits the pens are CLUT entries, so SetRGB32 recolours every
  drawn icon at once. No redraw, no file write. The test checks file dates do
  not move.
- Over 8 bits (RTG truecolour) drawn pixels keep their RGB. 3.2: send the
  Workbench ARexx `MENU INVOKE WINDOWS.UPDATEALL` (name verified in L15) after
  the pen change. 3.1 has no ARexx port: accept stale colours until the next
  window refresh (owner question Q6).
- The Amarchy main tick (`tw_tick`) already retakes pens after a theme switch;
  the icon pens join it so a crash of a theme step leaves the pens consistent.

## 6. Tests

Host (each pure function where it lives, `make test ONLY=<name>`):
- `tests/test_gen_lucide_icons.py`: table parse, unknown name fails, quantise
  levels, selected image plate, planar round trip through mkicon `planes()`,
  32x16 variant.
- `li_parse` / `li_splice` in tests/test_keymap.c style C suite
  (`test_iconfile`): byte round trip of every `.info` under
  `build/rig/os-31` and `build/rig/os-322` (parse then splice same images =
  identical bytes), splice keeps all non-image bytes, FORM ICON strip, NewIcon
  strip keeps other tooltypes and order, restore splice returns original bytes.
- `ip_colours` and the fold table: fixed RGB for two themes, fold per depth,
  `desk_pen_plan` no overlap for depths 2..8.
- Manifest parse/format and the idempotence rule.

Reachability (one test, `tests/rig/test_lucide_reach.py`, real entry point):
turn flat on through `amarchy_menu_rig.py` (the same Amarchy menu path a user
takes), which runs `desk_dos_apply`. Assert `amarchy-icons STATUS` shows
`written > 0` and `pens-held = 16` (sentinel), `ENVARC:Sys/def_drawer.info`
image bytes equal the generator output for `folder`, a screen pixel read at
the RAM: icon shows pen 33..47, then switch theme and assert pen 33 RGB equals
`ip_colours` for the new theme with file dates unchanged, then flat off and
assert the file equals the original byte for byte.

Rig steps (rigs only; A1200 last, owner check):
- 3.2: `UPTERM_RIG=<n> python3 tools/rig/rig.py start --max --os 32 --purpose "Lucide icons: 3.2 reach"`,
  run the reachability test, read every on-screen dialog text, stop the rig.
- 3.1: same with no `--os`, purpose `"Lucide icons: 3.1 reach"`.

## 7. Checklist (25 rows)

Each row: done criterion. First visible slice is L8 (Workbench defaults on 3.2).

- L0a Measure free pens 32..47 at depth 8 on 3.2 rig (ObtainPen result per pen). Done: numbers in thoughts/shared/research.
- L0b Same on 3.1 rig. Done: numbers recorded.
- L0c 3.2 icon.library: does it draw a depth-6 classic image with raw pen indices or remap it? Done: written answer with pixel reads.
- L0d 3.1 workbench 40: pen-0 box behind icons (no transparency) on root and in windows. Done: written answer.
- L0e Read the landed FLAT-BACKDROP fix; record its backdrop pen. Done: pen and commit in this plan.
- L1 Pin lucide-static font in Makefile with SHA-256, licence file, README credit, manifest entry. Done: `make dist-check` passes.
- L2 `dist/lucide-icons` table for defaults, System, Utilities, Prefs, Tools, Amarchy, type fallbacks. Done: generator accepts it.
- L3 `tools/gen_lucide_icons.py` + host test. Done: `lucide.dat` built by `make`, test green.
- [x] L4 `li_parse` / `li_splice` + host suite with os-31/os-322 round trip. Done: suite green on all 267 fixture files. DONE slice B: amarchy/iconfile_core.c, tests/test_iconfile.c (ONLY=iconfile); corpus run on the 267 rig files.
- [x] L5 `ip_colours` + fold table + host test (pens 4..7 roles checked). Done: suite green. Done in the slice D commit (amarchy/iconpen_core.c, tests/test_iconpen.c; fold targets pens 0..5, never green/red 6/7, because the Amarchy flat palette differs from the stock-pen draft table).
- [x] pure half / [ ] backdrop wiring: L6 `desk_pen_plan` shared with backdrop + no-overlap test. Done: suite green, backdrop code uses it. `desk_pen_plan_for` (button 4, backdrop 5, icon pens by depth) + no-overlap test depths 2..8 done. OPEN: backdrop code must read `desk_pen_plan_for().backdrop`; the backdrop commits 3561616f and d12fbf35 are NOT on main, only on branch worktree-agent-a19eb3932992a25c3.
- [ ] L7 Pen holder in amarchy-flat: obtain at boot, re-obtain on screennotify, release on flat off. Done: STATUS shows `pens-held 16` on 3.2 rig. Pure model done and host-tested (`ip_holder_*`, `ip_status_line`); amarchy/flat.c glue (ObtainPen callbacks, screen-change trigger, STATUS line) and rig check OPEN. Rig steps: `UPTERM_RIG=<n> python3 tools/rig/rig.py start --max --os 32 --purpose "<session>: pens-held check"`, flat on, `amarchy-flat STATUS` expects `pens-held 16`; flat off expects 0; Workbench screen close/open expects 16 again; stop rig after 5 s quiet.
- L8 `amarchy-icons APPLY` for Workbench defaults only, backup + manifest, atomic write. FIRST VISIBLE SLICE on 3.2. Done: rig shows Lucide disk/drawer/tool/project/trashcan/RAM, owner check request sent.
  - CODE DONE, RIG OPEN (tick only after the rig steps pass). Code: `amarchy/iconapply_core.c` (pure: manifest, crc idempotence, atomic-write names and recovery table, apply, restore), `amarchy/icons.c` (DOS glue, `__stack` 16384 plus cookie), host suite `make test ONLY=iconapply`. Backup `SYS:Storage/Icons/Amarchy/<volume>/<path>.info`, manifest `SYS:Storage/Icons/Amarchy/manifest`. Targets: `ENVARC:Sys/def_*.info` and `ENV:Sys/def_*.info` for disk, drawer, tool, project, trashcan, RAM.
  - Rig steps (3.2 rig, `UPTERM_RIG=<n> rig.py start --max --os 32 --purpose "<session>: L8/L9 Lucide default icons check"`):
    1. `make wasabi`, `make lucide-dat` (network), `make amiga`, `make build/amiga/amarchy-icons`; install the kit (all Installer options ticked). Check `List UP-Term:Amarchy/Icons/lucide.dat` and `List UP-Term:bin/amarchy-icons`.
    2. Record originals: `Copy ENVARC:Sys/def_#?.info T:orig` then `md5` each; note which of the six exist (3.2 tree has only def_RAM, def_tool; def_disk, def_drawer, def_project, def_trashcan come from icon.library defaults).
    3. `UP-Term:bin/amarchy-icons ?` prints the template, rc 0. `amarchy-icons STATUS` prints `icons=original`.
    4. `amarchy-icons APPLY`. PASS: counters line has `written=12` (or `skipped-nodat` / `skipped-nodefault` named for any missing icon), rc 0, `STATUS` prints `icons=applied`, `SYS:Storage/Icons/Amarchy/manifest` lists the paths, backups exist under `SYS:Storage/Icons/Amarchy/ENVARC/...`. Open Workbench: disk, drawer, tool, project, trashcan, RAM show Lucide images (screenshot via haiku; owner screen check request).
    5. `amarchy-icons APPLY` again. PASS: `written=0 skipped-done=12`, backups still md5 equal to step 2 originals (not the Lucide images).
    6. Read dialog text of any requester that appears; quote it in the report.

- L9 RESTORE + flat off wiring in `desk_dos_apply`. Done: defaults byte equal to originals after flat off.
  - CODE DONE, RIG OPEN. `ia_restore` puts only the image bytes back (later Snapshot / Information edits survive), whole-file copy when the current file does not parse; absent originals (default objects) are deleted again. `desk_dos.c` `dk_icons_sync` (called from `dk_flat_sync`, so from `desk_dos_apply`): flat or flat gadgets on runs `amarchy-icons APPLY`, off runs `amarchy-icons RESTORE`; one process per on/off change; sentinels `desk_dos_icons_apply` / `desk_dos_icons_restore`.
  - Rig steps (continue on the L8 rig after step 5):
    1. `amarchy-icons RESTORE`. PASS: `restored=12 restore-failed=0`, `STATUS` prints `icons=original`, no manifest, `SYS:Storage/Icons/Amarchy` holds no `.info` backup. `md5` of each existing `ENVARC:Sys/def_*.info` equals step 2 (byte equal); the defaults that did not exist are gone from ENVARC: and ENV: and Workbench shows the original icons.
    2. `amarchy-icons RESTORE` again: no-op, `restored=0`, rc 0.
    3. Wiring: in Amarchy settings turn flat system look on. PASS: a process named `amarchy-icons` runs (`Status`), then manifest exists and icons are Lucide. Turn flat off. PASS: RESTORE runs, md5 equal to step 2 again. Toggle on and off twice to see no stuck process (`Status` lists none).
    4. Interrupted write: `Copy` a bad `def_tool.info.amt` beside a good `def_tool.info`, run APPLY; PASS: the leftover is deleted and `def_tool.info` is intact.
    5. Stop the rig when done; do not stop rig 1.

- L10 Table paths: SYS:System, SYS:Utilities, SYS:Prefs, SYS:Tools. Done: rig listing of written counts per drawer.
- L11 Amarchy program icons. Done: written count and screen check.
- L12 All-volume walk, type fallback, read-only / locked / `.uaem` skips. Done: STATUS counters on a rig with one write-protected volume. PURE HALF DONE (iconapply_core: ia_walk_decide / ia_walk_fallback_match / ia_walk_count, counters in STATUS, host suite iconapply); walk itself and rig test open.
- L13 FORM ICON and NewIcon strip plus restore. Done: host suite + rig file with a NewIcon restored byte for byte. HOST PART DONE slice B (NewIcon and FORM ICON strip + restore in iconfile_core, host suite); rig file test open.
- L14 Live theme update in `gt_dos_select` and preview `pv_set`. Done: pen RGB changes, file dates unchanged.
- L15 Over-8-bit redraw (per Q6). Done: 3.2 RTG rig icons recolour after switch.
- L16 3.1 path: fold rules, pen-0 box handling per Q5. Done: 3.1 rig screen check.
- L17 Reachability test `tests/rig/test_lucide_reach.py`. Done: green on 3.2 and 3.1 rigs.
- L18 Rig report with quoted dialog text. Done: report file path.
- L19 Docs (Amarchy docs, `python3 tests/test_amarchy_docs.py`), ledger row ticked. Done: docs test green.
- L20 Ledger row for the mkicon `--drawer` layout bug (62 bytes before the image, stock files use 56). Done: row in todos.md.

## Risks

- Real A1200 icon set may differ from the rig trees (NewIcons, GlowIcons on 3.1 installs). The type fallback covers unknown files; L13 covers the formats.
- ObtainPen can fail if another program holds pens 32..47 (MUI, an RTG driver). Fold path keeps icons readable; STATUS reports it.
- Workbench caches icon images in open windows; a file rewrite needs the drawer reopened. APPLY at flat-on time runs before the user opens drawers in most cases; otherwise the 3.2 Update All covers it.
- Large volumes: the walk is slow on a 68020. It runs in its own process via `pl_start`, never in the input path.

## Owner questions as asked (answers below in Owner answers)

- Q1 NewIcon `IM1=`/`IM2=` tooltypes: strip them (backup keeps them, RESTORE puts them back). Alternative: leave NewIcon files untouched, so they keep their old look.
- Q2 Backup location: per volume `<vol>:.amarchy-icons/`. Alternative: one central store on SYS:, which breaks when a volume moves to another machine.
- Q3 Icon size: 32x32, plus 32x16 on hires non-laced. Alternative: one size for all modes (icons look tall on non-laced).
- Q4 Antialias: 3 coverage levels, selected image on a SELBG plate. Alternative: crisp 1-level glyphs (no blends, fewer pens).
- Q5 3.1 pen-0 box behind icons: accept it (the box is backdrop-coloured when the backdrop pen equals pen 0). Alternative: draw a plate, which looks heavier.
- Q6 Over 8 bits: Update All on 3.2, accept stale colours on 3.1 until refresh. Alternative: rewrite files per theme (owner already rejected file rewrites).
- Q7 Depth 4/5: Amarchy owns pens 8..15. Alternative: fold all to 0..7.
- Q8 Pen range 32..47 as default, final after L0a/L0b.

## Owner answers (2026-10-31, Amarchy s28)

- Q1 NewIcon IM1=/IM2= and OS 3.5 FORM ICON tail: strip, back up, restore byte for byte (as recommended).
- Q2 Backup location: NOT a hidden per-volume drawer (dot-names are not hidden on Amiga). Use `SYS:Storage/Icons/Amarchy/`, mirroring `<volume>/<path>` beneath it. Plan rows L8/L9/L12 follow this.
- Q3 Size: two variants per icon, square-pixel and tall-pixel (for 640x256 style non-laced hires). Amarchy picks the right one from the Workbench screen's pixel aspect (DisplayInfo resolution ratio) at APPLY and again on screen-mode change (screennotify). L3 generates both; L8 picks.
- Q6 Over 8 bits: 3.2 runs Workbench Update All after a theme switch; 3.1 stays stale until window refresh. No per-theme file rewrite.
- Q4, Q5, Q7, Q8: recommended defaults taken (3-level edge blend + selection square; accept 3.1 pen-0 box; pens 8-15 on 4/5-plane screens; pens 32-47 final after L0a/L0b).

- Backdrop pen landed as pen 5 (DK_MUTED, commit 3561616f, s28): WBPattern tiles are 3 planes so pens 0-7 only; 0-3 stock icon pens, 4 flat button fill, 5 free. L5 fold table and L6 `desk_pen_plan` must treat pen 4 and pen 5 as taken.
- Q5 re-check (open): the recommended answer assumed backdrop pen == pen 0. It is now pen 5 on 3.2. On 3.1 the backdrop path is unchanged (palette editor gated, v47+), so the 3.1 root still fills with pen 0 and the pen-0 box matches there. Inside 3.1 drawer windows the window fill is also pen 0, so the box matches. Box only shows if a 3.1 backdrop pattern is set. L0d measures it; ask the owner only if L0d shows a visible box.

## 8. Execution (one agent per slice, sonnet unless noted; cap 3 agents, rig 8 only)

Order follows dependencies. A slice starts when its inputs are landed on main. Every slice: worktree from origin/main, host test where the code lives, revert check for fixes, md5 of built binaries, evidence file under thoughts/shared/research/lucide/, report once.

| Slice | Rows | Needs | Rig | Done when |
|---|---|---|---|---|
| A generator | L0e, L1, L2, L3, L20 | nothing | no | `make` builds lucide.dat with square + tall variants; generator test green; L20 ledger row |
| B icon file core | L4, L13 (host part) | nothing | no | li_parse/li_splice round trip on all os-31 + os-322 .info; NewIcon/FORM ICON strip + restore byte equal |
| C measure | L0a, L0b, L0c, L0d | rig 8 free | 3.2 + 3.1 | pen 32-47 ObtainPen results, icon.library remap answer, 3.1 pen-0 box answer, in plan section 3 |
| D pens | L5, L6, L7 | A, C, backdrop 3561616f landed | 3.2 | STATUS `pens-held 16`; no-overlap test with pens 4, 5 |
| E first slice | L8, L9 | A, B, D | 3.2 | Lucide WB defaults on rig 8 3.2; flat off restores byte equal; backup under SYS:Storage/Icons/Amarchy; owner screen check |
| F coverage | L10, L11, L12, L13 (rig part) | E | 3.2 | per-drawer write counts; read-only volume skipped; NewIcon restored |
| G theme live | L14, L15, L16 | E | 3.2 + 3.1 | pen RGB changes on theme switch, file dates unchanged; 3.2 Update All; 3.1 fold check |
| H close | L17, L18, L19 | F, G | 3.2 + 3.1 | reachability test green on both; docs test green; ledger ticked |

A and B run in parallel now (host only). C runs when rig 8 is free. Opus only if a slice hits an unexplained crash or icon.library remap behaviour that L0c cannot settle.
