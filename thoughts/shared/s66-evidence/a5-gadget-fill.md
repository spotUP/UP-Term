---
date: 2026-11-09
topic: A5 Installer gadgets draw text only / unselected radios draw nothing (rig 172, KS 3.1, 640x256x8)
status: buttons = rig-setup artefact (amarchy-flat never ran); unselected MX circle = known product gap, not fixed
---

# A5 Installer gadget fill hunt (rig 172), s66

## Verdict
1. Push buttons (Proceed/Abort/Help) text only: RIG-SETUP ARTEFACT. amarchy-flat was not resident; Amarchy's
   FLAT-ABSENT-AT-BOOT start ran but the binary it starts does not exist on this --os 31 tree.
2. Unselected radio (MX) choices draw no circle: PRODUCT GAP, independent of the rig. Persists with amarchy-flat
   running. Already recorded (not fixed) in s66-evidence/a5-installer.md "Unselected MX choices draw no circle".
No code change, no commit.

## Evidence (text, rig 172, origin/main 1eb68dd6 build)
- `Status` before: wasabid, RAM:Amarchy, SolidWin, Workbench, Installer. No amarchy-flat.
- `GetEnv AMARCHY_FLAT_START` = 1: desk_start_gate -> desk_dos_flat_ensure -> dk_flat_start ran once
  (amarchy/desk_dos.c:554-561), which does `pl_start("UP-Term:bin/amarchy-flat BOOT", ...)`.
- `List UP-Term:bin/amarchy#?`: amarchy-menu, amarchy-index, amarchy-gui, Amarchy only. No amarchy-flat.
  The --os 31 tree is a clean Workbench (SYS:WBStartup empty, no amarchy line in S:Startup-Sequence/User-Startup);
  its UP-Term:bin was only partly populated, and the rig's kit shadow pass only overwrites files that already exist.
  VTC:amarchy-flat (217704, 12:31:02) exists but nothing starts it from there.
- ENV:Sys/palette.prefs (written 12:31:35 by Amarchy; ENVARC:Amarchy/desk-original/ENV-Sys-palette.prefs.absent same
  second): 4-colour pen array DETAIL 0, BLOCK 0, TEXT 2, SHINE 0, SHADOW 0, FILL 3, FILLTEXT 0, BACKGROUND 0 = flat pens
  (SHINE = SHADOW = BACKGROUND). ENV:Amarchy/prefs holds only its comment line (flat switches at their defaults = on).
  => flat pens on the screen, no flat program to redraw the bevels: the FLAT-ABSENT signature (s61 rig 93/94).
- Pixels before (crop of window 72,100 496x132, run-length): Proceed/Abort rows 194-209 = body 1a1b26 + label a9b1d6
  only; radio row 109: `1a1b26*153 7aa2f7*8` at the selected choice, unselected rows nothing but body.
- Started by hand: `Run >NIL: VTC:amarchy-flat BOOT`. STATUS: `flat=on`, `pens text=2 line=3 back=0 outline=3 button=4`,
  `rectfill ... bhits=0`, `frame=hooked calls=0`.
- Depth-cycling the window redrew nothing (backing store), so a real redraw was forced through Help (not Proceed):
  Help page buttons 98/99 (disabled) + 91 drew rounded 7aa2f7 fills with ghost dots, corner AA 31354d/5570aa;
  back on the start-mode page the redrawn Help button row 214: `31354d*1 5570aa*1 7aa2f7*232 5570aa*1 31354d*1`.
  STATUS after: `bhits=10`, `frame=hooked calls=48 flat=12`. Proceed/Abort were not redrawn by Installer on the way
  back (rows unchanged), so they still show the pre-flat draw until the next page.
- Radio choice clicked 2 -> 3 (fresh MX draw with flat running): selected `7aa2f7*6/8` fill, unselected 2 and 4 rows
  still body only. Restored to choice 2 afterwards.
- Code: no MX/mxclass/radio path in amarchy/flat*.c (git grep -i "mximage|radio" origin/main -- amarchy/flat*: none).
  The MX image draws its ring in SHINE/SHADOW, both = BACKGROUND under flat pens, so it vanishes on any rig.

## Not established
- The brief's "rig 168: unselected radio rings visible" is not in the rig 168/169 evidence; a5-installer.md
  (rig 169, 24-bit) records the same missing circle. Unverified that any rig ever showed the ring under flat pens.

## Product notes (for owner decision, not fixed)
- MX circle: needs a flat draw for the MX image (new flat path, like the border-button fill). Design work, not a
  one-line fix.
- Robustness: desk_dos_flat_ensure writes/keeps flat pens even when pl_start of UP-Term:bin/amarchy-flat fails
  (missing file). A real kit always installs it, but the invariant "flat switch on means amarchy-flat resident" is
  violated silently. Candidate: check pl_start's result / FindSemaphore after the start and fall back to stock pens.

## Rig 172 state left
Running, Installer on the start-mode page (choice 2 selected, as before), amarchy-flat now resident (started by hand
from VTC:, same 1eb68dd6 build). Proceed/Abort still show the pre-flat draw until the page changes. All screenshots
deleted.

## Correct setup for the eye check
Copy VTC:amarchy-flat to UP-Term:bin before starting Amarchy (or `Run >NIL: VTC:amarchy-flat BOOT` before the Installer
opens), then open the Installer.
