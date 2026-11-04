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

## Amarchy flat drawing (pixel checks)

| date | key | emulator + config | build | check | result | evidence |
|---|---|---|---|---|---|---|
| 2026-11-04 (s56) | S56-LOCALE-DRAGBAR-COLOUR, S56-LOCALE-CANCEL-CORNERS | FS-UAE rig 75, 3.2, 060 max, WB 640x256 depth 8 | 85bdf561 | Locale listview knob pen, Cancel corner pixels, before/after + revert | PASS: knob pen 3 rounded, Cancel inner diagonals pen 2; revert fails | s56-evidence/locale-prefs-knob-corners.md A, B |
| 2026-11-04 (s56) | S56-FLAT-BUDGET-RED smoke | FS-UAE rig 85, 3.2, 060 max | 6a682293 vs 7e51272c | Locale knob, Cancel corners, XCON knob active/inactive, WB drawer edge, clone draws | SAME as 7e51272c; XCON inactive knob pen 251 (= STATUS inbar) vs 250 in old evidence; clone draws=242 fallback=0; Amarchy alive | locale-prefs-knob-corners.md "Smoke check 6a682293" (1a8d35f9) |
| 2026-11-04 (s56) | S56-LOCALE-SAVE-FILL | FS-UAE rig 75 | 85bdf561 | Save button area 100..187 x 224..237 | UNVERIFIED read: pen 0 except label; needs one pixel dump | todos.md S56-LOCALE-SAVE-FILL |
| 2026-11-04 (s56) | owner eye check | FS-UAE rig 96 full install, 3.2 | 7e51272c | new-shell knob, Locale knobs, Cancel corners | WAITING on owner | todos.md S56-VTCON16 |
| 2026-11-04 (s56) | S56-UD2-EARTH-OVER-PHOTO globe corners | FS-UAE rig 109, 060 max, 24-bit CGX screen | fc8741e1 (s56-ud2-earth-photo) | UPDemo 15, 23x23 corner patches of globe block, shots every ~1 s | before regen of earth.anim: TL 480 TR 513 BL 482 BR 445 of 529 dark; after: 0/0/0/0; palette path not exercised | s56-evidence/ud2-earth-photo.md |
