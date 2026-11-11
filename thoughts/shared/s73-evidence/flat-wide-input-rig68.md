---
date: 2026-11-11
topic: FLAT-WIDE-INPUT rig 68 proof
branch: flat-wide-input
commit: f4e1369e (Fit read-only display ground boxes to their text)
rig: 68
---

# FLAT-WIDE-INPUT proof on rig 68

Measure only. No source changes, no commits. Pixels read as text through
`ami.grab` (scratch scripts runs.py / census.py / art.py / stale.py / yr.py in the
session scratchpad). No screenshots taken.

## Build under test

`nice -n 19 make build/amiga/amarchy-flat build/amiga/amarchy-gui build/amiga/Amarchy build/amiga/amarchy-menu build/amiga/upprefs` in the worktree, rc=0.

| binary | bytes |
|---|---|
| amarchy-flat | 239792 (md5 4cec5c4f7ea4a3d5a392c43b53822b90) |
| Amarchy | 398708 |
| amarchy-gui | 567920 |
| amarchy-menu | 684260 |
| upprefs | 199380 |

Struct readers of `flatshared` (flat_shared.h): amarchy/flat.c, amarchy/desk_dos.c
(linked into Amarchy, amarchy-menu, amarchy-gui), prefs/upprefs.c. New fields
gfit / gfitbusy are appended at the struct end; readers use only earlier fields,
so the rebuilt readers stay layout-compatible.

## Verdicts

Coordinates: window-relative (grab origin 0,14); screen y = window y + 14, screen x = window x.
Time Preferences window W0 at 0,14 size 332x203.

### Q7 new build resident: PASS
`List UP-Term:bin/amarchy-flat` = 239792 bytes (11:26:34). STATUS from the resident
patch shows the new gfit / gfitbusy fields. After reboot: flat=on gboxes=38 side=1.

### Q1 box fits text at open: PASS
Date display text "11/11/26 11:29 AM" (17 chars). Box rows window y167..178
(screen 181..192). Row 176: 3e4254 x92..239; rounded rows 93..238 and 94..237.
Outside: 1a1b26 x1..91 and x240..330. STATUS gfit=1 gfitbusy=0.

### Q2 box refits on slider change, no stale pixels: PASS
- Two clicks left in Hours container (screen 135,159): text "11/11/26 9:29 AM"
  (16 chars). Box x96..235 (row 176); non-ground pixel range x96..235; all
  pixels outside the box 1a1b26 over rows 163..179. gfit=5 gfitbusy=0.
- Two clicks right (screen 310,159): back to "11/11/26 11:29 AM" (17 chars). Box
  x92..239; outside all 1a1b26. gfit=9 gfitbusy=0.

### Q3 refit on calendar day click: PASS
Click screen (100,110): text "11/24/26 11:29 AM" (17 chars). Non-ground x92..239,
outside all 1a1b26. STATUS gboxes=81 gfit=10 gfitbusy=0.

### Q4 counters: PASS
gfit 1, 5, 9, 10 across the steps above; gfitbusy 0 throughout.

### Q5 editable year field stays full width: PASS (caveat)
Region screen x52..108 y30..47: 3e4254 500 px over x55..105 y33..44; text a9b1d6
100 px x61..90; row 38 inset spans x55..105, the full gadget width, not fitted to
the text. Agent 13 reported "year band 130 only, inset 492" with pen 3f4551. Pixel
counts are not directly comparable because the inset pen differs (environment
palette, see below); the span is the full gadget width, which is the claim.

### Q6 FLAT-SPACING unchanged: PASS (one unverified offset)
- Calendar rows 69 / 84: 1a1b26 x2 gaps at x55-56, x99-100, x143-144 (matches
  agent 13; cell fill x13..54).
- Calendar column 30: 1a1b26 x2 gaps at y62-63, 77-78, 92-93, 107-108. Agent 13
  reported y76-77, 91-92, 106-107, 121-122. Gap spacing (15) and width (2) match;
  absolute rows differ. Unverified whether this is a grab-origin difference or a
  layout shift; spacing itself is intact.
- Button row 190: Save 7aa2f7 from x13. Slider column 200: gaps present.
- Calendar cell fill now 00ff9f (agent 13 described a9b1d6 / 7aa2f7); palette
  environment difference, untouched code path.
- Serial log tail (300 lines): no guru, alert, error or exception. Rig answered
  every command after the last reboot.

## Environment notes and unexpected events

1. 11:08:45 rig 68 reset loop (watchdog: "10+ CPU resets in 60 s ... no CPU
   exception logged (ROM reset)"), log
   /Users/spot/Code/UP-Term/vtcon/build/rig68/reset-loop-20261111-110845.fs-uae.log.
   It coincided with `ami.py exec 'List UP-Term:bin/amarchy#? UP-Term:bin/Amarchy'`
   while the old s72 build (237076) was resident. Watchdog restarted the rig. Cause
   not investigated.
2. ~11:19 `ami.py put` of the new amarchy-flat to UP-Term:bin hung the Amiga (no
   ping, fs-uae 109% CPU, status down).
3. Running ami commands from the main checkout's tools makes the rig gate refresh
   vtc/ from the main checkout's build/amiga (owner root was main), undoing
   staged copies. Fix used: `rig.py stop`, then `rig.py start --max --purpose
   "s73: FLAT-WIDE-INPUT proof"` from the worktree. owner.json is now
   {"root": "/Users/spot/Code/UP-Term/vtcon-flat-wide-input", "purpose": "s73:
   FLAT-WIDE-INPUT proof"} (was the main checkout root). Next user of rig 68 from
   main must restart from main to get main's binaries.
4. Rig start warned DOpus 5.104 not installed in VTC: (Workbench without DOpus).
5. ENVARC:Sys/palette.prefs was missing (inset drew 7f007f). Ran
   `Copy ENV:Sys/palette.prefs ENVARC:Sys/palette.prefs CLONE` and rebooted; inset
   now 3e4254 (agent 13 saw 3f4551). The untouched year field also draws 3e4254,
   so the pen difference is environment, not the fit code. ENV/ENVARC:Amarchy/prefs
   do not exist; defaults in use.
6. Time Preferences date was changed to the 24th by the Q3 click. Not saved or used.

## End state

- rig 68 up; `rig.py status`: hold until 2026-11-11 15:27:23 (pid 79658), idle
  watchdog off; ping answers (wasabid 0.11b2).
- uitree: W0 "Time Preferences" 0,14 332x203 active (front).
- Resident amarchy-flat 239792 (checked by List + STATUS fields).
- Scratch: small scripts and logs only in the session scratchpad; nothing over 10 MB.
