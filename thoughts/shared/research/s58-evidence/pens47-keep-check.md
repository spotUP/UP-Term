# Keep check: miasma and ristretto picks (rig 149)

Question: were the wrong pens on rig 148 after the miasma and ristretto picks a REVERT (FP-1 Keep requester not answered)?
Build: s58-integ 4d083e4e plus commit 7eef5119 (harness prints the Keep timing per pick, no product change). Rig 149, fresh-all [PASS], OS 3.2, 060 `--max --os 32 --idle-minutes 0`, purpose "vtcon-c1: Keep check miasma ristretto".
md5 rig UP-Term:bin/x equals build/amiga/x: Amarchy 2bfd910d, amarchy-gui 1bae23e9, amarchy-menu a5405f0e, amarchy-flat cc7086b7.
Commands: `UPTERM_RIG=149 INSTALLER_KEEP=1 UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0 --allow-stale-base '...' --purpose '...'" python3 tools/rig/installer_rig.py ONLY fresh-all`;
per pick (scratchpad keepcheck/pick.py, a wrapper of `tools/rig/amarchy_theme_save_rig.py --selects N-N --no-build` with KEEP_SECS=40 and a forced walk direction, because walk_key picks Down whenever there is room and the list order is alphabetical): then `RAM:rolepens` probe, `Type ENV:/ENVARC:Amarchy/prefs`, `ami.py shot`, PIL pixels at (320,128) and (320,230).

## Verdict lines
solitude: requester seen yes at 9.1 s | Keep returned True at 9.1 s | requester gone yes | pens 0-3 match | pens 4-7 match | pixel match | KEPT | menu window back no after 60 s
miasma: requester seen yes at 8.9 s | Keep returned True at 9.0 s | requester gone yes | pens 0-3 match | pens 4-7 match | pixel match | KEPT | menu window back no after 60 s
ristretto: requester seen yes at 8.9 s | Keep returned True at 8.9 s | requester gone yes | pens 0-3 match | pens 4-7 match | pixel match | KEPT | menu window back no after 60 s
miasma (again): requester seen yes at 10.0 s | Keep returned True at 10.1 s | requester gone yes | pens 0-3 match | pens 4-7 match | pixel match | KEPT | menu window back no after 60 s
(seconds are from the Return key of the pick.)

Hypothesis (rig 148's miasma and ristretto were REVERTED because Keep was not answered): CONFIRMED as the mechanism, by contrast. With the requester answered, all four picks were kept with correct pens. Decisive lines: pick-attempt0 below (requester window open, harness had stopped looking, pens back to the previous theme) versus the four lines above. The rig 148 run itself was not re-run; its Keep timing was never logged, so that run's own miss is inferred, not measured.

## Attempt 0 (not one of the four picks): Keep not answered
First pick on the fresh rig (default theme tokyo-night, Up 1 = starship), KEEP_SECS still 12: `keep starship: requester seen no | Keep returned False | requester gone unknown` printed 12 s after Return; 3 s later uitree showed `W 0 0 0 434x92 active "Amarchy desktop theme"` (so the requester opened later than 12 s after Return on this first apply, unlike 9 s on the four later picks; cause not hunted). 25 s later uitree no longer had it, and the pens were tokyo-night's (1a1b26 414868 a9b1d6 7aa2f7) while ENV:Amarchy/prefs said `amarchy-gui-theme = starship`: a REVERT with the picked name left in the prefs (FP1-REVERT-THEME-LINE). So a requester answered late or not at all gives exactly the rig 148 picture (revert, stale pens, theme line = picked name). Harness fix to consider: KEEP_SECS 12 is too short for a first apply (not changed here; the brief allowed only the timing print). Log: scratchpad keepcheck/pick-attempt0-keepsecs12.log.

## Pens 0-3 / 4-7 expected versus read (LPEN from RAM:rolepens), pixels
Expected 0-3 = background muted foreground accent of the theme file; 4-7 from the expectation tool (flat=1) and, for solitude, from the 148 run.
| pick | pens 0-3 read | 0-3 expected | pens 4-7 read | 4-7 expected | (320,128) | (320,230) |
|---|---|---|---|---|---|---|
| solitude | 101315 4b4e55 cacccc 798186 | same | 343d41 080a0b 9fa5a9 080a0b | same | 101315 (exp 101315) | 080a0b (exp 080a0b) |
| miasma | 222222 666666 c2c2b0 78824b | same | 4b4b4b 121212 5f875f 121212 | same | 222222 (exp 222222) | 121212 (exp 121212) |
| ristretto | 2c2525 72696a e6d9db f38d70 | same | 535154 181414 adda78 181414 | same | 2c2525 (exp 2c2525) | 181414 (exp 181414) |
| miasma 2 | 222222 666666 c2c2b0 78824b | same | 4b4b4b 121212 5f875f 121212 | same | 222222 | 121212 |
Theme lines (not the verdict): ENV and ENVARC `amarchy-gui-theme` = solitude, miasma, ristretto, miasma. Pens read 3 to 60 s after Keep (the probe ran after the harness finished), pixels from `ami.py shot` at the same time; screenshots ~/Desktop/upterm-shots/keepcheck-<theme>-<n>.png (solitude-1, miasma-2, ristretto-3, miasma-4).

## Q3: windows after Keep (same for all four picks, at Keep +5 s, +30 s, +60 s)
uitree: screen `Amiga Workbench Release 3.2 ...` (at +5 s too), windows `Amarchy bar` (0 246 640x10), `""` (0 11 640x235, active), `f1host` (0 20 600x200). No window titled `Amarchy` (the menu, TITLE in amarchy_gui_rig.py) and no `Amarchy desktop theme` at any of the three times.
`Status`: Amarchy, amarchy-flat, amarchy-gui, amarchy-menu all still running at each time. So the menu window does NOT come back after Keep (matches the 148 observation, 150 s); the harness reports `[FAIL] no menu window after the pick (Keep answered)` for all four (30 s wait_stable timeout), a separate fault from the revert. Requester text: only the title "Amarchy desktop theme" was visible in the window list (434x92); gadget text not captured.
Raw logs: scratchpad keepcheck/pick-*.log, probe-*.txt, prefs-*.txt.
