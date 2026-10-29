---
date: 2026-10-29
author: spot (Amarchy planning agent)
topic: AMARCHY-LX
status: draft
ledger: thoughts/spot/todos.md (no AMARCHY-LX row; the Omarchy-gap row at :2797-2804 records the owner answers; add row AMARCHY-LX when approved)
roadmap: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md, Phase L (RL.1-RL.7, :328) and Phase X (RX.1-RX.5, :339); decisions 13, 14, 16, 17, 18, 19
depends: Phase J (job view, RJ.2 logo header band, RJ.3 input rows), AMARCHY-UHC (L runs after it, decision 17), Phase 2 R2.3 timer tick (RX.3 only), Phase 4 R4.9a/b (RX.3 only), P1b P1B-D1 `desk_dopus_mode()` (DOpus rows)
---

# Amarchy Phase L (logo, font tiers, About) and Phase X (extras) (AMARCHY-LX)

## Goal

Omarchy's look and four extras in the Amarchy drawn menu, on both desktops the owner supports: Workbench and
Directory Opus 5 (Magellan II) as Workbench replacement or on its own screen.

1. A replaceable logo shows for about 2 s when the Amarchy commodity starts, and heads every job view (RJ.2).
2. The drawn menu uses three font tiers (header, row title, description line) scaled from one theme base size.
3. An About row shows the logo, Kickstart and Workbench (or DOpus) versions, CPU, chip/fast RAM, UP-Term and Amarchy versions, uptime.
4. Extras: toggle rows that show their state, an add-launcher wizard, reminders, a disk speed test.

Done (REACHABILITY_PROTOCOL): per phase one rig run (060, `rig.py start --max`) drives the real entry points (the commodity
start, the real menu hotkey, real row picks through the input stream), reads the AMARCHY ARexx `STATUS` counters as the
sentinel, and asserts the numbers below. Rigs run DOpus: each rig test runs twice, once on a plain Workbench rig and once on a
rig with DOpus 5.91+ (kit 5.104) as Workbench replacement.

## Facts found (origin/main 3ec9b09e) and what to reuse

| Need | Reuse | Where |
|---|---|---|
| STATUS sentinel string | `status_text` key/val arrays (5 keys today); P2 branch adds `bar draws panels launches`, P4 adds `shots notes` | amarchy/amarchy.c:143-161, verb at :224 |
| One counter per action | `do_row`, `g_hits++` | amarchy/amarchy.c:163-170 |
| Menu font open | `pick_font` -> `mt_open(scr, square, spec)`, ONE `menu_text` per gui (`g->mt`); spec from amconf AM_FONT ("name:size") | amarchy/gui.c:1462-1470, close :1456; amarchy/amconf_core.h:15 |
| Font fallback | `fontpair_menu_choices`: FONTPAIR_MENU_MAX = 3 CANDIDATES for ONE font, not three tiers (the roadmap's "menu_font.c opens up to three fonts per screen" is a fallback list) | render/fontpair.h:31-44, amarchy/menu_font.c:60-64 |
| TrueType at any pixel size | `mt_open_ttf_spec(m, spec, px)` | amarchy/menu_text.c:191-217 |
| Text width, fit, draw | `mt_width`, `mt_fit`, `mt_draw`, `mt_height`, `mt_baseline` | amarchy/menu_text.h:18-44 |
| Row geometry | `mg_metrics` (head_h, row_h, pitch, unit), `mg_layout` (70 % of screen, MG_MAX_ROWS), `mg_width`; single `mg_font` (ysize, cw) | amarchy/menu_geom.h:16-76 |
| Header and row draw | `draw_header` (mg_header / jc_header text, MT_HEAD_ROLE), `draw_row`, `draw_list`, `draw_full` | amarchy/gui.c:382-399, 440, 530, 547 |
| Window ticks without timer.device | `IDCMP_INTUITICKS` already handled in the gui loop (`ticks`, `ticking`) | amarchy/gui.c:120-121, :1200 |
| Job view text and header | `jc_header`, `jc_line`, footer state machine | amarchy/job_core.h:26-70 |
| Menu row parse | `[menu]` row = `label \| action`, split at the FIRST `\|`; no description field exists; `[bind]` rows use `\|` for the category | config/keymap.c:726-736, :672-684; config/keymap.h:13-17, km_menu :99-104 |
| Row conditions | `mn_cond(term, dopus, workbench)`, MN_COND_DOPUS / MN_COND_WB by group | amarchy/menu_core.h:46-60, amarchy/menu_tree.c:21 |
| DOpus / Workbench present | `upactive_dopus_present` (FindPort "DOPUS.1"), `upactive_workbench_present` (FindPort "WORKBENCH", OS 3.2 only) | handler/upactive.c:122, :132 |
| DOpus desktop mode | P1B-D1 `desk_dopus_mode()` NONE / OWN_SCREEN / WB_DESKTOP, signal per OD-B2 (open; recommended: DOPUS.1 present and `dopus screen` first word `Workbench`) | thoughts/shared/plans/2026-10-29-amarchy-p1b-third-party-theme.md:92, :171 |
| ARexx to DOpus | `rexx_send("DOPUS.1", ...)` | amarchy/rexx_host.h:41, amarchy/leaf.c:443-464 |
| OS version and CPU name | `a_info("os")`: exec lib_Version.Revision, AttnFlags -> 68000..68060 (static, inside the Claude client) | claude/sys_amiga.c:416-431; `lib_check` :390 |
| Chip/fast RAM | `AvailMem(MEMF_CHIP)`, `AvailMem(MEMF_FAST)` | proc/top_amiga.c:178 |
| Theme file parser | gtheme struct, gt_* parse; P1a R1.5 adds backdrop/font/wbtitle keys | amarchy/gthemes_core.h:12-47; p1a plan :110-112 |
| Existing About rows | `workbench.about` (WORKBENCH ARexx, OS 3.2) only; no Amarchy About, no logo code or asset in amarchy/ (grep about/logo) | dist/keymap:223 |
| Version strings | no `$VER` in amarchy/ (grep) | - |
| Menu screen | `LockPubScreen(0)`: the default public screen | amarchy/gui.c:1508 |
| Timer tick for reminders | NOT built: P2 branch worktree-agent-a2768f656aa0a7b7f (b3474a77) built the bar and agents segment only; its F2 findings say no clock segment, no `bar_usable`, no plan items ticked; no timer.device in amarchy.c | branch research 2026-10-29-omarchy-ai-f2.md:20-38 |

Search done before proposing modules: `grep -i about|logo|font` over amarchy/, config/keymap.*, dist/keymap; `AttnFlags|AvailMem|lib_Version` over the repo; logo/IFF assets over the tree (none).

## DOpus as the desktop: what changes per item

The mode comes from P1B-D1 `desk_dopus_mode()`; whichever of P1b or LX lands first builds it to the P1B-D1 spec (no second
detector). Every unverified claim below is a probe row in RL.0.

| Item | Workbench desktop | DOpus WB_DESKTOP (replaces Workbench) | DOpus OWN_SCREEN |
|---|---|---|---|
| RL.1 start logo | themed window centred on the Workbench screen | same screen; the DOpus desktop is a backdrop window there, the logo window must open above it and must not leave DOpus deactivated (probe P-D1) | on the default public screen (gui.c:1508), which may be Workbench or DOpus's screen (probe P-D2); owner decision OD-L5 |
| RL.2 logo file | UP-Term:Amarchy/ | same file | same file |
| RL.3-RL.5 font tiers | theme base | same; the DOpus screen font (P1B-D3 `dopus set font screen`) is NOT the menu font, no coupling | menu may open on a screen of another size or aspect; tiers scale from that screen's aspect (menu_screen_aspect) |
| RL.6 About | Workbench version from version.library / workbench.library (probe P-A1) | "Desktop: Directory Opus <version>" line from the DOpus version reply (probe P-D3); Workbench line still shows the ROM/disk Workbench release, marked "not running" | as WB_DESKTOP plus the Workbench line without the marker |
| RL.6 existing About Workbench row | `workbench.about` shown (MN_COND_WB) | hidden (no WORKBENCH port); the Amarchy About row is the only About | shown on 3.2 |
| RX.1 toggle rows | `checked:` env/file | same, plus a `port:` condition so rows can tick on DOpus state (OD-X1) | same |
| RX.2 launcher wizard, icon | icon = .info beside the program or a left-out on the Workbench desktop (probe P-W1) | DOpus has its own desktop folder (DOpus5:Desktop/, unverified, probe P-D4); the "put on desktop" choice writes there | as Workbench (Workbench desktop still runs) |
| RX.3 reminders | P4 notice on the front screen | same screen; notice above the DOpus backdrop (same probe as P-D1) | front screen (may be DOpus's) |
| RX.4 disk speed | DosList volumes | same; no DOpus involvement | same |
| Backdrop | P1a wbpattern tile | logo never drawn on the backdrop (DOpus owns the backdrop picture through P1B-D3) | - |

## Checklist (IDs from the roadmap; splits get a letter)

Each item: what, where, the test that proves it. Red first. [J] = waits for Phase J, [P2] = waits for R2.3, [P4] = waits for R4.9a/b, [B] = waits for P1B-D1.

### Phase L

- [ ] RL.0 Probe session (one rig, 060, purpose "Amarchy LX: logo/font/About probe"), plain-Workbench rig and DOpus rig in the same session, text output only, log to thoughts/shared/research/2026-10-xx-lx-probe.md:
  P-F1 bitmap fonts in FONTS: on the rig at sizes needed for tiers (e.g. base 8 -> 11/8/7): OpenDiskFont exact sizes vs diskfont scaling result (tf_YSize returned, FPF_DESIGNED flag);
  P-F2 TrueType (the A4 Prefs font) at three px sizes: open time and glyph cache memory per tier on 060;
  P-A1 version.library lib_Version/Revision, workbench.library version, exec SoftVer, on 3.1 and 3.2;
  P-A2 uptime source: RAM: volume dol_VolumeDate vs DateStamp (trustworthy after a warm reset?);
  P-D1 a gui window opened while DOpus owns the Workbench desktop: on top, active, DOpus still accepts input after close;
  P-D2 default public screen when DOpus runs on its own screen;
  P-D3 DOpus ARexx version reply (`dopus version` or the documented equivalent; exact verb unverified) text;
  P-D4 DOpus desktop folder path and whether a new .info there appears without `dopus refresh`;
  P-W1 Workbench `PutDiskObject` left-out icon vs icon beside the program: which shows on the desktop without a reset.
- [ ] RL.1 Start logo. Where: gui.c new open mode `logo` beside `viewer` (gui_open(viewer, node) gains the mode; the warm server path
  through act_menu_open / ms_open carries it), so the logo shares the gui's screen lock, pens, fonts and theme roles. The commodity
  asks for it once after startup succeeds (amarchy.c main, after the cx/rexx ports open); closes after OD-L2 seconds counted with
  IDCMP_INTUITICKS (no timer.device), or on any key or click. STATUS gains `logos=N` (counted in gui, published like the P2 `draws`
  counter). amconf key `logo-at-start` (OD-L3). Test: host test of the logo-mode timeout counter (ticks -> close at 2 s = 20 ticks);
  rig row l_start.
- [ ] RL.2 Logo file and core. `logo_core.c/.h` (portable C89): parse the logo file per OD-L1, size it to a box (cells per char from
  the font tier or picture scale), centre it. Shipped default `dist/Amarchy/logo.txt` (installed to UP-Term:Amarchy/logo.txt by the
  Installer, user may replace it); RJ.2 header band and RL.6 About read the same path through one `logo_path()`.
  Host tests tests/test_logo_core.c: empty/missing file -> text fallback "Amarchy"; ragged lines; box clipping; centring; oversize
  logo scales down not off-screen; revert proof (centring forced to 0 fails).
- [ ] RL.2b [J] RJ.2 header band calls the same draw (`draw_logo(g, box)`), not a copy. Test: rig row l_job (job view shows
  logos+1 when a job opens).
- [ ] RL.3 Three font tiers. gui holds `mt[3]` (MT_TIER_HEAD, MT_TIER_ROW, MT_TIER_DESC) opened in `pick_font` from one spec name and
  three sizes from `tier_sizes(base, ratios)` (pure, in menu_geom or a new tiny `font_tier_core`, decided at build after grep: prefer
  menu_geom.c). `mg_font` becomes per tier (mg_metrics takes head/row/desc fonts; head_h from the head tier, row_h from row + desc
  tiers). Bitmap fonts: nearest designed size if P-F1 says scaled glyphs are unreadable (OD-L4). Close all three in the close path
  (gui.c:1456). Host tests: tier_sizes for base 12 -> 16/12/11 (Omarchy numbers from roadmap RL.3) and base 8 -> 11/8/7 (rounding
  rule stated in the test); mg_layout with three tiers: head_h, row_h, visible on a 640x256 and a 640x512 screen; revert proof.
- [ ] RL.4 Description line. Keymap `[menu]` rows carry an optional description (syntax OD-L6); km_menu gains `desc`
  (pool string, never NULL); menu draws it under the label in the desc tier and MT muted role; row_h grows only on levels where a
  row has a description (OD-L7). Visible-row count on 256-line screens drops: assert the number in test_menu_geom (e.g. 640x256
  topaz 8 base: N rows before, M after; measured at build, written into the test). Ship descriptions for the root groups and
  every row whose label is not self-explaining (list made at build, owner sees it in the PR). Host tests: keymap parse with and
  without desc, an action that contains `|` still parses whole (OD-L6 guard), mg_width counts the desc width; existing keymap
  suite unchanged rows count.
- [ ] RL.5 Theme tokens `font-base` and `font-ratios` (header,row,desc in percent, default 133,100,92) in the gtheme parser, extending
  P1a R1.5 (same parser, same file; no second parser). Missing key = default; out of range clamped (base 6..32). Host tests in
  test_gthemes: every shipped theme parses; a theme with font-base 16 gives tiers 21/16/15. Precedence with the user's AM_FONT size:
  OD-L8.
- [ ] RL.6 About row. `about_core.c/.h` (portable): formats the About lines from a filled struct (kick ver/rev, wb ver/rev or
  "not running", desktop name+version, cpu name, chip/fast free and total, UP-Term and Amarchy versions, uptime seconds -> "2 days
  3 hours" full words). `about_dos.c` fills it: exec/version.library per P-A1, AttnFlags via ONE shared `cpu_name(attn)` moved out
  of claude/sys_amiga.c:428 into a common module both call (single source), AvailMem, RAM: date per P-A2, DOpus version per P-D3
  only when desk_dopus_mode() != NONE [B]. UP-Term and Amarchy versions: add `$VER` strings (none exist in amarchy/) from one
  version header generated by the Makefile from the release number. Row `about = About Amarchy | about:` (new KA_ABOUT kind) at the
  menu root, shown in a gui view: logo on top (RL.2 draw), lines below in the row tier. STATUS gains `abouts=N`. Host tests:
  test_about_core every line, 3.1 vs 3.2, DOpus present/absent, uptime words, chip-only machine (fast 0) wording; cpu_name table;
  revert proof.
- [ ] RL.7 Host tests (above) green with `make test ONLY=<suite>` (logo_core, about_core, keymap, menu_geom, gthemes), nice 19, max 2 workers;
  one rig reachability test tools/rig/amarchy_lx_rig.py, rows l_* (below).

### Phase X

- [ ] RX.1 Toggle rows. `[menu]` row option `checked:<cond>` where cond is `env:NAME` (GetVar non-empty and not "0"), `file:PATH`
  (Lock succeeds) and, if OD-X1 says so, `port:NAME` (FindPort). Parsed in config/keymap.c, evaluated once per open in gui (same
  moment as menu_cond, gui.c:1491), tick glyph drawn in the icon column (vector tick drawn with Move/Draw, no font glyph needed).
  Picking the row runs its action; the next open shows the new state. STATUS gains `checks=N` (rows evaluated true at last open).
  Ship rows: the existing on/off settings that already have an env var (list made at build). Host tests: parse, refusal of unknown
  cond kinds, evaluator with injected getvar/lock/findport.
- [ ] RX.2 [J] Add-launcher wizard. Row `apps.add = Add launcher | wizard:launcher`; RJ.3 input rows in sequence: name (text),
  command (text, file requester optional OD-X2), window style (choose one: UP-Term tab, own window, no window), icon (choose one:
  none, beside program, on desktop). Writes one `[menu]` row to the user keymap (keymap.user overlay, config/keymap.h:137), never to
  dist/keymap; duplicates refused with a message. Desktop icon goes to the Workbench desktop (P-W1) or the DOpus desktop folder
  (P-D4) by desk_dopus_mode() [B]. STATUS `launchers=N`. Host tests: row writer (escaping, `|` in the command per OD-L6 rule), name
  clash, keymap.user round trip.
- [ ] RX.3 [P2][P4] Reminders. `remind_core.c/.h`: list of (time, text), parse/save `ENVARC:Amarchy/reminders` (survives reboot),
  due-check on the R2.3 tick, fires through note_core directly (P4 Order note: not through the port). Menu: `reminders.list`
  (view), `reminders.add` (RJ.3 rows: text, time "in N minutes" or "at HH:MM"), delete from the list. STATUS `reminders=N fired=N`.
  Host tests: parse, due-check at boundaries, past-due after reboot (fires once, OD-X3), save round trip.
- [ ] RX.4 [J] Disk speed test. Row `system.diskspeed = Disk speed test | job:diskspeed`; choose-one volume (DosList volumes,
  writable only); `diskspeed_core.c` (sizes, KB/s math, result text) plus Amiga side writing and reading a temp file of OD-X4 size
  in 32 KB blocks timed with timer.device (UNIT_MICROHZ, opened by the tool, not the commodity tick), output into the job view,
  file deleted after. STATUS via the job view counters (`jobs`). Host tests: KB/s formatting, size clamps to free space, cleanup
  on cancel (job core cancel path).
- [ ] RX.5 Host tests above, and one rig test per row kind (rows x_* below), same script.

### Knobs and the case that proves each moves output

| Knob | Case |
|---|---|
| logo file (UP-Term:Amarchy/logo.txt) | l_start with a 3-line replacement: logo box height in the gui's published size changes (STATUS `logoh=`) |
| logo-at-start (OD-L3) | l_start_off: logos stays 0 after a commodity restart |
| logo time (OD-L2) | host tick counter; l_start window gone after 2 s and present at 1 s |
| theme font-base | l_tiers: theme with font-base 16 vs default; STATUS `tiers=h/r/d` pixel sizes differ as asserted |
| theme font-ratios | host tier_sizes; l_tiers second theme |
| row description | l_desc: level with descriptions: STATUS `rowh=` larger and `visible=` smaller than the same level without |
| About fields | host about_core; l_about lines include the rig's CPU "68060" and the desktop line per rig kind |
| checked: cond | x_toggle: setenv flips the tick count `checks` 0 -> 1 |
| reminder time | host due-check; x_remind fires after 60 s, not before |
| disk speed size (OD-X4) | host clamp; x_disk job output names the size used |

Exemption: none planned; any knob that cannot move a rig-visible number gets a written exemption in this table before build.

## Host tests (all `make test ONLY=<suite>`, nice 19, max 2 workers)

- logo_core: parse, fallback, clipping, centring, scale-down.
- menu_geom: tier sizes, three-tier layout, visible-row counts on 256/512-line screens.
- keymap: description field, `|` inside actions, `checked:` parse, `about:` / `wizard:` kinds, user-row writer.
- gthemes: font-base, font-ratios, clamps, every shipped theme parses.
- about_core + cpu_name: every line, 3.1/3.2, DOpus present/absent, uptime words.
- remind_core, diskspeed_core: as listed.

## The rig reachability tests (one script, rows share state)

tools/rig/amarchy_lx_rig.py, pattern of tools/rig/amarchy_menu_rig.py (table-driven, verdict per row in <RIG>/amarchy/lx_<row>.verdict,
`--only/--failed/--fresh`, `--desk wb|dopus`). Started with `UPTERM_RIG=<n> rig.py start --max --purpose "Amarchy LX: logo, fonts,
About, extras check (<desk>)"`. Entry points only: the commodity started by its real start line, the real super+space menu hotkey,
row picks by key press; ARexx only to read STATUS. Runs on a Workbench rig and on a DOpus WB-replacement rig; the DOpus own-screen
run follows OD-L5. Stops its rig at the end.

Phase L:
1. l_status: STATUS has `logos=0 abouts=0` before start completes (baseline proves a dead value reads 0).
2. l_start: start the commodity -> `logos=1`, window "Amarchy" logo exists (amiga_windows) at 1 s, gone at 3 s; on the DOpus rig DOPUS.1 still answers after it closes (P-D1).
3. l_tiers: open the menu -> STATUS `tiers=` three distinct sizes equal to tier_sizes(theme base); switch theme to the font-base 16 test theme, reopen -> new sizes.
4. l_desc: open the root -> `rowh=` and `visible=` equal the host-asserted numbers for the rig's screen.
5. l_about: pick About -> `abouts=1`, About view text (gui publishes its lines to the rig verdict file through the existing text-check path) contains "68060", the Kickstart version, and "Directory Opus" on the DOpus rig only.
6. l_job [J]: run a job row -> `logos` +1 (header band drew the logo).

Phase X:
7. x_toggle: setenv the test var, open the menu -> `checks` +1; unset -> back.
8. x_wizard [J]: walk the wizard with key presses -> `launchers=1`, keymap.user holds the row, the new row runs its command (hits +1); on the DOpus rig the icon lands in the DOpus desktop folder.
9. x_remind [P2][P4]: add a reminder "in 1 minute" -> at +65 s `fired=1`, `notes` +1, notice window exists.
10. x_disk [J]: run the disk speed test on RAM: -> job Done footer, output names KB/s for write and read, temp file gone.

## Dependencies

- Can start now: RL.0 probe, RL.2 core, RL.3 tier math and layout, RL.4 parser, RL.5 parser keys, RL.6 core, RX.1, host parts of RX.2-RX.4.
- Wait for AMARCHY-UHC (decision 17): landing order of Phase L on main; cores may be built on a branch before.
- Wait for Phase J: RL.2b, RX.2, RX.4 (job view, RJ.3 input rows), rig rows l_job, x_wizard, x_disk.
- Wait for P1b P1B-D1 (or build it here to its spec, once): DOpus lines in RL.6, RX.2 desktop icon.
- Wait for P2 R2.3 tick and P4 R4.9a/b: RX.3 and x_remind. The P2 branch (b3474a77) has no tick yet.
- Merge risk: STATUS arrays (amarchy.c:143-161) are edited by P2 (branch), P4 and P1b (P1B-I2). Add keys in one commit per phase after rebasing on whichever landed.

## Risks

- Three TrueType tiers on a 68020/030: three glyph caches and three opens per menu open; P-F2 measures; fallback is one TTF tier plus bitmap for desc.
- Bitmap fonts on a stock install have few sizes (topaz 8/9/11); scaled bitmap glyphs look poor. OD-L4.
- Taller rows cut visible rows on 256-line PAL screens (MG_MAX_ROWS and 70 % rule): descriptions may need hiding there (OD-L7).
- The logo window at boot races Workbench/DOpus startup (the commodity may start before LoadWB or DOpus finish); P-D1 plus a wait for the screen.
- Uptime has no exec source on OS 3.x; the RAM: date trick is unverified (P-A2).
- DOpus ARexx verbs for version and desktop folder are unverified (P-D3, P-D4); without them the About desktop line says "Directory Opus" without a number.
- Writing user keymap rows from the wizard can break the user's file; write to a temp file, parse it, then rename.

## Owner decisions (none taken silently; recommended first)

Owner answers 2026-10-29 (AskUserQuestion, s21): every decision took the recommended option (a). OD-L1 text cell-art logo in the accent pen; OD-L2 2 s or any key/click; OD-L3 amconf logo-at-start, on by default, Prefs row; OD-L4 nearest designed bitmap size per tier; OD-L5 default public screen when DOpus has its own screen; OD-L6 `Label | action || Description`, split on the last ` || `; OD-L7 taller rows only on levels with descriptions, hidden under 400 lines; OD-L8 user Prefs size wins when set, theme ratios still apply; OD-L9 About as the last root row; OD-X1 env:, file:, port: conditions; OD-X2 text row plus ASL file button; OD-X3 missed reminders fire once at next start, marked missed; OD-X4 1 MB disk test clamped to half the free space; OD-X5 ENVARC:Amarchy/reminders. OD-B2 dependency closed (P1b OD-B2 decided 2026-10-29). Status: decided.

- OD-L1 Logo format. (a) Text logo file, each non-space character drawn as a filled cell in the theme accent (Recommended): any text
  editor makes one, no chip RAM picture, scales with the font tier, follows every theme, works on RTG and AGA; trade-off: blocky,
  no real picture. (b) IFF ILBM through picture.datatype, remapped to the screen: a real picture; trade-off: chip RAM, colours fight
  the theme pens, OD6 (P1a) chose less memory. (c) Both, text default and IFF if the file is .iff: most flexible; trade-off: two paths.
- OD-L2 Logo time. (a) 2 s or any key/click (Recommended, the roadmap's "about 2 s"): trade-off: none at boot speed. (b) 1 s. (c) amconf key.
- OD-L3 Logo at start switch. (a) amconf `logo-at-start`, on by default, Prefs AMARCHY row "Logo at start" (Recommended): trade-off: one more row. (b) Always on.
- OD-L4 Bitmap font tiers. (a) Nearest designed size in FONTS: per tier, the same size for two tiers when nothing smaller exists (Recommended): crisp; trade-off: tiers may collapse on a stock install. (b) diskfont scaling to the exact size: exact ratios; trade-off: scaled glyphs. (c) Tiers only with TrueType.
- OD-L5 Logo screen when DOpus runs on its own screen. (a) The default public screen, as the menu (Recommended): one rule for logo and menu; trade-off: may land on Workbench while the user looks at DOpus. (b) The front screen. (c) No start logo in that mode.
- OD-L6 Description syntax in `[menu]` rows (the action is everything after the first `|` today, actions may contain `|`). (a) A third field after ` || ` (double bar): `id = Label | action || Description` (Recommended): no existing row has `||`, keeps one file; trade-off: an action containing `||` must not be split (parser splits on the LAST ` || ` with spaces). (b) A separate id-keyed file `dist/menu-desc` like dist/menu-icons: no parse risk; trade-off: two files to keep in step, and the roadmap said the keymap field. (c) Description in the label part: `Label ; Description | action`; trade-off: `;` starts comments in [bind].
- OD-L7 Row height with descriptions. (a) Taller only on levels that have a description, and descriptions hidden when the screen is under 400 lines (Recommended): keeps 256-line PAL usable; trade-off: PAL users never see descriptions. (b) Always shown, scroll more. (c) Description only for the selected row (Omarchy-like hint line): fixed height; trade-off: less to read at once.
- OD-L8 Font size precedence. (a) The theme base wins unless the user set a size in the Prefs "Menu font" row, then that size is the base and the theme ratios still apply (Recommended): user's choice is never overridden; trade-off: theme base invisible for those users. (b) Theme base always wins over AM_FONT size. (c) AM_FONT always, theme base unused when a font is set.
- OD-L9 About placement. (a) A root row "About" at the end of the root, opening a gui view with the logo (Recommended, Omarchy root order): trade-off: one more root row on short screens. (b) Under System.
- OD-X1 Toggle conditions. (a) env:, file:, port: (Recommended): port: lets DOpus or AREXX-port state show; trade-off: a FindPort per row per open (cheap). (b) env: and file: only, as the roadmap names.
- OD-X2 Wizard command input. (a) Text row plus an ASL file requester button (Recommended): trade-off: one more input kind for RJ.3. (b) Text only.
- OD-X3 Reminder missed while switched off. (a) Fire once at the next start, marked "missed" (Recommended): trade-off: a burst of notices after a long break. (b) Drop missed ones.
- OD-X4 Disk speed file size. (a) 1 MB, clamped to half the free space (Recommended): quick on floppy-free systems; trade-off: cache-skewed on big CF/SSD. (b) 4 MB. (c) User picks 256 KB / 1 MB / 4 MB.
- OD-X5 Reminder storage. (a) ENVARC:Amarchy/reminders (Recommended): survives reboot like prefs; trade-off: ENVARC: copied to ENV: at boot (small file). (b) UP-Term:Amarchy/reminders.

## Order

RL.0 probe (one rig session, both desks) -> RL.2, RL.3, RL.4, RL.5, RL.6 cores and host tests (host only) -> [UHC lands] -> RL.1,
RL.3/RL.4 gui wiring, RL.6 view -> l_* rig rows (WB + DOpus) -> [J lands] RL.2b, RX.2, RX.4 -> RX.1 any time after RL.4 parser ->
x_toggle, x_wizard, x_disk -> [P2 R2.3 + P4 R4.9] RX.3 -> x_remind -> knob table check.
