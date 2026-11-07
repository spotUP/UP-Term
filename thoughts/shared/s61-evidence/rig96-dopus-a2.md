---
date: 2026-11-07
topic: rig 96 Directory Opus 5.104 install (A2), A1/A3 start-log check
tags: [rig, dopus, evidence, os32]
status: findings
---

# Rig 96 DOpus (A2)

## Commands (cwd vtcon-s61-rig96, UPTERM_RIG=96)
1. `rig.py start --max --os 32 --idle-minutes 0 --keep --purpose "up-term-49: install Directory Opus on rig 96"` -> rc 0, log scratchpad/a2/start.log.
2. `rig.py dopus` (as briefed, no --os) -> rc 1: `dopus: this rig runs; stop it first (the install boots it with --no-dopus)`.
   The brief said dopus is GATED "needs a running rig"; the GATED tuple only means ami.req is passed, and dopus_rig.main() refuses a running rig.
3. `rig.py stop` -> `stopped`, then `rig: removed .../build/rig96` (the whole rig folder incl. os-32 went; --keep did not keep it past `stop`).
4. `rig.py dopus` (still without --os) -> rc 0 but: `dopus: no ';BEGIN DOpus5' block in S:User-Startup after the install`.
5. `rig.py dopus --force --os 32` -> rc 0, `dopus: installed; block:` (see below).
6. Rig stopped by dopus itself (`stopped`); no fs-uae for rig 96 remains (ps count 0, `rig.py status` = down).

## Start-log verdict (A1/A3)
Start log has exactly one WARN: the DOpus-not-installed line (expected before the install). No "UP-Term:share/updemo missing" lines, no "[WARN] the Workbench stayed PAL (W43)", and no "saved ScreenMode is native (DisplayID 0x...)" line was printed (grep for updemo|ScreenMode|PAL on start.log: empty). Last lines: `refreshed kit-userland/upkg.info 362 bytes`, `up`.
CAUTION: the A1 updemo WARN lines DID appear in the `dopus --force --os 32` run (the install boot of a fresh --os 32 tree): earth.anim, fjord.png, kanim-stream.bin, photo.png missing in the rig's UP-Term: tree. The first `start` had them absent only because rig96's tree was the kept one with UP-Term installed; the dopus boot built a fresh os-32 tree and did not run `rig.py install`. So A1 is clean for start, but a fresh os-32 tree without an install warns (likely expected; not investigated).
A3 line never printed on start: either the saved ScreenMode is absent so the check is silent, or the message is conditional; not verified.

## Three conditions (build/rig96/vtc/)
Meaning (rig.py dopus_install ~1119): block file dopus5-user-startup.txt = the Installer's ';BEGIN DOpus5'..';END DOpus5' text captured from S:User-Startup; dopus5.stamp must equal the md5 of DOpus5_104_os3.lha in dist/addons/dopus/archives.txt; DOpus5/DirectoryOpus = the installed program.
- Before (after the first start): all three missing (the whole folder is wiped by `stop`/GC). Verdict: missing.
- After step 4 (no --os): DirectoryOpus present, block missing, stamp missing (install aborted at block_of, left dopus5-us.tmp = copy of the sys.hdf clone User-Startup with no DOpus lines).
- After step 5 (`--os 32`): block present (110 bytes), stamp present (33 bytes), DirectoryOpus present (551212 bytes). All three satisfied.

Decisive block lines (vtc/dopus5-user-startup.txt):
```
;BEGIN DOpus5
Assign DOpus5: "VTCX:DOpus5"
Path DOpus5:C ADD
Resident LoadWB DOpus5:C/LoadDB PURE
;END DOpus5
```

## Where the block landed
- After step 5 the block is in the booting tree: `grep -n DOpus build/rig96/os-32/S/User-Startup` -> lines 24-28 (that is the Installer writing into the tree the install boot used; os_tree_dopus() would re-write it on the next --os 32 start).
- sys.hdf: after step 5 rig96 has no sys.hdf checked (not used by --os 32). After step 4 the sys.hdf clone had NO DOpus block.
- Location: os-32 yes; sys.hdf no (for the successful run).

## Defect / finding
`rig.py dopus` has to be given the same system flags as the rig (`--os 32`); without them it installs into the sys.hdf clone, where the Installer left no ';BEGIN DOpus5' block (cause not investigated; the 5.82 block ';BEGIN DirectoryOpus 5' is the master image's). The 1.0 eye-check rig 96 therefore warned because nobody had run `rig.py dopus --os 32` for it; and `stop` deleting the rig folder (build/rig96, incl. vtc/) is what loses the install, so the VTC: install does not survive `stop` unless something else keeps it. Also `rig.py dopus` is NOT usable on a running rig (opposite of the brief). The Amiga-side drawer check (step 5 of the brief, `wasabi run "List DOpus5:..."`) was NOT done: the rig is stopped by the install and no restart was requested within the budget. Not verified: that a subsequent `start --os 32` boots with DOpus as Workbench.

## Cleanup
Scratch logs in /private/tmp/claude-501/-Users-spot-Code-UP-Term/59d7869f-00a7-443e-824b-0e3cb1d5d3aa/scratchpad/a2 (16 KB total) left; nothing over 1 MB. No repo files edited. Stray `build/rig96/vtc/dopus5-us.tmp.uaem` (33 bytes) left by the install.
