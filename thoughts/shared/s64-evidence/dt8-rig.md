# DT-8 eye check rig (s64), rig 63, PAL 16t

State: rig 63 RUNNING (started --idle-minutes 0: stop it after the owner answers: `UPTERM_RIG=63 python3 tools/rig/rig.py stop`).
Window title: "Amarchy session s64: DT-8 DOpus toolbar eye check (PAL 16t)".
Build: commit 4acb7752 toolbar (make dopus-toolbar: "22 images x 6 sets", log in scratchpad dt8-make.log).

## Commands (all UPTERM_RIG=63)
1. `make dopus-toolbar`
2. `rig.py stop` (a prior rig 63 was gone), `rig.py dopus` (installs DOpus 5.104 into the os-32 tree; needs the rig stopped, ~4 min)
3. `rig.py start --os 32 --max --idle-minutes 0 --purpose "Amarchy session s64: DT-8 DOpus toolbar eye check (PAL 16t)"`
   (--os 32 uses the existing Clean Workbench Installs image; ~/Downloads/AmigaOS-3.2-full (1) not needed)
4. `rig.py install`, `rig.py aga`, `rig.py reboot` -> `dopus screen` = "Workbench 640 256 4" (PAL hires, DOpus on Workbench, no DOPUS.1)
5. Scratch driver (scratchpad dt8_setup.py, imports amarchy_dopus_toolbar_rig): install_sets() (6 sets + 6 banks), Run VTC:Amarchy,
   `command DeviceList NEW`, d.pick() until theme = vantablack (it was tokyo-night; one pick Down).
6. Product apply did NOT switch the listers: both stayed PROGDIR:Buttons/Toolbar after the pick (dopus_toolbar_flat sets then
   re-queries and restores the old path when the query differs; reason not investigated). Set by hand, as in B7:
   `lister set <h> toolbar 'DOpus5:Buttons/Amarchy-toolbar-16t'` -> query returns it. Second lister closed (`lister close <h> force`),
   remaining lister `lister set <h> path 'RAM:'`, `dopus front`.
7. `Run >NIL: VTC:amarchy-flat` by hand (rig install leaves flat absent).

## Verified by text
- 22 of 22 brushes in DOpus5:Images/Amarchy/16t have the same md5 as build/dopus-toolbar/Images/16t/* (fetched with ami.py get, compared).
- Lister 271814436: path RAM:, toolbar DOpus5:Buttons/Amarchy-toolbar-16t (one lister open).
- Theme vantablack (ENVARC Amarchy prefs amarchy-gui-theme = vantablack).
- `RAM:amarchy-flat STATUS` (RAM copy of build/amiga/amarchy-flat): `flat=on calls=37`; process "amarchy-flat" running. Note the
  Amarchy ARexx STATUS has no flat= word; use amarchy-flat STATUS.
- Not looked at on screen (no screenshots taken).

## Found
- Working tree has uncommitted DT-9 edits (amarchy/dopustb_core.*, dopustb_dos.c, menu_theme.*, tests, dopustb_expect.c: dt_pick_set got an
  `outline` argument). build/amiga/Amarchy on the rig is from 21:18, built before/during those edits; the theme-apply toolbar switch not
  happening may be that mismatch or a regression. Unresolved; the eye check does not depend on it.
- make dopus-toolbar is current; rig.py install printed "[BUILD] stale before push" for many programs (only the kit install, no effect on this check).

## RTG 800x600 half (NOT done)
- Set: DT-6 recorded RTG 800x600 24-bit, pixel 1:1, topaz 8 -> dt_pick_set gives set 16 (square), bank Amarchy-toolbar-16, brushes Images/Amarchy/16.
  Confirm with `dopustb_expect <theme> 8 1 1` (tools/rig/dopustb_expect.c).
- rig.py option: `rig.py rtg` (screen mode from next boot; the default for a non-os rig, an --os rig defaults to aga256), then `rig.py reboot`.
  Same rig, same banks on disk (no reinstall of sets needed), but the reboot drops amarchy-flat and the lister: repeat steps 5-7 with
  Amarchy-toolbar-16 and `dopus screen` = 800 600. PAL half must be answered first (rig.py aga/rtg are mode switches on the same rig).
