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
