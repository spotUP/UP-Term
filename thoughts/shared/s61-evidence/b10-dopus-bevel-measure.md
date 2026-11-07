# B10: DOpus own-screen bevel measure (measure only)

Rig 94, window purpose "up-term-49: B10 DOpus own-screen bevel measure". origin/main cdf3ad0e build, amarchy-flat on (`flat=on`),
Directory Opus 5.104 (dopus5.library 73.8, STATUS `sums=780fd46b,ba2cf172`). Probe: a throwaway C program (not committed; reads
`IntuitionBase->FirstScreen` chain, `GetScreenDrawInfo`, `dri_Pens[SHINEPEN/BACKGROUNDPEN]`, `GetRGB32` on the screen's ColorMap).
No amarchy source changed.

## Mode 2: explicit (non-clone) screen mode (MEASURED)

Environment/workbench screen_mode = ModeID 50F91303 (1344x1080, depth 8), via `rig.py dopus --own-screen`.
Theme on the rig at measure time: tokyo-night (left from the earlier run in this rig folder).

| screen | SHINEPEN | BACKGROUNDPEN | SHINE rgb | BACK rgb | SHADOW | FILL | TEXT |
|---|---|---|---|---|---|---|---|
| DOpus own screen | 0 | 0 | 1A1B26 | 1A1B26 | 0 | 3 | 2 |
| Workbench Screen (800x600 d4) | 0 | 0 | 1A1B26 | 1A1B26 | 0 | 3 | 2 |

`amarchy-flat STATUS` dopus5 field (`dopus=73.8 patched=3 calls=C flat=F`), before / after lister 1 / after lister 2
(`DeviceList NEW` through ARexx DOPUS.1):

- calls=0 flat=0 -> calls=8 flat=8 -> calls=16 flat=16 (global `flat=on calls=21 -> 101 -> 179`).

Every dopus5 DrawBox call is flat-skipped: SHINEPEN == BACKGROUNDPEN as numbers, so the pen test passes.
Matches the B8b finding (calls and flat grow together).

## Mode 1: clone (MODE_WORKBENCHCLONE) -- NOT MEASURED

Needs screen_mode byte 23 of `DOpus5/Environment/workbench` set to 2 (file offset 20..23 is the ULONG screen_mode;
shipped = 1 = USE, own-screen = 50F91303). The edit was refused by the auto-mode classifier because the running rig 94 mounts
`/Users/spot/Code/UP-Term/vtcon/build/rig94/vtc` (the main checkout's folder; the worktree's seeded copy of build/rig94 kept the main
paths in vtcon-rig.fs-uae), and the task said not to touch the main checkout. Not attempted by another route.

Expected from the source (display.c:143-175, not measured): the clone screen copies the Workbench `dri_Pens` into the new screen,
so on this rig it would read the same pens as the Workbench row above (0/0). Unverified.

## What the numbers support

- Explicit mode here: pen numbers equal (0/0), RGB equal (1A1B26). Both option (a) and (b) give SKIP; the numbers cannot separate
  them. Caveat: the equality comes from the Amarchy theme (pen 0 for both) on the Workbench and own screen alike; a stock
  Workbench (SHINE 2, BACK 0, from the B8b notes) was not read in this run, so no run here shows a screen where the pen test says
  STOCK and RGB says SKIP or the reverse.
- The decision between (a) and (b) is still open for clone mode; a clone read is the missing datum.

## Left over

Rig 94 stopped at end (see commit message of the reply). Env file of rig 94 untouched in this run (still the explicit 50F91303 bytes).

## Part 2 (clone mode and stock look): NOT MEASURED, blocked on the DOpus install

Rig 94, purpose "up-term-49: B10 part 2 DOpus clone-mode bevel measure", main checkout df96b38e. No number below was read.

`rig.py dopus --own-screen` (part 1's rig.py stop removed build/rig94, so the install had to run again) fails on the sys.hdf
clone, three times (twice via rig.py, once by driving dopus_rig.drive by hand): the Installer reaches the
"How should Directory Opus 5 start?" page (85%), the answer "Instead of Workbench" (gadget 4) leads to the page
"Modify Startup-Sequence File" (88%) showing the clone's own startup sequence (BOOTX lines: `Assign >NIL: VTC: VTCX:`,
`Echo >BOOTX boot log`, `C:Run >NIL: C:Execute BOOTX:go`, `EndCLI >NIL: BOOTX:s/startup-sequence`), the driver clicks
gadget 90 ("Proceed with Changes"), and the Installer shows "Sorry... an Error Has Occured! STARTUP: Problem with file
Startup-Sequence in line 540. DOS Error Type: Cause of disk-related error unknown." and exits. Result: no `;BEGIN DOpus5` block in
S:User-Startup, ENVARC:DOpus5 empty, no dopus5.library, `rig.py dopus` aborts with "no ';BEGIN DOpus5' block".
Not caused by this session's edits (no source changed). Part 1 passed this step only because an earlier install already stood in
build/rig94/vtc; a fresh install on the current clone image does not work (rig tooling regression, probably the clone's
startup-sequence changed since the 2026-10-21 DOpus install code; unverified which commit).
The `--os32` route is also closed: the 3.2 tree `/Users/spot/Downloads/AmigaOS-3.2-full (1)` is not on this machine
(UPTERM_OS32_TREE unset).

Not attempted: hand-editing the Startup-Sequence page (Skip This Part) or the sys.hdf, since the task limits edits to the
workbench environment file. Consequently the screen_mode byte 23 edit, the tokyo-night clone read, the stock-look reads and the
calls/flat deltas were not done.

What the source alone says (display.c:143-175 and flat_core.c:193-201, no measurement): a clone copies the Workbench dri_Pens,
so clone mode under tokyo-night would read 0/0 like part 1's Workbench row (a and b both SKIP), and under a stock look the clone
would read Workbench's SHINEPEN 2 vs BACKGROUNDPEN 0 (pen test STOCK, i.e. DrawBox is not skipped, (a) compares RGB of two different
colours and also does not skip; (b) skips on the clone anyway because it is not the Workbench screen, hence the clone case is where
(a) and (b) differ). Unverified.

Needed to continue: either a fixed `rig.py dopus` for the current clone image (the Startup-Sequence modify step), or a
rig folder that still holds a good DOpus install (stamp + dopus5-user-startup.txt) to copy into build/rig94/vtc.
Rig 94 stopped, build/rig94 removed.
