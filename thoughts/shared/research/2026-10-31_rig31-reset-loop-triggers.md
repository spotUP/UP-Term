# RIG31-RESET-LOOP trigger hunt

## s39 trigger hunt (2026-10-31, rig 6, 3.1 via --os 31, 060)

Result: NOT reproduced. No reset loop, no watchdog.log, no reset-loop-*.fs-uae.log was written, so there is no first fault line to quote.

- Boot: fresh build/rig6 on 3.1; start printed "[WARN] the Workbench stayed PAL (W43)". The clean 3.1 tree has only Devs/Monitors/PAL (no RTG driver, no NTSC monitor) and ENVARC screenmode.prefs holds the PAL hires mode (0x29000). This rig therefore has no RTG screen, so trigger (a) (RTG NTSC switch + RTG screen close) could not be built here. The ScreenMode list items are not readable through the uitree; clicking rows left Width/Height at 640x256.
- (b) Palette prefs opened and USE clicked: 3 of 3 cycles, wasabid answered a ping after each, no reset.
- (c) Amarchy bar started; three open/close key cycles (LAMIGA+Space as raw 0x40 qual 0x8, Esc) did not visibly open the menu (uitree showed only the bar), so this was not a real test of the menu on an RTG screen. 3 cycles, no reset.
- Conclusion: the trigger needs an RTG screen. Next: use a 3.1 rig where RTG is installed (the rigs 5/7 that looped had it), or install the RTG driver into the os-31 tree, then do ScreenMode USE to an NTSC RTG mode and close the screen.

## RTG run (s39)

Result: NOT reproduced on an RTG screen either. 0 "CPU reset" lines in build/rig6/logs/fs-uae.log.txt, no reset-loop-*.fs-uae.log, no watchdog.log, wasabid answered after every step.

Setup: the os-31 tree had no RTG driver. rig.py now has os_tree_rtg(): when an --os tree is made it reads Devs/Monitors/uaegfx(+.info), Libs/Picasso96API.library and Libs/Picasso96/{emulation,rtg,fastlayers}.library from the master system disk (xdftool, master only read). Rig 6 (3.1, 060, --os 31 --fresh), then `rig.py rtg` (reboot): `ami.py screens` = 800x600 depth 24 "Workbench Screen" (RTG). This is the 800x600 uaegfx mode, not the NTSC 640x360 variant (screenmode.rtg360.prefs) of the 29 Oct loops.

| Trigger (all on the RTG screen) | Count | Result | Evidence |
|---|---|---|---|
| UP-Term window open + close (close gadget, gclick sys:close) | 3 (plus 4 extra closes of leftovers) | no reset | ping/Version answered after each; 0 CPU reset |
| Palette prefs, USE | 3 | no reset | screen stayed 800x600x24, Version answered after each |
| Menu LCOMMAND(0x40)+Space with qualifier 0x40, then Esc, after `Run VTC:Apps/bin/Amarchy` | 3 | menu opened (screen_text has "Filter" and "Apps"), no reset | 0 CPU reset |

Notes: the menu needs the Amarchy commodity running (Run VTC:Apps/bin/Amarchy); without it the key does nothing (the earlier doc's "wrong qualifier" was also the missing commodity: qualifier is LCOMMAND 0x40). Typing `exit` does not close the UP-Term window; the close gadget does. Not tried: ScreenMode USE to the NTSC RTG mode (rtg360) and closing that screen, the exact 29 Oct trigger; the ScreenMode list rows are not clickable through the uitree. Loop seen once rig-side came with 060 libs and a prior crash (see todos RIG31-RESET-LOOP).

## NTSC RTG trigger (s39)

Result: NOT reproduced. 0 "CPU reset" lines in build/rig6/logs/fs-uae.log.txt after every step, no watchdog.log, no reset-loop-*.fs-uae.log, wasabid answered a ping after every step. Rig 6, 3.1 (--os 31 --fresh), 060, uaegfx driver from the master disk.

How the NTSC mode was reached: `rig.py rtg` copies the 800x600 file, not rtg360, so VTC:screenmode.rtg360.prefs was copied to ENVARC:Sys/screenmode.prefs and the rig restarted: the boot came up on a 640x360 depth 24 uaegfx Workbench Screen (start still prints the "stayed PAL (W43)" warning because its check is width == 640; false alarm for 640x360). Switching at run time works without the ScreenMode editor: copy the prefs file to ENV:Sys/screenmode.prefs; the running IPrefs reacts (a second `C:IPrefs` returns rc 20) and the screen changes.

| Trigger | Count | Result | Evidence |
|---|---|---|---|
| Boot straight into 640x360 RTG, UP-Term open + close gadget (gclick 0 sys:close) | 3 | no reset | ping after each, screen 640x360x24, 0 CPU reset |
| Run-time switch 800x600 <-> 640x360 via ENV:Sys + ENVARC:Sys screenmode.prefs, no windows open | 3 round trips (6 switches) | no reset | uitree S line 800x600 / 640x360 each time, ping ok, 0 CPU reset |
| Switch to 640x360 with UP-Term (vsh) windows open, then close | 3 | no reset, but the switch never completes | IPrefs raises the System Request (Retry/Cancel, "close all windows") on the 800x600 screen; UP-Term windows stay; 0 CPU reset |

The third row is the W43 requester. Once it is up, clicks at the UP-Term close gadget, Retry and Cancel (pointer script at the uitree coordinates, gclick, and `Break 1 C/D` on the vsh process) did not dismiss it in the budget; the rig stayed alive (ping ok, 0 resets). Not tried: the real ScreenMode editor USE (list rows not clickable), the "right after a forced crash/reboot cycle" variant (68060.library state surviving ColdReboot), and the 29 Oct sequence of closing the UP-Term-owned screen after the switch completed. The 29 Oct loops are therefore still unexplained; a stuck System Request with windows open is the nearest state reached.

## Remaining triggers (s39)

Rig 6, 3.1 (--os 31 --fresh), 060, uaegfx from the master disk, 800x600 RTG via a run-time ENV:Sys copy of VTC:screenmode.rtg.prefs (`rig.py rtg` left the boot on the PAL prefs here: ENVARC:Sys/screenmode.prefs held the 0x29000 PAL mode; the copy made the screen 800x600x24).

| Trigger | Cycles | Reset | "CPU reset" lines |
|---|---|---|---|
| 1. Real SYS:Prefs/ScreenMode: list scrolled with the arrow gadgets, row clicked, Width/Height read from the uitree (640x360), Use (gadget id 8) | 1 Use, then 3 UP-Term open + close-gadget cycles on 640x360, all pings answered | NO | 0 |
| 2. Same trigger after a forced crash/reboot cycle | (a) `crashme illegal` (Illegal instruction 1050 at 1009E1E0): LOOP, see below. (b) After a restart: `ami.py reboot` (ColdReboot, back after 25 s), ScreenMode Use to 640x360 (depth came up 16), 3 close-gadget cycles, 2 own-screen cycles, pings answered | (a) YES (b) NO | (a) 478 when stopped (192 then 452 within about 30 s) (b) 2, both from the ColdReboot itself (PC=100254d2 then f800d0), none after the trigger |
| 3. UP-Term-owned screen (`NewShell "XCON:0/20/400/100/own/CLOSE/OWNSCREEN"`) closed by its close gadget after the switch to 640x360 had finished, no windows open during the switch | 3 on rig-6 boot A, 2 more after the reboot | NO | 0 |

Finding: the loop reproduces with a plain guru and no RTG at all. `crashme illegal` (tests/amiga/crashme.c) on the 3.1 060 rig: "Illegal instruction: 1050 at 1009E1E0 -> 00F80AD2" at log line 1290, then "CPU reset PC=f80dc8 (Kickstart ROM)" / f800d0 repeating, wasabid refused connections, ~8 resets a second. That matches the 29 Oct state (crash first, loop after) and rig9-installer-loop.md result 8. So the 29 Oct loops need the crash, not the NTSC switch or the screen close; the RTG switch is at most where the earlier crash came from. A fresh fs-uae process (rig.py stop, start without --fresh) cleared the loop; a ColdReboot after a clean run did not loop. Not separated: whether a crash on 3.2/3.1 without 060 libs loops (the 68060.library state in the emulated CPU is the suspect); the RTG switch itself never crashed in 4 runs (rig 6 s39 total: 9 switches).

Evidence cut from build/rig6/logs/fs-uae.log.txt (rig deleted per rule 7). Next step: run `crashme illegal` on a 3.1 `--a1200` (68020) rig; no loop there means the 060 path is the cause.

## 68020 crash check (s39)

Question: does a plain guru loop a 3.1 rig running as a 68020, as it loops the 3.1 060 rig?

Run: rig 3, `rig.py start --a1200 --os 31 --fresh`, then `ami.py put build/amiga/crashme RAM:crashme`, `ami.py exec "RAM:crashme illegal"`.

Result: NO LOOP on the 68020. First fault line (log line 1189): "Illegal instruction: 1050 at 40083A40 -> 00F80AD2". Reset counts (`grep -c "CPU reset"`): 0 before the crash, 2 after (line 1192 PC=f80dc8, line 1281 PC=f800d0, one reset pair), still 2 at 60 s and at 90 s after the guru. wasabid answered `ami.py ping` after the guru (the machine had rebooted itself and was up). The 060 rig gave 478 reset lines within about 30 s for the same program and fault.

Conclusion: the same guru (illegal instruction 0x1050 in RAM) loops the 3.1 060 rig and does not loop the 3.1 68020 rig, so the loop depends on the 060 configuration (68060.library or emulated 060 CPU state surviving the reset), not on the guru itself. Consistent with rig9-installer-loop.md result 8. Not separated: 68060.library versus the emulated 060 core itself; the 3.1 060 run without 68060.library was not done (rig.py has no flag for it, none added).
