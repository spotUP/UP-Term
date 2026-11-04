# FS-UAE upstream report draft: 3.1 palette.prefs save crashes guest (RTG switch)

Status: DRAFT, not filed. Filing is outward-facing; owner approves first.

## Summary
On FS-UAE with AmigaOS 3.1, a 68060 CPU and graphics_card = uaegfx (Picasso96), copying a palette.prefs
file to ENV:Sys/palette.prefs makes IPrefs reset the Workbench. The display switches RTG -> native -> Picasso96.
After that switch the CPU often runs from address $A6 / $7A and the guest dies.

## Steps
1. Boot AmigaOS 3.1 with uaegfx RTG Workbench, 68060.library.
2. `Copy RAM:palette.prefs ENV:Sys/palette.prefs`.
3. Watch the log during the IPrefs Workbench reset.

## Observed (log lines)
- `BEAMCON0 0000 -> 0020 PC=10031f1e` (68060.library Dispatch idle STOP), `SetSwitch() - Picasso96`
- `Illegal instruction: 0b7a at 000000A6 -> 00000000`
- then thousands of `filesys_lput/wput called PC=0000007a`, or `IDE1 DATA write without request!? 00 PC=0000007A`,
  or `CPU reset PC=f80dc8` followed by a reset loop. FS-UAE exits.

## Frequency
Bare OS, no Intuition patchers: 2 of 6 boots crash. With patchers loaded the odds rise to about 3 of 4.
The same save through the Workbench Palette prefs program on 3.2 applies cleanly.

## Evidence
thoughts/shared/research/s54-evidence/31-copy-crash-bisect.md (rounds 1-3, commits 117c7add, 766108d7).
FS-UAE version and full config: fill in from build/rig67 config before filing.
