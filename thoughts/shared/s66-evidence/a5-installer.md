---
date: 2026-11-09
topic: A5 INSTALLER-WINDOW-CUT-OFF + INSTALLER-DIALOG-NO-BORDERS (s66 hunt)
status: not reproduced on HEAD f7d1250c; root cause = already-fixed FLAT-ABSENT-AT-BOOT (old binary on rig 94)
---

# A5 Installer window bottom + button borders, s66

## Verdict
Not reproduced on HEAD f7d1250c. No code change, no commit.

## Setup measured
- Rig 168, purpose "up-term-bb s66: A5 installer window cut off", `--max --no-dopus`, KS 3.1 (40.68, WB 40.42),
  Installer 43.3, DOpus 5.104 install via VTCX:dopus_run.script (same script as tools/rig/dopus_rig.py).
- Workbench Screen 800x600x24, title bar 14 rows (helvetica 11 screen font).
- Resident: Amarchy, amarchy-flat, amarchy-gui, amarchy-menu.
  `amarchy-flat STATUS` at end: `frame=hooked calls=636 flat=155`, `clone draws=76 fallback=0`.

## Pixel measurements (RGB crop via ami.req 0x07, run-length map x 152..647)
- Every page, including progress pages "(30% done)" and "(85% done)": window 152,195 496x210, bottom row 404.
- Lowest gadget ends row 401; rows 402-403 body 1a1b26 with 1 px accent sides; row 404 full 496 px accent ring 7aa2f7.
- Title bar rows 205-208 accent. Content not clipped top or bottom.
- Buttons: rounded flat fills (primary 7aa2f7 anti-aliased corners, secondary 3e4254). Labels a9b1d6.
- 30% page: ghost-dotted boxes rows 369-401 = Installer-disabled buttons while copying (expected).
- Inactive window (backdrop clicked at 60,500): full ring incl. row 404 in inactive pen; rows 402-403 body.

## Not measured
- Flat off vs on on this rig: amarchy-flat has no QUIT/Disable entry point in flat.c (grep `"QUIT`/`STATUS/S`
  found none), and Break hung rig 97 before. Not needed: rig 93 measure
  (thoughts/shared/s61-evidence/installer-borders-rig-measure.md) already shows flat ABSENT reproduces the
  owner picture exactly (row 404 body colour, 000000 below, no button bevels), flat present fixes both.

## Root cause (by elimination + rig 93 evidence)
Rig 94 booted tokyo-night flat pens (SHINE=SHADOW=BACKGROUND pen 0) without amarchy-flat resident, so every
bevel/ring drawn only by the flat patch vanished: bottom ring row = body colour ("written over by bg color")
and button frames missing ("buttons have no borders"). Same root as INSTALLER-DIALOG-NO-BORDERS item (1)/(2),
fixed by FLAT-ABSENT-AT-BOOT (amarchy/desk_dos.c desk_dos_flat_ensure, called first in desk_start_gate in
amarchy/amarchy.c; regression: tools/rig/amarchy_desk_rig.py --flat-ensure; proof
thoughts/shared/s61-evidence/flat-ensure-rig-proof.md). Later flat commits (b7377f75, c4844c4e rounded
Installer Border buttons) also post-date rig 94's binary.

## 418 px window height
Not reproduced (210 px here, sys.hdf clone earlier 210 px). Unverified whether rig 94's Installer used a
different screen font; rig 94 state at the failing moment is gone.

## Owner eye-check
Fresh `tools/rig/rig.py dopus` rig on a HEAD build: Installer window bottom edge has the accent ring, buttons
are rounded filled boxes, on an inactive window too.

## Height hunt (s66 second pass)

Owner shot: /Users/spot/Pictures/claude-inbox/CleanShot 2026-11-09 at 10.06.31@2x.png (85% page, "How should
Directory Opus 5 start?"). The earlier "not reproduced" verdict is withdrawn: the owner's picture IS what the rig shows,
and the window IS short for its layout. What follows says why, in numbers, and which layer owns it.

### Measured (rig 169, live, text only)
- wasabid window list: `W 0x10030ff4 152 195 496x210 borders L4 T14 R4 B2`, screen 800x600x24 "Workbench Screen".
  Borders are the stock values for a helvetica 11 screen font (WBorTop 2 + 11 + 1 = 14, WBorBottom 2): no kit layer
  changes the border sizes or the window size.
- Gadgets (uitree): MX 16x9 at y 280/290/300; Proceed 160,369 238x16; Abort 402,369 238x16; Help 281,386 238x16.
- Inner area rows 209..402 (194 rows). Help ends row 401: **1 px** above the inner bottom, 2 px above the visible
  ring (row 403 is the body-coloured inner border row, row 404 the accent ring). Proceed row ends 384, Help starts
  386: 1 px between the button rows. The prompt is one line (DOpus script line 449, nothing missing); the space above
  the MX block is empty, so nothing is clipped, but the bottom block has no margin.

### Root cause: Installer 43.3's own size formula (C:Installer, md5 d23d5e8b2ae23b97685cb3e811aa9abd)
Disassembled build/rig-inputs/Installer (hunk 0, m68k-elf-objdump):
- 0x42c4 LockPubScreen(NULL); 0x436a font = Screen->RastPort.Font (screen+136 = the screen font, helvetica 11);
  0x4370 bsr 0x2b4 sizes the window; falls back to GfxBase->DefaultFont, then topaz 8, only if it exceeds the
  visible area; 0x43d2-0x4414 centres it; 0x4420 OpenWindow (NewWindow at a4+4).
- 0xbcba: lineh = clamp(8, tf_YSize, 100); charw = clamp(8, TextLength("...",3)/3, 100).
- 0x2b4: **Height = (lineh + 1) * 16 + tf_YSize + 7 = 17*Y + 23**; Width = 62 * charw (0x18818).
  Y=11 -> 210 (this rig); Y=8 (topaz) -> 159 (s61 hunt 1 on 640x256: 448x159, matches).
- The constant 23 budgets the title bar as Y+3 and the bottom border as 2, leaving 16 text lines + 2 px. The button
  block (two rows of Y+5 = 16 px, 1 px gaps, 1 px bottom margin) is placed from the bottom, so the window has
  exactly as much room as Installer 43.3 ever gives; on a 2-row flat bottom border that reads as "buttons sit on the
  edge". Not caused by amarchy-flat, the strut patch (210 px window, nothing clamped) or the border sizes.
- 0x44a4: after OpenWindow, Installer stores Window->Height (a0@(10)) into its layout height (a4+5974) and lays out
  from it. So a taller window opened for it is honoured (buttons stay bottom-anchored, the extra rows become margin).

### Fix
No code changed. The bug is in a third-party binary's formula (Installer 43.3), not in a vtcon layer; every fix is a
product decision for the owner:
1. Amarchy's OpenWindow patch (amarchy/strut_patch.c already hooks OpenWindow/OpenWindowTagList) adds bottom margin
   to windows whose task is "Installer" (Installer re-reads Window->Height, 0x44a4, so this works without touching
   its layout). Needs owner approval: it changes a stock program's window.
2. Ship/require Installer 47.19 (3.2) where available (different sizing, see s61 installer-ks32-proof.md).
3. Accept stock 43.3 sizing.
No regression test written (no fix chosen). Rig proof = the numbers above (rig 169, 2026-11-09).

### Unselected MX choices draw no circle (recorded, not fixed)
MX gadgets 2/3/4 are GadTools MX_KIND (16x9 images, mxclass). The unselected image is the mx image's bevel drawn in
SHINE/SHADOW pens; on this screen SHINEPEN == BACKGROUNDPEN (s61-evidence/b10-dopus-bevel-measure.md:23), so the light half of the outline
vanishes unless amarchy-flat redraws the MX image. Unverified which flat path (if any) handles mxclass: grep
amarchy/flat.c for MX/mxclass next.

## Grow fix (s66)  -- STOPPED: growing the window does not add the Help-row margin

Built, NOT committed (owner decision needed): amarchy/win_core.c/.h (win_is_installer_name, win_installer_grow), amarchy/strut_patch.c
(installer_grow(), caller_is_installer(), req.grew, grown counter + sentinel AMARCHY_STRUT_GROWN, both hooks), tests/test_win.c
(installer_grow; `make test-one SUITE=win`: 309 checks OK; with win_installer_grow stubbed to return 0 it FAILS 4 checks, 354-359).
Recognition: task name or CLI command name (BSTR, memory reads only) with last path part "Installer" (any case) AND requested
height == 17*YSize+23 (YSize clamped 8..100). Window titles are the script path, so no title rule. Installer 47.x: not measured (no
rig run); the formula gate leaves any other sizing untouched by construction.
Height rule: +YSize/2 rows (5 at helvetica 11), top up half of that, not above the title bar, bottom not below screen height - strip.
Rig 172 (--os 31, 640x256 topaz 8, DOpus5 install first page): without Amarchy 448x159 at 96,48; Proceed/Abort y 177, Help row y 191
(h 13): bottom margin 1 px, row gap 1 px. With Amarchy hook: 448x163 at 96,46 (hook moved it, so the counter moved); Proceed 179,
Help 193, MX 121/131/141 (unchanged absolute). Bottom margin still 1 px, row gap still 1 px.
Finding: Installer DOES relayout from Window->Height (button rows moved +2 with the bottom edge), but it anchors the button block to
the bottom with a fixed 1 px margin and spreads the extra rows ABOVE it (content centred). So a taller window cannot give the Help
row a bottom margin; the "extra rows become margin" reading of 0x44a4 above is wrong. The fix as designed does not cure the squeeze.
Rig 172 stopped and cleared. No screenshot taken.

## Margin patch (s66)

Owner decision: patch Installer 43.3's layout margins in memory, inline and fail-closed like the dopus5 precedent.

**Patch site.** Installer 43.3 (C:Installer, md5 d23d5e8b2ae23b97685cb3e811aa9abd), hunk 0 at file offset 40. Code offsets:
- 0xc0ee `322c5dce 92405341 35410006` (single-row Help placement): `subq.w #1,d1` = 0x5341 at 0xc0f4.
- 0xc274 `322c5dce 92405341 35410006 37410006` (Proceed/Abort row): 0x5341 at 0xc27a.
- Guards that must also match: 0x441c `2c6c003c 4eaeff34` (plain OpenWindow call), 0x2ca `52402200 e941d268 00145e41` (Height = 17*Y+23).
- Patch: 0x5341 -> 0x5941 (`subq.w #4,d1`) at both sites (high byte 0x53 -> 0x59). Both sites #1 = ORIG, both #4 = PATCHED, anything else = UNKNOWN, no write.

**Rule.** k = 4 = Installer's own spacing: side margin BorderLeft+4 and the 4 px Proceed/Abort gap (rig: Proceed x 104 = 96+4+4, Abort x 322 = 318+4). Two button rows each gain 3 px, so the window grows by 2*(4-1) = 6 px and moves up 3 px to stay centred. Topaz 8: 159 -> 165. Helvetica 11: 210 -> 216. When the grown window does not fit the screen, nothing is patched.

**Where and why.** `inst_window()` in amarchy/strut_patch.c, called from the OpenWindow hook `plain_hook` before parse(). It runs on Installer's own task before layout, and layout reads Window->Height after OpenWindow, so code and window size change together. The code is found via pr_CLI->cli_Module, else pr_SegList[3]; the hunk size must be >= INST_CODE_MIN+8 (0xc284). Writes happen under Forbid, signature rechecked inside, then CacheClearU. No restore: the code belongs to that Installer process and dies with it. Already-patched code (resident) still gets the grown window. Sentinel AMARCHY_INSTALLER_MARGIN (strut_sentinels, g_last[4]) counts applied patches.

**Files.** amarchy/inst_core.h, amarchy/inst_core.c (pure: inst_sig, inst_apply, inst_row_margin, inst_font_y, inst_grow, inst_box), tests/test_inst.c (`make test ONLY=inst`), Makefile, tests/test_main.c, amarchy/strut_patch.c. Commit f8a63b97 (not pushed).

**Test before/after.** With inst_apply stubbed (no write): 5 of 29 checks fail (margins stay 1/1, bytes stay 0x53). With the fix: 29/29 OK.

**Rig 172 (OS 3.1, DOpus5 install, page "How should Directory Opus 5 start?").**
- Topaz 8, plain: window 96,48 448x159; Proceed/Abort y 177 h 13; Help y 191; bottom margin 1, row gap 1.
- Topaz 8, Amarchy: window 96,45 448x165; Proceed/Abort y 174; Help y 191; bottom margin 4, row gap 4. Radio buttons unchanged relative (y 60), nothing clipped. AMARCHY_INSTALLER_MARGIN unset -> 1, AMARCHY_STRUT_OPENS 1.
- Helvetica 11, plain: window 72,23 496x210; Proceed/Abort y 197 h 16; Help y 214; margin 1, gap 1.
- Helvetica 11, Amarchy: window 72,20 496x216; Proceed/Abort y 194; Help y 214 (bottom 230, content bottom 234); margin 4, gap 4. Radio buttons at 105/115/125, all inside. Sentinel 1.
- First helvetica Amarchy attempt: rig went into a Kickstart reset loop (182 "CPU reset" lines) a few seconds after `Run RAM:Amarchy`, before Installer started. Not reproduced on the retry with the same build and prefs. Cause unknown (unverified; Installer was not running, so the margin patch did not run at that point).
- No screenshot taken. Rig 172 stopped and cleared.
