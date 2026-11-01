---
date: 2026-10-29
author: spot (Amarchy plan agent)
topic: AMARCHY-P1A
status: decided
ledger: thoughts/spot/todos.md rows AM-ROADMAP, AMARCHY-WB-BACKDROP (:3312), AMARCHY-DESK-EXTRAS (:3314)
builds on: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md Phase 1 (R1.1-R1.22); thoughts/shared/plans/2026-10-26-amarchy-gui-themes.md (landed, 29 of 30, only T19 owner eye check open)
source read at: origin/main 98998de6 (local main lags in 10 files under amarchy/ handler/ config/: line numbers below are origin/main)
---

# Amarchy P1A: desktop-wide theme, native Workbench (R1.1-R1.22)

## Goal

Picking an Amarchy theme restyles the native Workbench, not only the drawn menu: screen palette and DrawInfo pens
(palette.prefs), root and drawer backdrops (wbpattern.prefs), pointer colours and optionally shapes (pointer.prefs),
Workbench font, icon label colours and the screen title (font.prefs / 3.2 workbench.prefs). Every write is live where the
OS allows it, snapshotted first, auto-reverted after 15 s without a confirm, persisted to ENVARC: only after confirm, and
restored at the next Amarchy start after a crash. The whole desktop part is an option (owner R1.18: optional).

Out of scope here (separate plan): R1.23 DOpus 5, R1.24 MUI/Selecta, R1.25 VisualPrefs/FullPalette/Birdie. This plan
leaves one hook for them: a target table in desk_core (D-TGT below) where each later target is one more row with its own
amconf on/off key, snapshot file list and apply function. Out: screen depth change.

Finish line: every item in the checklist ticked, the host suite `desk` green with fail-before demos for the fixes, and the
rig reachability test RX1 passing on a 3.2 rig (060, `UPTERM_RIG=<n> rig.py start --max`), with every knob shown to move
its output file or pixels (knob audit R1.16k).

## What is known about the file formats (from the NDK in the repo)

Source: vendor/ndk-3.2r4-Include_H/prefs/*.h (VTCON_NDK, Makefile:549) and the shipped 3.2 file
build/rig/os-322/Prefs/Env-Archive/Sys/Pointer.prefs. All files are IFF FORM PREF + PRHD (prefs/prefhdr.h) + body chunks.

| File | Chunk | Known (NDK) | Unverified (R1.1 probe) |
|---|---|---|---|
| ENV:Sys/palette.prefs | PALT, struct PalettePrefs (prefs/palette.h:22-28): pap_Reserved[4], pap_4ColorPens[32], pap_8ColorPens[32] (DrawInfo pen index lists, ~0 terminated), pap_Colors[32] ColorSpec (ColorIndex -1 terminated, 16-bit RGB) | DrawInfo pen indices TEXTPEN.. BARBLOCKPEN, NUMDRIPENS 13 (intuition/screens.h:101-115) | whether 3.2 Palette writes more than PALT (OS 3.2 Palette editor may add a chunk for >8 colours); whether ColorIndex 17-19 (sprite colours) in pap_Colors reach the pointer or pointer.prefs wins; which change IPrefs applies live and which needs a Workbench screen reset |
| ENV:Sys/wbpattern.prefs | PTRN, struct WBPatternPrefs (prefs/wbpattern.h:10-19): wbp_Which WBP_ROOT/DRAWER/SCREEN, wbp_Flags (WBPF_PATTERN 0x1, WBPF_NOREMAP, placement TILE/CENTER/SCALE), wbp_Depth, wbp_DataLength, data follows; PAT_WIDTH/HEIGHT 16, MAXDEPTH 3 | pattern = 16x16 planes, depth <= 3 | data layout when WBPF_PATTERN is clear (picture file name?), whether IPrefs re-reads on file change (notify) or `SYS:Prefs/WBPattern USE` must run, 3.2 extra fields |
| ENV:Sys/pointer.prefs | PNTR, struct PointerPrefs (prefs/pointer.h:11-23): Which, Size, Width, Height, Depth, YSize, hotspot X,Y; colour table of (1<<Depth)-1 RGBTable entries; bitmap data follows | Shipped 3.2 file: 41 PNTR chunks (pp_Which 0x00-0x28, not only NORMAL/BUSY), each 137 bytes, 16x24, depth 2 = 3 colours per chunk | meaning of the 39 extra Which values (3.2 pointer types), whether colour change needs all 41 chunks, live apply path |
| ENV:Sys/font.prefs | FONT, struct FontPrefs (prefs/font.h): fp_Type FP_WBFONT 0 / FP_SYSFONT 1 / FP_SCREENFONT 2, fp_FrontPen, fp_BackPen, fp_DrawMode, fp_SpecialDrawMode, TextAttr, fp_Name[128] | icon label text colour lives HERE (FP_WBFONT FrontPen/BackPen/DrawMode), not in workbench.prefs | VERIFIED: apply live at once via `Font FROM x USE`; FrontPen = text, BackPen = fill/outline/shadow colour, DrawMode 0 JAM1/1 JAM2/2 COMPLEMENT/4 INVERSVID, fp_SpecialDrawMode 1 = outline, >=2 = shadow, both only with JAM1; FONT chunks 156 bytes x3 (types 0,1,2), TextAttr at body 20, name at 28; file ENV:Sys/Font.prefs |
| ENV:Sys/workbench.prefs | WBNC struct WorkbenchPrefs + WorkbenchExtendedPrefs (wbe_LockPens locks pens 4..7, wbe_DisableTitleBar, wbe_TitleUpdateDelay, wbe_Flags "assorted options", no defines), WBHD hidden devices, WBTF V47 title format string (prefs/workbench.h:37-47) | screen title = WBTF wtfp_Format | VERIFIED: WBNC is 50 bytes (BOOL = 16 bit), WBTF = NUL string, no label bit in WBNC (outline is font.prefs special mode); WBTF NOT live (title unchanged after USE). OPEN: format codes, wbe_Flags bits |

Consequence for R1.22: the roadmap row says "3.2 workbench.prefs" for icon labels; the NDK puts label pens in font.prefs
(FP_WBFONT) and only the title format in workbench.prefs. The probe settles it; see owner decision OD10.

## Reuse map (file:line, origin/main 98998de6)

| Need | Reuse | Where |
|---|---|---|
| Theme parse, built-in default, user-over-shipped lookup | gt_parse / gt_load / gt_builtin, struct gtheme | amarchy/gthemes_core.h:19-50 |
| The one theme-choice path (hotkey, both menu row kinds) | gt_dos_select (saves AM_GTHEME) | amarchy/gthemes_dos.c:46; callers amarchy/amarchy.c:180 (KA_GTHEME), amarchy/leaf.c:354 (Style > Theme row), amarchy/leaf.c:541 (LF_GTHEME) |
| Theme -> 16-colour roles | menu_theme_roles_gui | amarchy/menu_theme.h:54-57 |
| Saved option keys, ENV:+ENVARC: save | am_keys / AM_* enum, amconf_save | amarchy/amconf_core.h:12-26, amarchy/gthemes_dos.c:57-59 (call pattern) |
| Prefs window row for the on/off option | settings_def rows on PG_PAGE_AMARCHY | prefs/settings_def.c:175-184 |
| Action kind for a new verb | KA_GTHEME / KA_WIN pattern | config/keymap.h:79-80; dist/keymap:174 (style.theme submenu) |
| Hotkey dispatch, one counter | do_row | amarchy/amarchy.c:163-186 |
| ARexx verbs + STATUS sentinel | verb dispatch, STATUS reply builder | amarchy/amarchy.c:224 (verbs), :143-158 (STATUS key[]/val[]) |
| Global variable sentinel | am_sentinel / am_sentinel_str | amarchy/sentinel_dos.h:5-7 |
| Commodity Wait loop (add the confirm timer signal) | Wait mask | amarchy/amarchy.c:332 |
| timer.device tick | jb timer open + TR_ADDREQUEST | amarchy/job_dos.c:80, :106; tile.c poll timer (amarchy/tile.c:9) |
| Big-endian IFF byte helpers | put32 / get32 (static today) | handler/clipfmt.c:70, :78 -> extract to one shared helper (single source), used by clipfmt and desk_core |
| Pen readback on the WB screen | GetRGB32 on the WB ColorMap | handler/vtcon_handler.c:2633, amarchy/gui.c:198 |
| Requester | EasyRequestArgs | handler/vtcon_handler.c:1824 (pattern) |
| Rig test pattern (table driven, resumable, verdict files, --only/--failed) | amarchy_menu_rig.py, amarchy_rig.arexx | tools/rig/amarchy_menu_rig.py:1-30, tools/rig/amarchy_rig.py:30; ami.read_file tools/rig/ami.py:518; IFF read from ENV: in python: tools/rig/keymap_rig.py:36 |
| Theme role exemption list (knob audit) | GUI themes plan section 1, "20 keys parsed and ignored" | thoughts/shared/plans/2026-10-26-amarchy-gui-themes.md section 1 (update it when desk uses more keys) |

Searched for and NOT found (build new): any palette.prefs / wbpattern / pointer.prefs / font.prefs writer, LoadRGB32 caller,
IPrefs nudge, snapshot/revert code (git grep over amarchy/ config/ handler/ tools/rig/: only GetRGB32 reads).

## Probe corrections to the format table (R1.1b-d, see research note Part 2)
- wbpattern.prefs PTRN struct: 16 reserved bytes, Which, Flags, Revision, Depth, DataLength, data; USE writes ROOT, DRAWER,
  SCREEN always; pattern = depth 3, 96 bytes planar; picture = NUL-terminated path (flags without 0x1, NOREMAP 0x10 kept).
- pointer.prefs: colour table (3 RGB byte triples) at chunk offset 32; all 41 chunks share one table; live via `Pointer USE`.
- palette: pens 2..12 and colours 0-3 apply live after windows are closed; colours 4-7, 17-19 and pens 0/1 do not.

## Design

- amarchy/desk_core.[ch] (pure, host-tested, no OS calls): `desk_plan(const gtheme *, const desk_opts *, desk_out *)`
  turns a theme into byte images: PALT, PTRN root + drawer, PNTR colour rewrite of a read pointer.prefs, FONT pen fields
  of a read font.prefs, WBNC/WBTF. Read-modify-write for pointer/font/workbench: the user's shapes, font names and other
  fields are kept byte for byte; only the themed fields change. Also: darker-tone derive, contrast checks, pen mapping,
  backdrop cycle index, snapshot file list, target table D-TGT `{ name, amconf key, files[], plan_fn, apply_kind }`.
- amarchy/desk.c (OS side, commodity only): snapshot to T:amarchy-revert/, write ENV:, live apply (LoadRGB32 on the WB
  ViewPort; IPrefs nudge per probe result), confirm requester + 15 s timer in the Wait loop (amarchy.c:332), revert,
  ENVARC: persist on confirm, ENVARC:Amarchy/pending-revert crash marker, boot-time restore.
- Trigger: gt_dos_select stays the one choice path. Desktop apply runs in the commodity because it owns the timer and
  outlives the menu: the menu process (leaf.c:354/:541) sends ARexx `DESK THEME <name>` to AMARCHY after a successful
  select; the hotkey path (amarchy.c:180) calls desk_apply directly. Both bump one counter. (OD1 decides whether Style >
  Theme does this or a separate row does.)
- New theme keys read: dark_background / darker_background / lighter_background / selection / muted (already in every
  shipped .theme file, GUI plan section 1), plus optional `backdrop=`, `font=`, `wbtitle=` (roadmap R1.5).

## Checklist (7 of 40 done; R1.1e font part done, WBTF format codes + wbe_Flags bits still open)

Each item: what / where / test that proves it. "FB/PA" = fail-before / pass-after demo required (fixes, guards).

Probe first (no product code before R1.1 is answered; decision 10):
- [x] R1.1a Probe palette.prefs: save from SYS:Prefs/Palette on the 3.2 rig with 4 and 8 colours, dump chunks (text only,
  python IFF walker like keymap_rig.py:36), record exact PALT bytes as golden files tests/data/desk/palette-*.prefs. Test:
  golden files committed, chunk list in the research note.
- [x] R1.1b Probe what applies live: write a changed ENV:Sys/palette.prefs and check GetRGB32 of pen 0 and DrawInfo pens
  (wasabi read) without and with `C:IPrefs`; same for pap_4ColorPens. Record: colours live? pens need screen reset?
- [x] R1.1c Probe wbpattern.prefs: WBPattern USE with solid root, tiled root, drawer entry; golden files; does a file write
  alone redraw the backdrop?
- [x] R1.1d Probe pointer.prefs: change only colours in Pointer prefs, diff vs shipped file (which of the 41 chunks
  change); does pap_Colors 17-19 in palette.prefs move the pointer at all on 3.2? Live apply path.
- [ ] R1.1e (font.prefs part done in 2e67b155, see research note; WBTF codes and wbe_Flags open) Probe font.prefs FP_WBFONT pens and fp_SpecialDrawMode (label outline/shadow values) and workbench.prefs WBTF
  format codes + wbe_Flags (save from SYS:Prefs/Workbench with each option toggled, diff).
- [x] R1.1f Probe pen readback for the rig test: does wasabi expose the WB ViewPort colours; else a tiny tests/amiga/
  penprobe (GetRGB32 of pens 0-7 and 17-19, DrawInfo dri_Pens) printing text. Test: prints known default values.
- [x] R1.1g (2e67b155) Write findings to thoughts/shared/research/2026-10-29-amarchy-p1a-prefs-probe.md; update this plan's format
  table (unverified -> verified or changed). Owner decisions OD3, OD10, OD11 are asked again with the probe facts.

Core (host-tested, pure):
- [x] R1.0 Extract put32/get32 from handler/clipfmt.c:70/:78 into one shared helper; clipfmt uses it. Test: test_clip (ee7b7004) (ee7b7004)
  unchanged green; desk tests link the same helper.
- [x] (2bc631f7) R1.5 gtheme carries the extra keys (dark/darker/lighter background, selection, muted) and backdrop/font/wbtitle;
  missing key = derived value (OD5). Where gthemes_core.[ch]. Test: test_gthemes new cases, every shipped theme parses
  them; FB/PA: parser change reverted -> fails.
- [x] (a2c7cd2e) R1.2 desk_core PALT bytes: theme -> pap_Colors 0-3 (4-colour) and 0-7 (8-colour) per mapping OD13. Test: byte-exact
  vs R1.1a golden for a fixture theme made of the probe's colours.
- [x] (a2c7cd2e) R1.19 DrawInfo pens in pap_4ColorPens / pap_8ColorPens: title fill (FILLPEN), FILLTEXTPEN, SHINEPEN/SHADOWPEN,
  BARBLOCKPEN/BARDETAILPEN, TEXTPEN, HIGHLIGHTTEXTPEN mapped to pen indices (OD13). Test: golden bytes; each of the
  mapped pen slots asserted moved by changing its role colour (knob).
- [x] (a2c7cd2e) R1.18a Darker tone: pure `desk_darker(theme)` per OD5, no hardcoded colour. Test: per shipped theme, darker luma <
  background luma, and contrast vs the bar/menu background role (MT_BG) and vs foreground above a threshold (OD5 states it).
- [x] (a2c7cd2e) R1.3 desk_core PTRN root bytes, solid and tiled flat pattern (OD6), WBPF_PATTERN, depth <= screen depth. Test:
  golden vs R1.1c.
- [x] (a2c7cd2e) R1.20 PTRN drawer entry (WBP_DRAWER) in the tone of OD7. Test: golden; drawer colour index differs from root when
  OD7 says panel tone.
- [x] (a2c7cd2e) R1.21a Pointer colours: read-modify-write of every PNTR chunk's RGBTable (count from R1.1d), bitmaps byte-identical.
  Test: on the shipped os-322 Pointer.prefs fixture, only colour bytes differ (diff count = chunks x 9 bytes max).
- [ ] R1.21b Optional pointer shapes (normal/busy) per OD8, off by default. Test: shape on -> PNTR Which 0/1 bitmap bytes
  replaced, others kept; off -> byte-identical to R1.21a output.
- [x] (a2c7cd2e) R1.4 FontPrefs: theme `font=` names an existing font only (decision 4); unknown font -> key ignored, file untouched.
  Test: golden; missing font -> no FONT chunk change.
- [x] (a2c7cd2e) R1.22a Icon label colour and outline (OD10: JAM1, special mode 1) in FP_WBFONT pens, or workbench.prefs if the probe says so
  (OD10). Test: golden; label pen != backdrop pen (contrast guard).
- [x] R1.22b (closed by OD11, owner 2026-10-29: title stays stock) Screen title WBTF (3.2 / workbench.library >= 47 only, OD11); older OS: target skipped and reported. Test:
  golden; v40 mode produces no WBNC write.
- [x] (a2c7cd2e) R1.6 Contrast guard over all shipped themes: pen 0 vs pen 1, text vs fill, label vs backdrop, darker vs MT_BG; a theme
  failing the guard is refused with a message, nothing written. Test: all 23 shipped themes pass; FB/PA with a fixture
  theme fg == bg -> refused.
- [x] (a2c7cd2e) D-TGT Target table in desk_core: rows palette, wbpattern, pointer, font, workbench (each its own amconf key, OD2);
  R1.23-R1.25 add rows later. Test: a target switched off writes nothing and is absent from the snapshot list.
- [ ] R1.11 Backdrop set per theme: solid generated first entry, then UP-Term:Amarchy/Backdrops/<theme>/*, user dir
  ENVARC:Amarchy/backdrops/<theme>/ wins on same name; cycle index wraps. Test: host list/override/wrap cases.
- [x] (a2c7cd2e) R1.16a Revert logic (pure): snapshot list = exactly the files the enabled targets write; restore order; pending
  marker content. Test: host cases incl. a crashed-marker restore choosing the right files.

OS side (commodity):
- [x] R1.7 (rig 3 PASS for apply, snapshot and revert copies, ENVARC unchanged, master off writes nothing: fix 9265f711; code in desk_dos.c, commit 863752b6; OPEN: rig 3 run of tools/rig/amarchy_desk_rig.py shows AMARCHY_DESKS 0 / DESKMASK 0 after a Style > Theme pick with desktop-theme = on in ENV:Amarchy/prefs, so desk_dos_apply returns before writing; cause not yet found, surface its return value) desk.c: snapshot to T:amarchy-revert/, write ENV:Sys/*.prefs, LoadRGB32 live palette, IPrefs nudge per R1.1b-d.
- [ ] R1.8 Confirm requester (EasyRequest from the commodity, or gui.c window) + 15 s timer in the Wait loop amarchy.c:332
  (job_dos.c:80 timer pattern); timeout -> restore snapshot + re-apply. Rig: RX1 step 4.
- [ ] R1.9 Boot-time restore: pending-revert marker in ENVARC:Amarchy/ read at Amarchy start. Rig: RX1 step 6.
- [ ] R1.10 ENVARC: persist only after confirm. Rig: RX1 step 5 (ENVARC unchanged before confirm, equal after).
- [ ] R1.18b (keys desktop-theme/-palette/-backdrop/-pointer/-labels as hidden settings rows, commit f7234895; no PG_PAGE_AMARCHY gadget yet; restore-on-off open) Option on/off: amconf key (e.g. `desk-theme`, plus per-target keys D-TGT) and a PG_PAGE_AMARCHY row in
  prefs/settings_def.c; turning it off restores the snapshot taken before the first desktop apply (OD2, OD14). Test: host
  amconf key round-trip; rig: off -> ENV: files equal the pre-Amarchy originals.
- [ ] R1.12 KA_DESK `desk:<theme NAME|backdrop next|revert|confirm>` in config/keymap.h after KA_GTHEME (:80), arg check
  like win_arg_ok, dist/keymap line `super+ctrl+space = desk:backdrop next`, do_row case. Test: test_keymap parse cases;
  FB/PA on the arg check.
- [ ] R1.12b ARexx `DESK THEME|BACKDROP|REVERT|CONFIRM` verbs at amarchy.c:224; leaf.c:354/:541 send `DESK THEME` after
  gt_dos_select when the option is on (OD1). Test: host none (glue); rig RX1.
- [ ] R1.13 Menu rows: Style > Backdrop (next, list) and the desktop on/off row (OD1 decides whether a separate
  "Desktop theme" row exists). Test: menu_core row test like style_theme_lists_the_gui_theme_rows.
- [ ] R1.14 STATUS sentinels: `desks=` (applies), `reverts=`, `confirms=`, `restores=` appended to key[]/val[] at
  amarchy.c:143-158, plus am_sentinel AMARCHY_DESK=<theme>. Test: rig RX1 reads them.
- [ ] R1.12c Emergency restore (OD12): hotkey `desk:revert` restores the last confirmed set without any requester.

Docs, install, tests, cleanup:
- [ ] R1.15 man/amarchy.1 section "Desktop theme"; installer copies UP-Term:Amarchy/Backdrops (empty per-theme drawers,
  decision 3: no images shipped). Test: install script dry-run lists the drawer.
- [x] (a2c7cd2e) R1.16 Host suite `desk` (make host test ONLY=desk), R1.2-R1.6, R1.11, R1.16a, R1.18a, R1.19-R1.22.
- [ ] R1.16k Knob audit: every option key and every theme key used moves its output (byte diff on host, pixel or file
  diff on the rig); unused keys carry the written exemption (update GUI-themes plan section 1).
- [ ] R1.17 Rig reachability test RX1 (below), tools/rig/amarchy_desk_rig.py, verdict files, --only/--failed. [OPEN after 9265f711: rig 3 steps off and on pass except ENV:Sys palette.prefs and wbpattern.prefs bytes differ from desk_expect (stock editors rewrite them?); pointer and font are not compared by the script yet]
- [ ] R1.17b Type/host/amiga build: make host test, make amiga; desk_core.c and desk.c in the amiga source lists.
- [ ] R1.17c Cleanup: no stray branches, worktrees, rigs; rig stopped; ledger rows AMARCHY-WB-BACKDROP and
  AMARCHY-DESK-EXTRAS updated with commits.

## Host tests (suite `desk`, tests/test_desk.c, golden files in tests/data/desk/)

- H1 PALT byte-exact vs probe golden, 4- and 8-colour.
- H2 DrawInfo pen lists: each mapped slot moves when its role colour moves; ~0 terminator kept.
- H3 PTRN root solid + tiled, drawer entry, byte-exact.
- H4 PNTR read-modify-write on the shipped 3.2 Pointer.prefs: only RGBTable bytes change; shapes option replaces only
  Which 0/1 bitmaps.
- H5 FONT: pens set, fp_Name and TextAttr of the user kept; unknown theme font ignored.
- H6 WBNC/WBTF: title format written, other fields kept; v40 skip.
- H7 Contrast guard over all 23 shipped themes + refusing fixture.
- H8 Darker tone below background luma and above the contrast floor vs MT_BG for every shipped theme.
- H9 Backdrop list/override/wrap.
- H10 Revert: snapshot list = enabled targets' files; crash-marker restore.
- H11 Target off -> no bytes, no snapshot entry (D-TGT).
- Every guard/fix item demonstrates FB/PA by reverting its line once and recording the failing assertion (file:line).

## Rig reachability test RX1 (one test, top-level entry point)

tools/rig/amarchy_desk_rig.py on a 3.2 rig started `UPTERM_RIG=<n> rig.py start --max --purpose "<session>: P1A desktop
theme reachability"`; Amarchy and the build of this branch installed. Steps, each a verdict row:
1. Baseline: read STATUS (desks/reverts/confirms/restores), md5 of ENV:Sys/{palette,wbpattern,pointer,font,workbench}.prefs
   and ENVARC: copies, penprobe output (R1.1f).
2. Apply through the real entry point: the hotkey bound to the theme choice (raw key events through the commodity, not the
   ARexx verb) selects `catppuccin` (any theme != current). Assert: STATUS desks +1 exactly; AMARCHY_DESK=catppuccin;
   every enabled ENV: file changed and equals host desk_core output for that theme (byte compare: the code that ran is the
   code tested); penprobe pen 0 == theme's mapped colour, pointer colour 1 == mapped, drawer/backdrop pixels in the darker
   tone (region probe of an empty backdrop area); ENVARC: unchanged.
3. Super+Ctrl+Space through the hotkey: STATUS desks +1, wbpattern.prefs root entry changed to the next backdrop.
4. Let the requester time out (> 15 s, no click): reverts +1; ENV: files equal the step-1 md5; penprobe equals baseline.
5. Apply again and confirm (click the requester's confirm gadget through wasabi): confirms +1; ENVARC: equals ENV:.
6. Crash path: apply, kill Amarchy before the timeout, start Amarchy: restores +1, ENV: equals the last confirmed set.
7. Option off (R1.18b): ENV: files equal the pre-Amarchy originals from step 1.
Read every on-screen dialog text before any "silent" verdict. Stop the rig at the end (not rig 1).

## Risks

- Live apply vs reboot: palette colours apply live via LoadRGB32; DrawInfo pen assignments and the screen font are read
  when the Workbench screen opens, so they may need an IPrefs screen reset (closes every window, the "reset the Workbench
  screen" requester rig.py:1459 handles) or a reboot. A 15 s confirm on a change that is not visible yet confirms
  nothing. R1.1b measures it; OD3 decides.
- Unreadable screen: contrast guard refuses before writing; auto-revert on timeout; boot-time restore; emergency revert
  hotkey (OD12). The revert path itself must not depend on the screen being readable (no click needed).
- Revert on a bad theme file: unparseable keys fall back to built-in values (gt_parse contract), and the guard refuses a
  theme whose result fails contrast; the snapshot is taken before any write, so a half-written apply is restored.
- Clobbering user prefs: pointer shapes, font names, workbench options are read-modify-write; turning the option off
  restores the originals (OD14 decides what "originals" means after a confirm).
- IPrefs not re-reading a file write (unverified, R1.1b-d); fallback `SYS:Prefs/<Editor> USE <file>` per editor.
- 3.2 vs 3.1/3.9: 41-chunk pointer.prefs and WBTF are 3.2-only; detect workbench.library version, skip a target cleanly.
- wbe_LockPens: if set, Workbench locks pens 4-7 itself; theming them may be overwritten or fight it.
- Chip RAM: picture backdrops on 2 MB chip limited by depth <= screen depth; solid/tiled patterns cost 16x16xdepth.
- Two processes: the menu process must not apply the desktop itself (it exits; the timer would die with it). Glue sends
  `DESK THEME`; a missing commodity means the menu theme saves and the desktop part reports "Amarchy not running".

## Owner answers (2026-10-29)

- OD1: (a) Style > Theme restyles the menu and the desktop when the desktop option is on.
- OD2: (a) off by default; one master switch plus one switch per part (palette, backdrop, pointer, labels, title), all on
  under the master.
- OD3: (a), re-confirmed 2026-10-29 with the R1.1b facts: apply every live part at once under the 15 s confirm (colours
  0-3, pens 2-12, icon labels, pointer colours, backdrop pattern). Amarchy never closes Workbench windows; when drawers are
  open, the stock IPrefs "close windows, then Retry" requester handles the pens. The confirm window says colours 4-7 apply
  after a reboot.
- OD4: (b) a themed gui.c window with a countdown, not EasyRequest. Constraint for the build: the window must stay usable
  when the theme under test is unreadable, so Escape and the timeout both revert, and the window draws its text and
  buttons with a contrast check (fall back to stock pens 1/2 when the pair fails the floor). The OD12 key is the last way
  out.
- OD5: (a) the theme's darker_background, else dark_background, else background at 80 % luma; the contrast floor in the
  test falls back to computed.
- OD6: the backdrop is a Workbench pattern tile in wbpattern.prefs (PTRN), repeated by Workbench, never a picture. Owner
  reason: it uses less memory. The tile is on by default, drawn in the darker tones; a solid choice stays available.
- OD7: (a) drawer windows use the theme background (MT_BG).
- OD8: (b) ship flat normal and busy pointer shapes now; the other 39 3.2 pointer types keep their shapes and get the
  theme colours. Two pointer designs are a new asset row.
- OD9: (a) pointer pen 1 = foreground (body), 2 = background (outline), 3 = accent (highlight).
- OD10: (a), re-confirmed 2026-10-29: FP_WBFONT in font.prefs, FrontPen = theme foreground, BackPen = a darker tone,
  DrawMode JAM1, fp_SpecialDrawMode 1 (outline, proven live by R1.1e). The user's font name and size are kept.
- OD11: (a), re-confirmed 2026-10-29 (WBTF is not applied live, format codes unproven): the screen title stays stock
  until the Phase 2 bar. R1.22b is closed by this decision.
- OD12: (a) a desk:revert binding (proposed super+ctrl+shift+backspace) restores the last kept set with no requester.
- OD13: (a) flat dark mapping: 0 background, 1 darker tone, 2 foreground, 3 accent; 4-7 selection, muted, green, red.
- OD14: (a) off restores the prefs files from before the first apply, kept in ENVARC:Amarchy/desk-original/.
- OD15: (a) 3.1 and 3.2 for palette, backdrop and label pens; the 41-chunk pointer file and the title on 3.2 only, chosen
  by version. Tests need a 3.1 rig as well.

## Owner decisions (to ask; nothing below is decided in this plan)

Not reopened (already decided): 15 s auto-revert + persist after confirm + boot restore (roadmap decision 2); solid
backdrop + user picture drawer, no shipped images (3); fonts only from FONTS: (4); R1.18 darker-tone flat backdrop,
optional; R1.19 pens in palette.prefs; R1.20 drawer backdrops in wbpattern.prefs; R1.21 pointer.prefs; no depth change.

- OD1 What triggers the desktop. (a) Recommended: Style > Theme applies menu AND desktop when the desktop option is on
  (Omarchy: one theme restyles everything). (b) A separate Style > Desktop theme row with its own list. Trade-off: (a) one
  choice, but every menu-theme try costs a confirm requester; (b) independent, but menu and desktop can drift apart.
- OD2 Default of the desktop option and per-target switches. (a) Recommended: off by default, one master switch plus a
  switch per target (palette, backdrop, pointer, font/labels, title), all on once the master is on. (b) On by default.
  Trade-off: (a) nothing on a user's Workbench changes until opted in, matches R1.25's default-off rule; (b) visible out of
  the box but rewrites prefs on first theme pick.
- OD3 Changes that are not live (pen assignments, screen font, maybe title). (a) Recommended: apply colours live with the
  15 s confirm; write reset-only parts to ENV: with the same snapshot, show "applies after Workbench reset / reboot" in the
  requester, persist to ENVARC: on confirm. (b) Force an IPrefs Workbench screen reset during apply (closes all windows).
  (c) Leave pen assignments out. Trade-off: (a) no windows closed, but the user confirms parts not yet seen; (b) true
  preview, closes every window; (c) loses R1.19. Re-asked after the R1.1b probe.
- OD4 Confirm UI. (a) Recommended: Intuition EasyRequest from the commodity ("Keep this desktop theme? 15", Keep/Revert).
  (b) A themed gui.c window with a countdown. Trade-off: (a) small, always readable with stock pens; (b) on-brand but drawn
  in the colours being tested.
- OD5 Darker tone source. (a) Recommended: the theme's own `darker_background` key, else `dark_background`, else
  background scaled to 80 % luma; contrast floor vs MT_BG stated as a number in the test. (b) Always computed (factor).
  (c) A user percentage knob. Trade-off: (a) theme author's colour, may be too close to background in some themes (guard
  catches it, then falls back to computed); (b) uniform but ignores the theme; (c) one more knob to test.
- OD6 Backdrop pattern. (a) Recommended: solid flat fill by default, with a "tiled" choice that adds a sparse 16x16 dot
  grid in dark_background on the darker tone. (b) Tiled by default. (c) Solid only. Trade-off: (a) calm default, pattern
  for those who want it; (b) more texture, more chip pens used; (c) simplest.
- OD7 Drawer window background. (a) Recommended: theme background (MT_BG, the panel tone, so drawers look like Amarchy
  panels on the darker desktop). (b) Same darker tone as the root. (c) Leave drawers stock. Trade-off: (a) drawers stand
  out from the desktop; (b) uniform, drawers blend into the backdrop; (c) no change.
- OD8 Pointer shapes. (a) Recommended: colours only now; shapes later as an option once a flat normal/busy pair is
  designed. (b) Ship flat normal + busy shapes now, others kept. (c) Theme all 41 3.2 pointer types. Trade-off: (a) no
  asset work, small risk; (b) more Omarchy-like, needs drawing two pointers; (c) large asset job.
- OD9 Pointer colour mapping. (a) Recommended: 1 = foreground (body), 2 = background (outline), 3 = accent (highlight).
  (b) accent body, background outline, foreground highlight. Trade-off: (a) readable on every backdrop; (b) louder pointer,
  weak on accent-heavy themes.
- OD10 Icon label location. (a) Recommended: use FP_WBFONT pens in font.prefs (verified in the NDK), keep the user's font
  name, add outline only if R1.1e proves the bit. (b) Wait for a workbench.prefs field as the roadmap row says. Trade-off:
  (a) works on 3.1 and 3.2, but writes font.prefs even when the theme names no font; (b) may not exist. Re-asked after
  R1.1e.
- OD11 Screen title. (a) Recommended: leave the title stock until P2's bar exists (bar shows clock/memory). (b) An Amarchy
  title format, e.g. "Amarchy  <theme>  <chip/fast free>" via WBTF on 3.2. Trade-off: (a) no duplicated info later;
  (b) visible now, 3.2 only, format codes unverified.
- OD12 Emergency restore hotkey. (a) Recommended: yes, a `desk:revert` binding (e.g. Super+Ctrl+Shift+Backspace) that
  restores the last confirmed set without a requester. (b) None: requester timeout and boot restore only. Trade-off:
  (a) one more binding, a way out when the screen is unreadable after confirm; (b) fewer keys.
- OD13 Pen mapping (4-colour pens 0-3, 8-colour 4-7). (a) Recommended: 0 = background, 1 = darker tone (shadow/detail),
  2 = foreground (text/shine), 3 = accent (fill/title bar); 4-7 = selection, muted, green, red; DrawInfo TEXTPEN 2,
  SHINEPEN 2, SHADOWPEN 1, FILLPEN 3, FILLTEXTPEN 0, BARBLOCKPEN 0, BARDETAILPEN 2. (b) Keep the stock pen order (1 dark
  text, 2 light shine) and only recolour. Trade-off: (a) flat dark look, but programs that assume pen 1 = text may draw
  light-on-light; (b) safest for old programs, less Omarchy.
- OD14 What "off" restores. (a) Recommended: the prefs files as they were before Amarchy's first desktop apply (kept in
  ENVARC:Amarchy/desk-original/). (b) The last confirmed set before the current one. Trade-off: (a) clean uninstall of the
  look; (b) undo of one step only.
- OD15 3.1 support. (a) Recommended: palette, backdrop, font pens on 3.1 too; 41-chunk pointer and WBTF on 3.2 only,
  detected by version. (b) 3.2 only for the whole desktop theme. Trade-off: (a) more test rows (two rigs); (b) simpler,
  excludes 3.1 users.

DOpus desktop probe 2026-10-29 (rig 8, 5.104): thoughts/shared/research/2026-10-29-amarchy-dopus-desktop-probe.md. Desktop is a BACKDROP|BORDERLESS window on the Workbench screen; `dopus screen` first word tells Workbench desktop from own screen DOPUS.1.
