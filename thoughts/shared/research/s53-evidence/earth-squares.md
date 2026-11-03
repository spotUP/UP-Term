---
date: 2026-11-03
session: vtcon-ee s53
ledger: UPDEMO-EARTH-SQUARES
status: root cause found (no code regression); fix not applied
---
# UPDEMO-EARTH-SQUARES: "squares instead of the texture" on rig 91

## Verdict
The coded-globe fallback ran (owner confirmed: not the NASA Earth). The "squares" ARE the coded globe:
demo/updemo.c globe_frame() paints land/sea as a checkerboard, `land = (u ^ v) & 1` (~line 1871).
No PNG decode failure, no clipping bug.

## Why kitty_earth() returned 0 (rig 91 state, measured via tools/rig/ami.py exec, UPTERM_RIG=91)
- `Assign UP-Term: EXISTS` -> `UP-Term  Rushhours:Tools/UP-Term` (the Installer's install, not VTCX:).
- `List UP-Term:share/updemo/` -> "object not found" (rc 20). share/ holds only nano, misc, man, tutorial.
- Installed tree is old: UP-Term:share/man/man7 dated 27-Oct-26, UP-Term:bin/ps 02-Nov-26; C:UPDemo absent.
- The install rule that puts the file there (dist/install.dos:585-586, `MakeDir ... UP-Term:share/updemo` +
  `Copy updemo/earth.anim UP-Term:share/updemo CLONE QUIET`, part `Lab tools`) arrived in ee1ed79a
  (2026-11-03 09:40), after rig 91's install. Rig refresh ([REFRESH] pushes handler files + VTCX: tree) does
  not re-run the kit install.
- The file itself is fine: build/dist/up-term/Files/updemo/earth.anim == build/rig91/vtc/UP-Term/share/updemo/
  earth.anim == distkit copy, 983803 bytes, md5 52d17627cba8cd8950fb8263995bdc5b, header "UPANIM1\n" count 0x2c (44),
  first PNG 160x160 RGB. So the format and the kit build are OK.
- Loader demo/updemo_amiga.c:51-80 tries only "UP-Term:share/updemo/" then "" (the shell's current dir).
  The owner ran `VTCX:UPDemo 15` from a cwd without earth.anim; the copy on VTCX: sits at
  VTCX:UP-Term/share/updemo/earth.anim, which neither path reaches. -> load() returns 0 -> fallback.
- Binary is current: build/rig91/vtc/UPDemo md5 == build/amiga/UPDemo (8ed4efc1...), contains "UP-Term:share/updemo/".
- MakeDir with two names when the first exists: checked in RAM: on rig 91: rc 10 but the second dir is still made;
  install.dos runs at FailAt 21, so that rule is not broken.

## "now" (regression feel)
Earlier passes were on a fresh full install (rig 96, build/rig96/os-32/UP-Term/share/updemo/earth.anim present).
Rig 91 is a test rig with an install older than ee1ed79a. Process gap, not a code regression.

## Incomplete frame (secondary, UNVERIFIED)
Not reproduced in budget. It is in the coded-globe path (8 frames, f=24, s=48,v=48 via kitty_chunks, 4096-byte
chunks). Candidates to check next: engine/kitty.c a=f path (kit_load/kit_frame ~1337) with eaa21b90 (K39 RGB
frames kept as RGBA canvases) and 92d580cf (K36 frame clock) drawing a frame before the canvas is whole.

## Proposed fix (not applied)
Re-run the kit install (tools step) on rig 91, or have the loader also try PROGDIR:share/updemo/ /
PROGDIR:UP-Term/share/updemo/ so a VTCX: run finds the refreshed copy; and make the rig refresh/drift check
flag a kit install older than the dist media.
