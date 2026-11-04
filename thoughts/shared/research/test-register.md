---
date: 2026-11-04
topic: Test register - every rig, emulator and pixel test already run, with its verdict
status: living document, append only
---

# Test register

Owner 2026-11-04: "log your tests so you dont retest them".

Rule for every session and every agent:
1. BEFORE a rig, emulator, A1200 or pixel test: grep this file for the test key (bug id, emulator, recipe).
   A row with the same build, config and recipe means the answer is known: do not run it again.
   Run again only when something in the row changed (new commit on the tested path, new config, new emulator
   version), and say which column changed.
2. AFTER every run (pass, fail or no verdict): append one row. A run with no verdict is logged too, with why,
   so the same dead end is not retried.
3. One row per test configuration, not per boot. Counts go in the result ("5 of 6").

Columns: date | key (ledger id) | emulator + config | build (commit) | recipe | result | evidence (file + section, commit)

## Palette Copy crash (FSUAE-31-PALETTE-UPSTREAM)

Recipe P: fresh boot or `rig.py reboot`, 20 s settle, `Version` rc 0, owner bytes tests/data/desk/palette-gui-save.prefs
to RAM:palette.prefs, `Copy RAM:palette.prefs ENV:Sys/palette.prefs`, 30 s wait, `Echo alive`.

| date | emulator + config | build | recipe | result | evidence |
|---|---|---|---|---|---|
| 2026-11 (s54) | FS-UAE 3.2.35 + 3 local commits, 3.1, 060 JIT, uaegfx, bare OS | rig build | P | 2 of 6 crash (jump to $A6, filesys lput flood or reset loop); about 3 of 4 with Intuition patchers | s54-evidence/31-copy-crash-bisect.md rounds 1-3 (117c7add, 766108d7) |
| 2026-11-04 (s56) | FS-UAE stock v3.2.35 (4ae7ddaec), 3.1, 060 JIT, uaegfx, SolidWin off | rig 68 | P | 6 of 6 host process dies after RTG to native switch; control boot alive 100 s; one extra boot with SolidWin on died the same way | fsuae-31-palette-upstream.md "Stock v3.2.35 retest (s56)" |
| 2026-11-04 (s56) | FS-UAE stock v3.2.35 under lldb, same config | rig 68 | P x4 | host SIGSEGV EXC_BAD_ACCESS 0x351004000, m68k_run_2 opcode fetch newcpu.cpp:5241; 1 of 4 guest reset loop instead; guest registers NOT read | fsuae-31-palette-upstream.md "lldb on stock v3.2.35 (s56)" (c1d3a362) |
| 2026-11-04 (s56) | Amiberry (/Applications/Amiberry.app), 3.1, 060 JIT, ZorroIII gfx 16 MB, SolidWin ON | rig 84 | P x6 + control | 5 of 6 guest lost (green screen), host alive; JIT "Error in compiled code" stores pc=00f8198a about 3 s after Copy; control no Copy alive | fsuae-31-palette-upstream.md "Amiberry retest (s56)" (cb830707) |
| 2026-11-04 (s56) | Amiberry, 3.1, 060 JIT OFF (`--no-jit`, cachesize=0) | rig 84 | none ran | NO VERDICT: guest never booted, Amiberry pauses (amiberry_inactive) or stalls (SDL isfullscreen) with its window in the background, even with inactive_pause=false. Do not retry unless the window is kept frontmost | fsuae-31-palette-upstream.md "Amiberry JIT-off retest (s56)" (f823b10a) |
| 2026-11-04 (s56) | FS-UAE rig build, 3.1, 060 JIT OFF (`--no-jit`) + 2 JIT-on control boots | rig 84 | P x6 + 2 | JIT off: 6 of 6 guest lost (host alive); JIT on control: 2 of 2 host dead. Not a pure JIT bug | fsuae-31-palette-upstream.md "FS-UAE JIT-off retest (s56)" |
| 2026-11-04 (s56) | FS-UAE debug build (fs-uae-debug 661e884dd + local), 3.1, 060 JIT OFF, uaegfx, IPrefs + SolidWin on, `--debugger`, dbgbreak 30 s after Copy | rig 84 | P x1 | guest lost, host alive; PC $190800F8 unmapped, A6=$B2, reset loop about 3 per second | fsuae-31-palette-upstream.md "Guest-side hang hunt (s56)" run 1 |
| 2026-11-04 (s56) | same, PC-range break `f 1000000 3fffffff` before Copy | rig 84 | P x1 | hit in exec VERTB dispatcher $F811F8: `MOVEA.L $4,A6` reads $B2, JMP (A5) to $190800F8; ExecBase pointer at $4 overwritten | same section, run 2 |
| 2026-11-04 (s56) | same, write memwatch `w 0 4 4 W` before Copy | rig 84 | P x1 | writer found: task IPrefs, exec AddHead PC $F8195E writes $000000B2 to $4, called from ROM intuition $FE3860/$FE7960 with NULL-based list; not our code. Splits (IPrefs off, native WB, Palette-saved file) NOT run | same section, run 3 |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, SolidWin off (S1), IPrefs on, RTG | rig 84 | P x1 | $4 written $B2 PC $F8195E; Echo alive no (same as hunt): SolidWin not needed | fsuae-31-palette-upstream.md "Split runs (s56)" |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, IPrefs killed (S2), SolidWin on, RTG | rig 84 | P x1 | no $4 write, Echo alive yes: IPrefs needed | fsuae-31-palette-upstream.md "Split runs (s56)" |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, native screen via `rig.py aga` (S3), uaegfx card still in config | rig 84 | P x1 | $4 region written (long 0 at $6, PC $F81970), guest reset; Echo alive no: RTG screen not needed | fsuae-31-palette-upstream.md "Split runs (s56)" |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, prefs from Palette editor Save (S4), no Copy | rig 84 | GUI Save x1 + P control | Save: no write, alive; Copy of owner bytes after it: $B2 PC $F8195E, Echo no: the prefs bytes trigger it | fsuae-31-palette-upstream.md "Split runs (s56)" |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, IPrefs + SolidWin on, RTG | rig 84 | P with file B (golden with the 13th pen word removed from both pen lists: 12 pens then ffff) | $4 not written after Copy in 30 s; `Echo alive` answers: ALIVE (1 boot) | s56-evidence/palette-31-safe-format.md (Field bisect) |
| 2026-11-04 (s56) | FS-UAE debug build, 3.1, 060 JIT OFF, memwatch `w 0 4 4 W`, IPrefs + SolidWin on, RTG | rig 84 | P with file C (golden with colour words requantised to nibble replication, pens untouched) | $4 written `$000000B2` PC $00F8195E (exec AddHead from IPrefs, same as the golden); `Echo alive` silent: CRASH (1 boot) | s56-evidence/palette-31-safe-format.md (Field bisect) |
| 2026-11-04 (s56) | same, file A (golden unchanged) | rig 84 | P | NOT RERUN: rows S4/P above already cover the golden on this config (crash) | - |

## Amarchy flat drawing (pixel checks)

| date | key | emulator + config | build | check | result | evidence |
|---|---|---|---|---|---|---|
| 2026-11-04 (s56) | S56-LOCALE-DRAGBAR-COLOUR, S56-LOCALE-CANCEL-CORNERS | FS-UAE rig 75, 3.2, 060 max, WB 640x256 depth 8 | 85bdf561 | Locale listview knob pen, Cancel corner pixels, before/after + revert | PASS: knob pen 3 rounded, Cancel inner diagonals pen 2; revert fails | s56-evidence/locale-prefs-knob-corners.md A, B |
| 2026-11-04 (s56) | S56-FLAT-BUDGET-RED smoke | FS-UAE rig 85, 3.2, 060 max | 6a682293 vs 7e51272c | Locale knob, Cancel corners, XCON knob active/inactive, WB drawer edge, clone draws | SAME as 7e51272c; XCON inactive knob pen 251 (= STATUS inbar) vs 250 in old evidence; clone draws=242 fallback=0; Amarchy alive | locale-prefs-knob-corners.md "Smoke check 6a682293" (1a8d35f9) |
| 2026-11-04 (s56) | S56-LOCALE-SAVE-FILL | FS-UAE rig 75 | 85bdf561 | Save button area 100..187 x 224..237 | UNVERIFIED read: pen 0 except label; needs one pixel dump | todos.md S56-LOCALE-SAVE-FILL |
| 2026-11-04 (s56) | owner eye check | FS-UAE rig 96 full install, 3.2 | 7e51272c | new-shell knob, Locale knobs, Cancel corners | WAITING on owner | todos.md S56-VTCON16 |
| 2026-11-04 (s56) | S56-UD2-EARTH-OVER-PHOTO globe corners | FS-UAE rig 109, 060 max, 24-bit CGX screen | fc8741e1 (s56-ud2-earth-photo) | UPDemo 15, 23x23 corner patches of globe block, shots every ~1 s | before regen of earth.anim: TL 480 TR 513 BL 482 BR 445 of 529 dark; after: 0/0/0/0; palette path not exercised | s56-evidence/ud2-earth-photo.md |
| 2026-11-04 (s56) | S56-LOCALE-SAVE-FILL | FS-UAE rig 87, 060 max, 3.2, depth 8 | 386666d6 (main) | Locale Prefs Save/Use/Cancel boxes vs Input Prefs Save/Use/Cancel and Workbench buttons, PIL pixel counts of 640x256 shots | PASS: Locale Save 1148 px pen 0 + 84 label, identical to Input Prefs Save; fill is pen 0 (flat_button_pen -> pen_back), same as Workbench primary buttons | s56-evidence/verify-run-87.md |
| 2026-11-04 (s56) | S33-RIG-ONLY-BATCH | rig 87 | 386666d6 | all 3.2 rows | NOT RUN: every row handed to rig 96 R3 / rig 64 / batched 3.1 run by the row text | s56-evidence/verify-run-87.md |
| 2026-11-04 (s56) | PAL31 build 5292ff95 (s56-pal31), FS-UAE 060 max, 3.1 (Palette 40.2), IPrefs + SolidWin | rig 84 | (a) golden palette-gui-save.prefs (md5 172bd3aa...) in ENVARC:Sys only, Amarchy started | PASS: ENVARC file converted (md5 6a80b98c..., 434 bytes, both PALT lists 12 pens then ffff), ENVARC:Sys/palette.prefs.32bak = golden bytes, ENV:Sys file written in safe form, Echo alive, no Guru; sentinels PAL31_CHECKS 3, PAL31_FIXES 1. Golden NOT put in ENV:Sys by hand: a live Copy there crashes 3.1 IPrefs before Amarchy runs (rig died twice) | thoughts/shared/research/s56-evidence/pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.1, same config | rig 84 | (b) desk palette apply: start-apply palette write; menu theme pick via amarchy_desk_rig.py | PARTIAL: start-apply wrote ENV:Sys/palette.prefs safe form, IPrefs applied the colours (Workbench RGB 0-3 = 1a1b26 414868 a9b1d6 7aa2f7), no crash. Menu theme pick NOT run: the drawn menu did not open on this 3.1 rig (rig script TimeoutError open menu) | pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.1, same config | rig 84 | (c) rig.py reboot after the conversion | INCONCLUSIVE: Workbench booted (Echo alive, Version ok), but the start re-cloned DH0: and ENVARC:Sys/palette.prefs and .32bak were gone, so the converted file was not what booted | pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.2 (Kickstart 47.115, Palette 47.21) | rig 84 | golden in ENVARC:Sys, Amarchy started | PASS no-change: ENVARC bytes unchanged (md5 172bd3aa...), no .32bak, Echo alive; PAL31 sentinels absent (pass not entered) | pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.1 (Kickstart 40.68, WB 40.42), rig started --os 31 | rig 84 | (b) desk theme picks via amarchy_desk_rig.py (rerun) | PASS: ENV:Sys + ENVARC:Sys palette.prefs identical, md5 6abeac3b..., first ffff at word 20 after 12 pens; Echo alive 30 s later, no Guru. First run again timed out at open menu right after --setup (transient) | pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.1 --os 31, same config | rig 84 | (c) golden in ENVARC, Amarchy converted, rig.py reboot | PASS: ENVARC 6a80b98c... and .32bak 172bd3aa... survive, ENV:Sys = 6a80b98c... at boot, WB 40.42 alive 30 s | pal31-rig-pass.md |
| 2026-11-04 (s56) | PAL31 5292ff95, 3.1 --os 31, same config | rig 84 | (d) WB reset requester after palette apply, with a stray CON: window open | INCONCLUSIVE: no requester in 3 picks; screennotify.library 1.0 present | pal31-rig-pass.md |
| 2026-11-04 (s56) | S56-PRIMARY-ACCENT-FILL / S56-CYCLE-PLAIN / S56-LOCALE-SAVE-FILL | FS-UAE rig 86, 060 max, 3.2, depth 8, flat on, Amarchy tokyo-night | 37b5e3d2 (main) | wbpenprobe pen indices of Save/Use/Cancel, arrows, Sound Type in Locale/ScreenMode/Input/Serial/Sound prefs | ACCENT FAIL: every Save/Use/Cancel is pen4 (plain), 0 pen3 px; CYCLE unconfirmed (Sound Type fill pen255, not pen4); SAVE-FILL PASS (not pen 0) | s56-evidence/accent-cycle-pixels.md |
| 2026-11-04 (s56) | s56-deskrig 81f68ac4, 3.1 (rig started --os 31), same config | rig 89 | amarchy_desk_rig.py after amarchy_gui_rig.py --setup (setup itself FAILed: flat prefs off, amarchy-flat BOOT left nothing resident) | FAIL, not conclusive: run 1 needed build/amiga/wbpens (make build/amiga/wbpens) then timed out at open menu; run 2 = 9 [FAIL]: off phase raised AMARCHY_DESKS (prefs empty, desktop-theme defaults on, so the off step did not run off); DrawInfo pens stayed stock 0 1 2 0 0 0 2 0 2 2 0 2 vs 3.2-layout want; palette themed-field compare uses desk_core 3.2 layout (12/12 3.1-safe form check passed, env md5 6abeac3b); font specialdrawmode expected 1 got 0; ENVARC:Sys/palette.prefs changed (3.1-safe form writes ENVARC too, per pal31-rig-pass.md). Script not changed; 3.1 palette-compare/ENVARC/pens expectations still stale, off-phase needs the prefs put checked | deskrig31.log (scratchpad) |
