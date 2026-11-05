---
date: 2026-11-05
author: opus-brief agent vtcon-c1
row: S57-ICL-PINK-SHADE rig proof
build: main c990849e
verdict: INCONCLUSIVE. Build fixed, rig up, all pixel counts 0, but the measured screen was not shown to be Workbench icon labels; amarchy-flat not on the shell path; 2 boot hangs
---

# Pink/red icon label proof on rig 145 (main c990849e)

## Build
c0e2365d failed to link upprefs (_ip_any_free missing). c990849e links iconpen_core, desk_core, be32 into upprefs; `make dist` rc=0.

## Run
fresh-all on rig 145 (INSTALLER_KEEP=1): first attempt hung at rig start ("daemon did not answer for 150 s", serial log showed the prompt up); retry PASS.
Per boot: Workbench window open/close via ARexx, 3 s settle, screenshot, exact pixel count (python, PNGs in ~/Desktop/upterm-shots/s57-pinkfix-rig145-boot*.png).

| boot | pink exact (247,118,142) | pink family | red family | grey shade | hang |
|------|----|----|----|----|----|
| 0 post-install | 0 | 0 | 0 | 0 | no |
| 1 ami.py reboot | not measured | - | - | - | YES: no ping for 180 s+, still dead 5 min later, serial ends at shell prompt "X8"; recovered by rig.py stop --keep + start |
| 2 fresh start | 0 | 0 | 0 | 0 | no |
| 3 fresh start | 0 | 0 | 0 | 0 | no |

Whole screen and a 140 px left strip both counted, all zero.

## Why this is weak evidence
- Screens have only about 5 colours (dominant (14,14,20), (26,27,38)): looks like a dark console screen, not Workbench with icon labels. The PNGs were not looked at (unverified).
- `amarchy-flat STATUS` returns "Unknown command" (rc 10) on the full-install rig, so flat state and held-pen state were not read.
- Grey shade count 0 also means no antialiased labels were on screen.
- Two hangs (first installer boot, first reboot after install) not investigated, per brief.

## Next
Get Workbench front screen with icon drawers visible (find where amarchy-flat is installed, or run it by full path), then repeat 3 boots. Check the reboot hang separately.
