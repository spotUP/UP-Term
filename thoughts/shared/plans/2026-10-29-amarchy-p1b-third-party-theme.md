---
date: 2026-10-29
topic: AMARCHY-P1B
status: draft
author: spotUP
ledger: thoughts/spot/todos.md rows AMARCHY-DESK-DOPUS (3315), AMARCHY-DESK-MUI (3316), AMARCHY-DESK-VISUALPREFS (3319), AMARCHY-PLANS (3330 item 3)
depends_on: plan-p1a.md (R1.1-R1.22, desk_core / desk_apply)
research: thoughts/shared/research/2026-10-29-amarchy-visualprefs-birdie.md, thoughts/shared/plans/2026-10-25-amarchy-dopus-integration.md, thoughts/spot/term-findings/dopus-probe-2026-10-25.md
---

# Amarchy Phase 1b: DOpus desktop, MUI (Selecta), flat GUI skin patches (R1.23-R1.25)

## Goal

One Amarchy theme switch (`DESK THEME <name>` on the AMARCHY ARexx port, the menu, or the hotkey) themes three more surfaces, each as a consumer of the P1a theme-apply entry point:

- R1.23: the Directory Opus 5 (Magellan II) desktop, when DOpus replaces or runs beside Workbench, gets the same palette and pens as the Workbench desktop.
- R1.24: MUI apps get a flat theme through Selecta, read once at window open, per app on its own screen. Waits on Selecta.
- R1.25: the system GUI goes flat through VisualPrefs + VP-QuickFix 1.5n (gui.prefs), FullPalette pen locking (fullpalette.prefs), and Birdie border fills (CLI args). Each of the three is its own Amarchy option, default off.

Done (reachability): one rig test drives `DESK THEME <name>` through the AMARCHY port, proves each new target ran via STATUS counters, and asserts the written bytes / queried pens equal the theme roles; with every new option off, the same test proves no file and no Startup-sequence line was touched.

## Interface consumed (from P1a)

P1a creates, and this plan only adds rows to:

- `amarchy/desk_core.[ch]`: pure `desk_plan(const gtheme *, const desk_opts *, desk_out *)` and the target table D-TGT, rows `{ name, amconf key, files[], plan_fn, apply_kind }`. P1a rows: palette, wbpattern, pointer, font, workbench. P1a states "R1.23-R1.25 add rows later". P1a test contract: a target that is off writes nothing and is absent from the snapshot list.
- `amarchy/desk.c` `desk_apply`: snapshot to T:amarchy-revert/, write ENV:, live nudge, confirm requester (15 s timer in the Wait loop, amarchy.c:332), revert, ENVARC: persist on confirm, ENVARC:Amarchy/pending-revert crash marker, boot restore.
- Triggers: ARexx `DESK THEME <name>` (verbs at amarchy.c:206-213), hotkey path amarchy.c:180.
- STATUS counters desks / reverts / confirms / restores (STATUS reply built at amarchy.c:126, :143, :224).
- Options: amconf keys (amarchy/amconf_core.h:12-26 AM_* enum; save pattern gthemes_dos.c:57-59) and the PG_PAGE_AMARCHY rows in prefs/settings_def.c. P1a OD2: master switch plus per-target switches, default off.

This plan needs one extension to the P1a contract (P1B-I1): a new `apply_kind` set beyond P1a's ENV + IPrefs nudge:

| apply_kind | Meaning | Snapshot / revert | Live? |
|---|---|---|---|
| `AK_AREXX` | send commands to a running port, query back | query the old state into T:amarchy-revert/<target>.rexx before set | yes |
| `AK_FILE_NEXT_OPEN` | write a file read by the consumer at its next window open | copy old file | at next open |
| `AK_FILE_REOPEN` | write a prefs file, then reopen the WB screen (VisualPrefs) | copy old file | yes, if P1B-R3 proves headless live apply |
| `AK_RESTART` | kill and restart a resident patch with new args (Birdie) | old args line | yes, if P1B-R5 proves clean restart |
| `AK_REBOOT` | file takes effect after reboot only (FullPalette lock if not live) | copy old file | no: confirm requester says "after reboot", no 15 s auto-revert, P1a OD3 non-live path |

Each new row also declares `startup_block` (NULL or the name of an S:User-Startup / Startup-sequence ;BEGIN/;END block the option owns), so the "off" path can remove exactly that block and nothing else.

## Reuse map

| Need | Reuse | Where |
|---|---|---|
| DOpus palette + pens commands from theme roles | `menu_theme_dopus_commands` (D14, built, tested, not yet called from any entry) | amarchy/menu_theme.c:77; test `menu_theme_formats_the_dopus_palette_and_pens_commands` in tests/test_keymap.c |
| Theme roles | `menu_theme_roles_fb` :29, `menu_theme_roles` :44, `menu_theme_roles_gui` :49 (MT_BG 0, MT_TEXT 7, MT_DIM 8, MT_ACCENT 14, MT_SELBOX 4) | amarchy/menu_theme.c |
| DOpus present / Workbench present | `upactive_dopus_present` (FindPort "DOPUS.1") :132, `upactive_workbench_present` (FindPort "WORKBENCH", 3.2 only, absent when DOpus replaces WB) :122 | handler/upactive.c; used at amarchy/gui.c:1491, menu.c:474, amarchy.c:124 via `mn_cond` (amarchy/menu_tree.c:21) |
| Send ARexx to DOpus | `rexx_send` | amarchy/rexx_host.h:41; usage pattern amarchy/leaf.c:443-464 (rexx_send("DOPUS.1", ...)) |
| Restore-row design for DOpus | D15 (open): dopus.theme + restore rows, save to UP-Term:Dopus/colours.saved | thoughts/shared/plans/2026-10-25-amarchy-dopus-integration.md |
| Big-endian IFF write / read | put32 / get32 | handler/clipfmt.c:70, :78 |
| Current pens / palette | GetRGB32 | handler/vtcon_handler.c:2633, amarchy/gui.c:198 |
| Requester | EasyRequestArgs | handler/vtcon_handler.c:1824 |
| Option storage | amconf AM_* keys, amconf_save | amarchy/amconf_core.h:12-26, gthemes_dos.c:57-59 |
| Prefs page rows | PG_PAGE_AMARCHY rows | prefs/settings_def.c |
| ;BEGIN/;END Startup block editing (Installer side, the model to follow) | Install.installer / Uninstall blocks | dist/Install.installer:86, dist/Uninstall:101, dist/README.txt:100, :913, :1090, :1107 |
| Rig test skeleton | amarchy menu rig; `ami.read_file`; IFF reader | tools/rig/amarchy_menu_rig.py:1-30, tools/rig/ami.py:518, tools/rig/keymap_rig.py:36 |
| P1a rig test to extend | RX1 | tools/rig/amarchy_desk_rig.py (created by P1a) |
| Selecta theme format and strict parser | `txt_theme_parse`; tokens | ~/Code/MusicPlayer src/uimodel/theme.c:211, include/selecta/uimodel/tokens.h:15-40 |

Searched and not found (so new code is justified): no C helper in the tree edits a ;BEGIN/;END block in S:User-Startup at run time (only the Installer script and Python rig/test code touch them; `git grep ';BEGIN'` hits only tests/*.py and tools/rig/*.py). P1B-C1 adds one, in amarchy/desk_core.c, pure, shared by all three R1.25 rows. No gui.prefs / fullpalette.prefs writer exists. No Selecta writer exists.

## Checklist

IDs are stable. Probes come first; no implementation item starts before the probe that decides its shape reports. Every probe runs on a rig as 060 (`UPTERM_RIG=<n> rig.py start --max --purpose "Amarchy P1b: <probe>"`), never rig 1, rig stopped at the end, findings written to thoughts/spot/term-findings/amarchy-p1b-<probe>-<date>.md. Roadmap decision 10: one rig probe session first, all P1B-R items batched in it where the rig state allows.

### Research / probes (rig)

- [ ] **P1B-R1 gui.prefs GUI chunk map by save-diff.** On a 3.2 rig with VP 1.5k + VP-QuickFix 1.5n installed (all Installer options ticked), save gui.prefs from the editor once as base (or from Prefs/Presets/GUISample.pre), then change exactly one option per save and diff the 1672-byte (0x688) "GUI " chunk. Options to map, minimum set for flat: frame style (Borderless / Filled), symbol style (Flat / Future), shine pen, shadow pen, fill pen, background pen, Full 3D titlebar, Text 3D, "Don't optimize border rendering", every DrawInfo pen slot and its Absolute/Relative flag. Output: offset table (offset, width, meaning, values) in the findings file and as a fixture `tests/fixtures/guiprefs/*.pre` (own saves only, no third-party preset bytes). Also record GVER value (4) and PRHD bytes.
- [ ] **P1B-R2 FullPalette lock flags.** Same rig, FullPalette 40.22 with FPPrefs after IPrefs. Lock one pen, save, diff ENV(ARC):Sys/fullpalette.prefs (FORM PREF / PRHD / FPAL 0xC08 = 8-byte header + 256 x 3 x 32-bit). Find where lock flags live (sample header was zeros: lock may be a separate chunk, header bits, or not in the file at all). Confirm file path and whether env FullPalette/Action, ColorMode, CopyColors matter. Record whether a changed file is picked up live (re-run FPPrefs?) or only after reboot.
- [ ] **P1B-R3 VisualPrefs headless live apply.** Can a written gui.prefs be applied without the editor GUI? Check `GUI ?` template and tooltypes (FONTNAME, SIZEBBOTTOM, NOPALETTEPREFS, any USE/SAVE/FROM switch), then try: write ENV:Sys/gui.prefs + editor run with a USE-like argument; write + IPrefs nudge; write + close/reopen WB screen. Record which one changes DrawInfo pens of a freshly opened window (read pens with GetRGB32 / DrawInfo dump). If none: R1.25 VisualPrefs becomes AK_REBOOT.
- [ ] **P1B-R4 FullPalette and VisualPrefs order + coexistence.** Confirm working order VP (before IPrefs), IPrefs, FPPrefs, Birdie; confirm VP "Don't optimize border rendering" ON, Background pen 0, env VisualPrefs/BirdieMode=NOBITMAPS before VP starts. Record any guru (Stack-first rule: first repro under Stack 262144).
- [ ] **P1B-R5 Birdie on 3.2 and clean restart.** Run Birdie 2000 on the 3.2 rig with flat args (same start/end colour, LRA..DBA=0, NOBITMAPS) on an 8-bit AGA screen and on an RTG screen if the rig has one. Verify: (a) runs on 3.2 at all, (b) LRA..DBA=0 removes bevel, (c) 8-bit gives flat fill only, (d) Birdie can be signalled to quit (Ctrl-C via `Break`, or its own QUIT arg) and unpatches cleanly, (e) restart with new args takes effect on new windows without reboot. Run 50 kill/restart cycles; any crash makes Birdie AK_REBOOT.
- [ ] **P1B-R6 DOpus desktop detection.** On a rig with DOpus 5.91+ (kit 5.104) as WB replacement and on one with DOpus beside Workbench: record FindPort("DOPUS.1"), FindPort("WORKBENCH"), `dopus screen` reply (probe doc saw `Workbench 640 256 8 10 444 574` on rig 7), S:User-Startup ;BEGIN DOpus5 block presence, and LoadWB line state. Decide which signal tells "DOpus owns the Workbench desktop" vs "DOpus runs in its own screen" (owner decision OD-B2).
- [ ] **P1B-R7 DOpus ARexx apply on the Workbench screen.** On the WB-replacement rig: `dopus query palette` (save), `dopus set palette` with the D14 output, `dopus set pens ...`, `dopus refresh all`, then `dopus query palette` / `dopus query pens` equal what was sent. Record which of the 16 palette slots DOpus actually changes on the Workbench screen (guide: first 8 system colours only on its own screen, last 8 user pens always). Also test `dopus set background <file> desktop tile` + `refresh background` with the P1a wbpattern file, and `dopus set font screen` for the P1a font row. Also test LoadTheme / SaveTheme internal commands via `dopus command` to judge the files route (OD-B3).
- [ ] **P1B-R8 DOpus vs FullPalette.** With FullPalette locks on and DOpus user pens set, check whether FullPalette lock and DOpus allocation fight (DOpus user pens 9-16 allocate screen pens). Record pen numbers both sides.

### Interface extension (host, pure)

- [ ] **P1B-I1 apply_kind set.** Add AK_AREXX, AK_FILE_NEXT_OPEN, AK_FILE_REOPEN, AK_RESTART, AK_REBOOT and the `startup_block` field to D-TGT in amarchy/desk_core.h; desk_apply dispatches per kind. AK_REBOOT targets skip the 15 s auto-revert and show "takes effect after reboot" in the confirm requester (P1a OD3 path). Host test: plan with only AK_REBOOT rows sets `needs_reboot` and no live step.
- [ ] **P1B-I2 STATUS counters per new target.** Extend the STATUS reply (amarchy.c:126/143/224) with `dopus=N selecta=N vp=N fpal=N birdie=N` (count of successful applies per target) so the rig test has a sentinel for each row. Host test for the formatter.

### Shared helper

- [ ] **P1B-C1 Startup block editor (pure).** `desk_startup_block_set(const char *text, const char *block, const char *body, anchor, char *out, size_t cap)` and `..._remove(...)`: insert / replace / remove one `;BEGIN Amarchy-<name>` ... `;END Amarchy-<name>` block in S:User-Startup text, anchored before or after a given line (VisualPrefs before IPrefs means Startup-sequence, not User-Startup: P1B-R4 decides the file; if it must be S:Startup-sequence, the editor touches only the named block and refuses if the anchor line is missing). Never touches lines outside its own block. Host tests: insert, replace, remove, missing anchor refuses, foreign blocks untouched, CRLF/LF preserved, idempotent re-apply.

### R1.23 DOpus desktop

- [ ] **P1B-D1 detection.** `desk_dopus_mode()` returning NONE / OWN_SCREEN / WB_DESKTOP using the signal OD-B2 picks (reuse upactive_dopus_present / upactive_workbench_present; add `dopus screen` reply parse if needed, pure parser with host test on the rig-7 reply line).
- [ ] **P1B-D2 D-TGT row `dopus`.** amconf key AM_DESK_DOPUS (default off), apply_kind AK_AREXX, plan_fn calls `menu_theme_dopus_commands` (reuse, no new formatter) and appends `dopus refresh all`. Snapshot: `dopus query palette` + `dopus query pens <each>` to T:amarchy-revert/dopus.rexx (this is D15's restore set; save copy on confirm to UP-Term:Dopus/colours.saved per D15). Revert = replay snapshot. Skipped silently (and counted as skipped, not failed) when D1 says NONE.
- [ ] **P1B-D3 background and font follow.** If P1B-R7 shows they work: same row emits `dopus set background <P1a wbpattern file> desktop tile` and `dopus set font screen <P1a font>` so DOpus desktop matches the Workbench rows. Each is a sub-switch only if the owner wants it (OD-B4).
- [ ] **P1B-D4 close D15.** Mark DOpus plan D15 done by this row, link from the DOpus plan, ledger row 3315.
- [ ] **P1B-D5 host tests.** plan with dopus off writes nothing and is absent from snapshot; dopus on + mode NONE gives zero commands; dopus on + WB_DESKTOP gives exactly the D14 lines + refresh; snapshot parser round-trips a `dopus query pens` reply.

### R1.24 MUI via Selecta (waits on Selecta)

- [ ] **P1B-M1 (waiting) Selecta hook landed.** Selecta reads ENV:Selecta/theme in its --theme format. Not landed. Every M item below waits on it, except M2's pure part.
- [ ] **P1B-M2 Selecta theme writer (pure).** `desk_selecta_render(const gtheme *, char *out, size_t cap)` emitting only names present in Selecta's token list (tokens.h:15-40: accent, surface, text, background, surface_raised, surface_hover, surface_pressed, text_muted, text_disabled, accent_text, divider, focus_ring, error, warning, success, scrim, frame, panel, header_tint, track, hover_light, plus radius/spacing kept at Selecta defaults by not emitting them) and role lines (normal, header, cursor, directory, file, playing, dim, label, value, border, error) as `fg bg`. Flat: radius tokens 0 only if owner picks it (OD-M2). Host test: output passes a vendored copy of Selecta's `txt_theme_parse` (strict: one unknown name fails the whole file) and the contrast gate `mui_theme_readable` >= 40 for text on surface and accent_text on accent.
- [ ] **P1B-M3 (waiting) D-TGT row `selecta`.** amconf AM_DESK_SELECTA default off, AK_FILE_NEXT_OPEN, file ENV:Selecta/theme (+ ENVARC: on confirm). Confirm requester text says "applies to MUI windows opened from now on".
- [ ] **P1B-M4 (waiting) version skew guard.** Read Selecta's version (or a `tokens` query if Selecta gains one) and emit only the token set that version accepts; until Selecta's spec is stable (I667) pin the token list to one Selecta release and test against it.

### R1.25 flat GUI skin patches

Each of VisualPrefs, FullPalette, Birdie: its own amconf key (AM_DESK_VP, AM_DESK_FPAL, AM_DESK_BIRDIE), default off, its own PG_PAGE_AMARCHY row. Off = Amarchy restores the stock look it snapshotted, removes only its own startup block, and from then on never writes that patch's files nor touches its Startup lines.

- [ ] **P1B-V1 install detection.** Pure check from file presence: C:VisualPrefs + LIBS:/select.gadget (VP), C:FPPrefs or path from R2 (FullPalette), C:birdie (Birdie). Row disabled with reason text in prefs when the patch is not installed. How the patch gets there is OD-V1.
- [ ] **P1B-V2 gui.prefs writer (pure).** `desk_guiprefs_render(const gtheme *, const unsigned char *base, size_t, unsigned char *out)`: copy the user's current gui.prefs (or the own-save fixture if none), patch only the offsets from P1B-R1 for the flat set (Borderless/Filled frames, Flat symbols, shine = shadow = fill pen, Full 3D titlebar off, Text 3D off, Don't optimize border rendering on when Birdie is on, Background pen 0, DrawInfo pens as Absolute palette indices mapped from theme roles, never pens 0-3 or last 4 fixed slots). Uses put32/get32. Host test: one fixture per mapped offset proves the knob moves exactly those bytes; unknown bytes unchanged.
- [ ] **P1B-V3 D-TGT row `visualprefs`.** apply_kind from R3 (AK_FILE_REOPEN or AK_REBOOT), file ENV:Sys/gui.prefs, startup_block `Amarchy-VisualPrefs` (`Run >NIL: C:VisualPrefs` before IPrefs) only if the user has no VisualPrefs line already (then leave their line alone and say so in prefs). Sets env VisualPrefs/BirdieMode=NOBITMAPS when Birdie row is on.
- [ ] **P1B-F1 fullpalette.prefs writer (pure).** Writes FPAL 256 x 3 x 32-bit from the theme palette for the pens the theme uses, lock flags per P1B-R2 for exactly those pens. Host test from R2 fixture.
- [ ] **P1B-F2 D-TGT row `fullpalette`.** apply_kind per R2, file per R2, startup_block `Amarchy-FullPalette` (FPPrefs after IPrefs) under the same "user line already present" rule.
- [ ] **P1B-B1 Birdie args builder (pure).** `desk_birdie_args(const gtheme *, depth, char *out)`: `ACTIVECOLOR`, `PATTERNS g.RRGGBB.RRGGBB` with start = end for flat, `LRA..DBA 0` (if R5 confirms), NOBITMAPS, MINDEPTH/PUBSCREENS per R5. 8-bit screens: flat fill only. Host test: every theme role used appears in args, start == end.
- [ ] **P1B-B2 D-TGT row `birdie`.** apply_kind per R5 (AK_RESTART or AK_REBOOT), startup_block `Amarchy-Birdie` (`run <>NIL: C:birdie <args>` after IPrefs/FPPrefs). Snapshot = old block text.
- [ ] **P1B-V4 off path test (host).** For each of vp / fpal / birdie: toggle on, apply, toggle off: plan restores snapshot files, removes only its block, and after that a second apply with the row off writes no file and produces no startup edit (desk_out lists nothing for it).

### Prefs page

- [ ] **P1B-P1 PG_PAGE_AMARCHY rows.** Desktop: DOpus desktop (on/off), Selecta MUI theme (on/off, greyed "waiting for Selecta" until M1), VisualPrefs flat GUI, FullPalette pen lock, Birdie borders (each on/off, greyed with reason when not installed). Full English labels.

### Reachability (one rig test)

- [ ] **P1B-X1 extend RX1 (tools/rig/amarchy_desk_rig.py), one test `test_desk_theme_reaches_p1b_targets`.** Rig as 060, purpose "Amarchy P1b: desk theme reaches DOpus, VisualPrefs, FullPalette, Birdie". Setup: kit DOpus as WB replacement, VP + QuickFix, FullPalette, Birdie installed. Steps:
  1. All P1b options off; send `DESK THEME <A>`; assert STATUS dopus/vp/fpal/birdie counters 0, ENV:Sys/gui.prefs and fullpalette.prefs md5 unchanged, S:User-Startup / Startup-sequence md5 unchanged, `dopus query palette` unchanged.
  2. Turn each on (amconf keys via the prefs ARexx/ENV path); `DESK THEME <A>`; assert each counter went +1; `dopus query palette` / `query pens` equal D14 output for theme A; gui.prefs mapped offsets equal theme A values (IFF read, keymap_rig.py:36 reader); fullpalette.prefs FPAL triples and lock flags equal theme A; Birdie block args equal B1 output; DrawInfo pens of a new window equal the mapped pens (only if R3 says live).
  3. `DESK THEME <B>`: same asserts move to theme B values (every knob moves output).
  4. Let the 15 s confirm time out: revert counter +1, all files back to step-2 bytes, DOpus palette back.
  5. Turn all off: stock bytes restored, own blocks gone, STATUS counters stop moving on the next `DESK THEME`.
  Selecta part added as a step 2b when M1 lands (reads ENV:Selecta/theme bytes; MUI window open check is visual, owner test).

## Host tests (summary)

All in tests/test_desk_core.c (the P1a host test file) unless noted: I1 apply_kind dispatch and needs_reboot; I2 STATUS formatter; C1 startup block editor (7 cases); D5 DOpus plan (4 cases); M2 Selecta writer vs vendored strict parser + contrast gate; V2 gui.prefs per-offset fixtures; F1 FPAL writer; B1 Birdie args; V4 off path for each patch. Run only these files (rule 6a), capped workers.

## Risks

- **Undocumented gui.prefs.** 1672 bytes, no spec. Mitigation: patch only offsets proved by R1 diffs, copy all other bytes from the user's own file. A wrong offset can give unreadable windows: confirm requester + 15 s auto-revert covers live; boot restore marker covers a reboot-only apply.
- **VP-QuickFix is a hack ("use at own risk").** A bad gui.prefs on 3.2 can draw all pens zero. Mitigation: always write a complete gui.prefs (QuickFix fixes pens-zero only when ENVARC:Sys/gui.prefs is absent, so never delete it, only replace).
- **FullPalette lock flags unknown.** R2 may find no lock data in the file; then F1 writes palette only, and lock comes from a different mechanism or is dropped (owner told).
- **Birdie unload on 3.2 unverified.** Restart may crash; then AK_REBOOT, which loses live preview for borders.
- **DOpus user pens vs FullPalette locks.** DOpus allocates user pens 9-16; locked pens can collide. R8 measures; mitigation may be mapping DOpus user pens to theme-locked pen numbers.
- **DOpus palette on WB screen.** Guide says first 8 system colours only apply on DOpus's own screen; on the WB desktop only user pens may change. Then the P1a palette row already covers system colours and DOpus row sets pens only. R7 decides.
- **Selecta strict parser.** One unknown token fails the whole theme: version skew between Amarchy and Selecta silently loses MUI theming. M4 guard + vendored parser test.
- **Startup-sequence edits.** VisualPrefs must run before IPrefs, which may sit in S:Startup-sequence, not User-Startup. Editing Startup-sequence risks an unbootable system. Mitigation: only named Amarchy blocks, refuse when anchor missing, backup S:Startup-sequence to T:amarchy-revert/ and ENVARC:Amarchy/ before edit, boot-safe ordering verified on rig before ship.
- **Licences unclear** (VP freeware without redistribution clause, FullPalette none, Birdie none): see OD-V1.
- **Every Amarchy program needs __stack + $STACK cookie**: no new program here; if P1B-R3 needs a helper binary for headless VP apply, it gets both.

## What waits on Selecta

P1B-M1, M3, M4, and the X1 Selecta step 2b. M2's pure writer and its parser test can be built now against Selecta's current tokens.h, but ship only after M1 so the token list matches the version that reads ENV:Selecta/theme. Ledger row 3316 stays blocked.

## Order

1. P1B-R1..R8 in one probe rig session (two rigs if the WB-replacement DOpus setup and the VP/FullPalette/Birdie setup cannot share one).
2. Owner decisions below answered (some depend on R results; ask after the probe report).
3. P1B-I1, I2, C1 (interface + helper), then D1-D5 (DOpus, smallest, mostly reuse), then V1-V4 / F1-F2 / B1-B2, then P1.
4. P1B-X1 rig test.
5. M2 any time; M1/M3/M4 when Selecta lands.

## Owner decisions

Fixed already, not reopened: both Workbench and DOpus desktops supported; each skin patch is its own option, default off; off restores stock and Amarchy then never writes their files or Startup lines; Amarchy designs its own flat theme (amiga-look.org node/787 not a base); Selecta theme per app on own screen, read once at window open; Selecta reads ENV:Selecta/theme in --theme format (pending).

Open (each to be asked with AskUserQuestion, recommended first):

- **OD-V1 Bundle vs link the skin patches.** (a) Recommended: do not bundle; Amarchy Installer offers to fetch VP 1.5k, VP-QuickFix 1.5n, FullPalette 40.22, Birdie 2000 from Aminet (or tells the user where), Amarchy only detects them. Trade-off: no licence risk, but needs network or a manual step. (b) Bundle in the kit after written OK from the authors (VP author contact tanti@intercom.it). Trade-off: one-step install, but blocked on replies, and FullPalette/Birdie have no clear contact. (c) Bundle now, citing freeware. Trade-off: fastest, real redistribution risk for FullPalette and Birdie.
- **OD-V2 Who owns the Startup lines when the user already has VisualPrefs/FPPrefs/Birdie lines.** (a) Recommended: leave user lines untouched, Amarchy only writes prefs files/args and says "your own startup line is used". Trade-off: Birdie args then can not be changed by Amarchy (Birdie has no prefs file), so Birdie row stays read-only for such users. (b) Comment out the user line and add an Amarchy block, restored on off. Trade-off: full control, but edits user lines, which the fixed decision only allows for Amarchy's own lines.
- **OD-V3 VisualPrefs line in S:Startup-sequence.** VP must start before IPrefs, which is in Startup-sequence on 3.2. (a) Recommended: Amarchy block in Startup-sequence directly before the IPrefs line, with backup and anchor check. Trade-off: correct order, but edits the boot-critical file. (b) Ask the user to add the line by hand (Amarchy shows it). Trade-off: no boot-file edit, worse UX. (c) Only support VP if the user's IPrefs already runs from User-Startup. Trade-off: rare setup, mostly unusable.
- **OD-V4 Reboot-only targets in live preview.** If R2/R3/R5 show no live apply: (a) Recommended: apply on confirm only, requester says "after reboot", no 15 s auto-revert, boot restore marker armed until next successful boot. Trade-off: no preview for those surfaces. (b) Exclude reboot-only targets from theme switching (set once in prefs). Trade-off: theme switch is partial.
- **OD-B2 DOpus desktop detection.** DECIDED 2026-10-29 by the owner (asked as P3 D11): option (a), DOPUS.1 present and `dopus screen` first word `Workbench`; cached, not polled. (a) Recommended: DOPUS.1 port present and `dopus screen` reply's first word is `Workbench`. Trade-off: one ARexx round-trip, but exact. (b) DOPUS.1 present and WORKBENCH port absent (existing upactive helpers, no ARexx). Trade-off: free, but WORKBENCH port exists only on 3.2, so wrong on 3.1. (c) Parse S:User-Startup for the ;BEGIN DOpus5 block. Trade-off: says installed, not running.
- **OD-B3 DOpus theme via ARexx vs files.** (a) Recommended: ARexx `dopus set palette/pens/background/font` + `refresh` (D14 reuse, live, reversible by query snapshot). Trade-off: lost at DOpus restart unless DOpus saves its environment; Amarchy re-applies at boot restore. (b) Write a .theme in D5Themes: and run `LoadTheme APPLYPALETTE APPLYFONTS APPLYBACKGROUNDS`. Trade-off: survives restart, but .theme/ENVR format is unverified (no upstream source checked out) and needs R7 to prove. (c) Patch DOpus5:Environment/ ENVR chunk. Trade-off: persistent, highest corruption risk on an unverified struct.
- **OD-B4 DOpus background and font follow Workbench rows.** (a) Recommended: yes, same sub-switches as the P1a wbpattern and font rows (one switch drives both desktops). Trade-off: simpler prefs, user cannot keep a different DOpus wallpaper. (b) Separate DOpus switches. Trade-off: more rows.
- **OD-B5 DOpus own-screen mode.** When DOpus runs in its own screen (not the WB desktop): (a) Recommended: apply the same row there too (P4 already scoped own-screen DOpus; one code path). Trade-off: changes a screen the user may theme separately. (b) Only the WB-desktop case. Trade-off: two DOpus looks on one machine.
- **OD-M2 Flat MUI radius.** (a) Recommended: emit radius, radius_small, radius_large = 0 for the flat look. Trade-off: fully flat, but locks Selecta out of its own rounded default. (b) Leave radius to Selecta. Trade-off: MUI not fully flat.
- **OD-F1 FullPalette locked set.** (a) Recommended: lock only the pens the theme roles use. Trade-off: other apps can still take free pens. (b) Lock all theme palette entries. Trade-off: apps that need free pens (DOpus user pens, MUI) may fail to allocate.
- **OD-X1 Rig test setup.** (a) Recommended: one rig image with DOpus WB replacement + all three patches, one X1 test. Trade-off: one setup, but a patch crash masks the others. (b) Two rigs (DOpus / skin patches). Trade-off: isolates failures, two images to maintain.
