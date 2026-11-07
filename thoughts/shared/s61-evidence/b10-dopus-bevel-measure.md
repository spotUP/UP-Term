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
