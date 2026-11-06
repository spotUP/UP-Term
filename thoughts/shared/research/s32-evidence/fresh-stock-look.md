---
date: 2026-11-03
author: vtcon-16 opus hunt agent
topic: RIG96-FRESH-STOCK-LOOK - fresh Amarchy full install boots with the stock look
status: root cause found (rig tooling), product fix not needed for the reported symptoms
---

# Fresh install "on 3.2" boots with the stock look

## Verdict (one cause for A and B)

The rig was never AmigaOS 3.2. `installer_rig.py` restarts the rig from a fresh clone with only
`UPTERM_RIG_FLAGS` (tools/rig/installer_rig.py:1607-1610 `rig_start()` -> tools/rig/assign_rig.py:86
`paths.rig_flags()`). Without that env var the `--os 32` of the original `rig.py start` is dropped: the
rig comes back on the default 3.1 sys.hdf with the A1200 3.1 ROM. `parse_args` (installer_rig.py:1613)
accepts `--max` but has no `--os`, so it cannot be passed on the command line either.

On 3.1 Amarchy deliberately gates the palette, Workbench border, pointer and IControl parts
(desk_dos.c:201-208 `dk_palette_editor_ok`: Palette editor must be v47+, DESK-31-CRASH: the 3.1 palette
write freezes the guest; desk_dos.c:757 Workbench editor with WBNC; desk_dos.c:194 Pointer v47+). So the
theme colours never reach the palette; the backdrop pattern and flat frames are drawn with the theme's pens,
but those pens still hold the stock 3.1 colours.

## Evidence (rig 47, my own reproduction, same steps as vtcon-ee on rig 96)

- `UPTERM_RIG=47 rig.py start --max --os 32 ...`, then `UPTERM_RIG=47 INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all` -> [PASS].
- After it: `Version` -> `Kickstart 40.68, Workbench 40.42`; `Version SYS:Prefs/Palette` -> `palette 40.2`;
  `SYS:Prefs/Workbench` absent. The build/rig47/os-32 tree on the host holds `$VER: palette 47.21` but is not mounted.
- build/rig47/vtcon-rig.fs-uae: `hard_drive_0 = build/rig47/sys.hdf`, `kickstart_file = roms/kick40068-a1200.rom`;
  build/rig47/start.args = `[]` (written 21:17, the installer's restart; sys.hdf re-cloned at that time).
- build/rig96 (host files only, rig not touched): identical - `kickstart_file = .../kick40068-a1200.rom`,
  `hard_drive_0 = build/rig96/sys.hdf`, start.args `[]`. Rig 96 is a 3.1 machine too.
- Screenshot ~/Desktop/upterm-shots/rig47-stocklook-1.png (800x600): EE9900 337815 px, 000000 111543,
  FFFFFF 11170, 6688BB 11068, AAAAAA 5146 - EE9900 count identical to rig 96's 337815, so the same state.
- Start apply ran: `GetEnv AMARCHY_DESK_START_PLAN` = 18 (DESK_START_PATTERN 2 + DESK_START_FONT 16, desk_core.h:314-329),
  `AMARCHY_DESK_START_WRITES` = 2. No PALETTE (4), WB (1), POINTER (8), ICONTROL (32): exactly the 3.1 gates.
- `List ENV:Sys`: WBPattern.prefs 410 and font.prefs written; no palette.prefs, Workbench.prefs, pointer.prefs, icontrol.prefs.
- amarchy-flat STATUS: `flat=on calls=82 chained=0`, `pens text=2 line=3 back=0 outline=3 button=4 ptext=0`
  (line/outline = pen 3 = stock 6688BB, the "frame colour" vtcon-ee measured), `rectfill installed=yes calls=1029 hits=32`,
  `frame=hooked calls=0 flat=0`, `pens-held 4 missing 12 13 14 15 refs=3 dim=14 knob=6 inbar=3`.
- amarchy-flat autostarted (Status: process 6 `UP-Term:bin/amarchy-flat`), as did Amarchy, amarchy-icons, amarchy-gui,
  amarchy-menu. S32-FLAT-NOAUTOSTART not reproduced.

## The two STATUS puzzles

- "missing 12 13 14 15" is not the flat state: it is the icon pen holder line `ip_status_line`
  (amarchy/iconpen_core.c:170-189, "pens-held N missing ..."), printed by amarchy-icons STATUS and repeated in
  amarchy-flat's STATUS: pens 12-15 were not obtained (held by another user / not on this screen).
- "flat=0 calls=0" is the frame-hook sentinel line `frame=hooked calls=%lu flat=%lu` (amarchy/flat_core.c:239),
  not `flat=` of the main line (flat_core.c:46). It reads 0 on this 3.1 machine; not the cause of the colours.

## Ledger row AMARCHY-START-APPLY is stale

The start path does apply: amarchy.c:627 `desk_start_gate(0)` -> amarchy.c:293 `desk_dos_start_apply()`
(desk_dos.c:1392, commits d94a609c / 61760273, gate 31ce61a5, all ancestors of fb6aa600). The sentinel PLAN=18
proves it ran on boot. Refuted as the cause of (A); close/rewrite the row after a real 3.2 check.

## Proposed fix (tooling, not product)

1. tools/rig/installer_rig.py `rig_start()` (1607): start with the running rig's own machine flags when
   UPTERM_RIG_FLAGS is unset - read `<rig>/start.args` (rig.py:100/2163, the same source `rig.py reboot_args`
   uses, rig.py:1923) before the first restart, keep `--os/--os32/--kick/--a500/--fast`; and let `parse_args`
   accept `--os <name>` into RIG_FLAGS. Reuse rig.py's reboot_args, do not re-parse.
2. Guard: `reboot_amarchy_check` (installer_rig.py:1077) logs `Version` and fails when `--os 32*` was asked
   but Kickstart < 47.
3. Host test (tests/ python unit for installer_rig): with start.args `["--max","--os","32"]` and no
   UPTERM_RIG_FLAGS, `rig_start` must call rig.py with `--os 32`; fails on revert (today it passes `start` only).

Workaround until then: `UPTERM_RIG=96 UPTERM_RIG_FLAGS="--os 32 --max" INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all`.

## Open (unverified)

- The look on a real 3.2 fresh install is not measured here (budget). Next step: rerun with UPTERM_RIG_FLAGS="--os 32 --max"
  and pixel-check backdrop 0E0E14 and flat frames.
- Product gap on 3.1 (not the reported bug): with the palette gated, Amarchy writes a theme wbpattern and flat pens that
  render in stock colours (orange EE9900 backdrop). Whether 3.1 should skip the pattern or poke live colours is an owner call.
