# Installer flat buttons, secondary label in the background pen: rig 94 measure

Commit 05678a64 (flat_tx_arm arms the background-pen label for every filled non-PLAIN button; regression test in
tests/test_flat_core.c: before the fix 2 failures at lines 946 and 968, after `[OK] flat_core: 2724 checks, 0 failed`;
flat-budget PASS).

## Rig 94 is KS 3.1, not 3.2

`ami.py info`: exec.library 40.10 (KS 3.1); build/rig94/vtcon-rig.fs-uae kickstart_file = kick40068-a1200.rom; the
rig was started with `--max` only (no --os 32). So this measure is NOT on AmigaOS 3.2. The file name
installer-rig94-32.png follows the brief; the numbers are 3.1. A 3.2 measure needs `rig.py start --os 32`
(the owner's 3.2 install as DH0:, no DOpus).

## Steps

`rig.py install` (new amarchy-flat, 212284 bytes, in build/rig94/vtc), `rig.py reboot --purpose "rig 94 - Amarchy
session: Installer flat button eye check"`, then `Run >NIL: Execute VTCX:dopus_run.script` (DOpus 5.104 Installer,
"Install DOpus5", first page). Screen 800x600 depth 24, tokyo-night. STATUS after: `rectfill ... bhits=4`.

## Pixels (guest grab via ami.py shot, exact; 238x16 boxes)

| button | corners (4) | edge colour (460 px) | fill | label colour | other |
|---|---|---|---|---|---|
| 90 primary | 1a1b26 | 7aa2f7 | 7aa2f7 | 1a1b26 (226 px incl. corner) | 8 AA px 3e4254 |
| 91 secondary | 1a1b26 | 00ff9f | 00ff9f | 1a1b26 (146 px incl. corner) | 8 AA px 3e4254 |
| 1 primary | 1a1b26 | 7aa2f7 | 7aa2f7 | 1a1b26 | |
| 100 secondary | 1a1b26 | 00ff9f | 00ff9f | 1a1b26 | |

No a9b1d6 pixel in any button (the old secondary label). Ring: none (edge is the fill colour). No disabled button
was visible, not measured. Pressed state not re-measured here.

PNG of the FS-UAE window: scratchpad flatbuttons/installer-rig94-32.png (1600x1256, Mac colour-managed, so exact
hex values do not occur in it; the numbers above come from the guest grab).
