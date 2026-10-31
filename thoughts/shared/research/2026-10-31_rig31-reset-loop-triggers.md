# RIG31-RESET-LOOP trigger hunt

## s39 trigger hunt (2026-10-31, rig 6, 3.1 via --os 31, 060)

Result: NOT reproduced. No reset loop, no watchdog.log, no reset-loop-*.fs-uae.log was written, so there is no first fault line to quote.

- Boot: fresh build/rig6 on 3.1; start printed "[WARN] the Workbench stayed PAL (W43)". The clean 3.1 tree has only Devs/Monitors/PAL (no RTG driver, no NTSC monitor) and ENVARC screenmode.prefs holds the PAL hires mode (0x29000). This rig therefore has no RTG screen, so trigger (a) (RTG NTSC switch + RTG screen close) could not be built here. The ScreenMode list items are not readable through the uitree; clicking rows left Width/Height at 640x256.
- (b) Palette prefs opened and USE clicked: 3 of 3 cycles, wasabid answered a ping after each, no reset.
- (c) Amarchy bar started; three open/close key cycles (LAMIGA+Space as raw 0x40 qual 0x8, Esc) did not visibly open the menu (uitree showed only the bar), so this was not a real test of the menu on an RTG screen. 3 cycles, no reset.
- Conclusion: the trigger needs an RTG screen. Next: use a 3.1 rig where RTG is installed (the rigs 5/7 that looped had it), or install the RTG driver into the os-31 tree, then do ScreenMode USE to an NTSC RTG mode and close the screen.
