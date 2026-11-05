---
date: 2026-11-05
author: agent vtcon-c1
row: S57-ICL-PINK-SHADE rig proof (Workbench screen)
build: main c990849e kit (make dist from kit-main), rig 145 fresh-all, installed image reused for boots 1-2
verdict: PASS on the measured build (0 pink, 0 red in 3 boots with 1381-3294 label-shade pixels on screen); caveat: one Tokyo-theme pink decoration seen once, see below
---

# Pink/red icon-label proof, Workbench in front (rig 145)

Rig: `--max --os 32 --idle-minutes 0`, purpose "vtcon-c1: pink icon-label pen proof". Fresh-all Installer, every option ticked, INSTALLER_KEEP=1, [PASS]. Boots 1 and 2: `rig.py stop --keep` then `rig.py start` (needed `--allow-stale-base`: origin/main had moved; the rig image is the installed one, tree irrelevant). No ami.py reboot used. No hang on any boot (ping within 180 s every time).

## Screen state per shot
Before each count: `ami.py uitree` shows the Workbench screen ("Amiga Workbench Release 3.2" title, boot 2 title was the Hyperion copyright line, root window "Workbench" present) with windows Utilities, Tools, System, Prefs, UP-Term, volume SYS:, opened via ARexx `WINDOW <drawer> OPEN` (ADDRESS WORKBENCH), shot taken 14 s after the last open. Boot 2 also had an "Output window" 640x141 open (not closed).

## Counts (python, whole 640x256 screen; PNGs ~/Desktop/upterm-shots/s57-pinkwb-rig145-boot{0,1,2}.png)
Classes: exact (247,118,142); pink family r>200, 80<g<170, 90<b<200, r-g>60; red family r>150, g<110, b<110; label shade = a colour on the bg (26,27,38) to text (169,177,214) blend line (tolerance 14), excluding the palette endpoints. The theme (Tokyo Night) is blue-grey, so the shades are (62,66,84), (76,79,98), (83,87,108), (116,122,148), not neutral grey; a neutral-grey test counts 0 and is wrong here.

| boot | pink exact | pink family | red family | label shade | Workbench front | hang |
|------|-----|-----|-----|------|-----|------|
| 0 post-install | 0 | 0 | 0 | 1361 | yes | no |
| 1 stop+start | 0 | 0 | 0 | 1381 | yes | no |
| 2 stop+start | 0 | 0 | 0 | 3294 | yes (Output window also open) | no |

Top colours boot 0: (26,27,38) 108771, (90,94,117) 21108, (14,14,20) 20995, (169,177,214) 7730, (122,162,247) 3511, (62,66,84) 455, (83,87,108) 421, (116,122,148) 414.
Boot 1: (26,27,38) 99914, (14,14,20) 31690, (90,94,117) 20199, (169,177,214) 7161, (122,162,247) 3495, then the same three shades.
Boot 2: (26,27,38) 106846, (90,94,117) 27380, (14,14,20) 15638, (169,177,214) 7123, (122,162,247) 3559, (76,79,98) 1931, (62,66,84) 455, (83,87,108) 421.
No rare colours (count <3000, not on the shade line) in boots 1 and 2; boot 0 had 3 theme accents ((187,154,247), (173,142,230), (68,157,171)).

## amarchy-flat STATUS (UP-Term:bin/amarchy-flat STATUS exists and works with the full path)
boot 0: `flat=on calls=773`, pens text=2 line=3 back=0 outline=3 button=4, pens-held 15 missing 47.
boot 1: `flat=on calls=537`, pens-held 16 refs=2.
boot 2: `flat=on calls=1014`, pens-held 16 refs=2.

## Finding: pink pixels in a first shot with no labels
First shot of boot 0 (only SYS: and UP-Term: windows open, no icons visible, shade count 0, so INVALID as a pass) had 119 exact (247,118,142) pixels at x 198-458, y 31-38, flat (neighbours only bg and (122,162,247)): a row of slanted strokes with a bar, a theme decoration in the root window, not label text and not a shade. It is covered by the drawer windows in the valid shots, so it is not counted there. (247,118,142) is Tokyo Night red f7768e, a legitimate theme colour.

## Limits
- The PNGs were not looked at by eye; the proof is pixel counts plus the valid-shade gate.
- Boots 1 and 2 reused the installed image (no second fresh install).
- The pre-fix state was not re-measured on this rig, so this shows the pink is absent on main c990849e, not that the fix removed it.
